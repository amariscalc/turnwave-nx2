#include "player.hpp"
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <sys/stat.h>

namespace fs=std::filesystem;

const Entry* Player::highlightedTrack() const {
    const auto& entries=view==View::Files?browserEntries():queue;
    const int index=view==View::Files?fileRow:selected;
    if(index<0 || index>=int(entries.size()) || !entries[index].track()) return nullptr;
    return &entries[index];
}

bool Player::isFavorite(const Entry& track) const {
    return std::any_of(favorites.begin(),favorites.end(),
                       [&](const Entry& favorite){return favorite.path==track.path;});
}

void Player::loadExtras(const std::string& directory) {
    settingsDir=directory;
    auto path=fs::path(directory)/"library-settings.txt";
    std::ifstream input(path);
    if(!input) input.open(path.string()+".bak");
    std::string header;
    if(!std::getline(input,header) || header!="Turnwave library 1") return;
    if(!(input>>hasCustom>>customPreamp)) {hasCustom=false;return;}
    for(auto& gain:customGain) {
        if(!(input>>gain)) {hasCustom=false;return;}
        gain=std::clamp(gain,-12,12);
    }
    customPreamp=std::clamp(customPreamp,-12,12);
    std::string relative;
    while(input>>std::quoted(relative)) {
        auto entry=resolveAudioEntry(root,relative);
        if(entry && !isFavorite(*entry)) favorites.push_back(std::move(*entry));
    }
}

void Player::saveExtras() const {
    if(settingsDir.empty()) return;
    if(mkdir(settingsDir.c_str(),0777)!=0 && errno!=EEXIST)
        throw std::runtime_error("Cannot create settings folder");
    auto path=(fs::path(settingsDir)/"library-settings.txt").string();
    auto temp=path+".part", backup=path+".bak";
    std::ofstream output(temp,std::ios::trunc);
    output<<"Turnwave library 1\n"<<hasCustom<<' '<<customPreamp;
    for(auto gain:customGain) output<<' '<<gain;
    output<<'\n';
    for(const auto& track:favorites)
        output<<std::quoted(fs::path(track.path).lexically_relative(root).generic_string())<<'\n';
    output.close();
    if(!output) {std::remove(temp.c_str());throw std::runtime_error("Cannot write settings");}
    if(std::rename(temp.c_str(),path.c_str())==0) {std::remove(backup.c_str());return;}
    // Some SD filesystems require the rename destination to be absent.
    if(!fs::exists(backup) && std::rename(path.c_str(),backup.c_str())==0) {
        if(std::rename(temp.c_str(),path.c_str())==0) {std::remove(backup.c_str());return;}
        std::rename(backup.c_str(),path.c_str());
    }
    std::remove(temp.c_str());
    throw std::runtime_error("Cannot save settings");
}

const char* Player::presetName(int row) {
    static const char* names[]={"Flat","Rock","Hip-Hop","Load custom","Save current as custom"};
    return names[row];
}

void Player::applyPreset(int row) {
    static constexpr std::array<int,10> gains[]={{},{4,3,-1,-2,0,2,4,4,3,2},{5,4,2,0,-2,-1,1,2,2,1}};
    if(row==4) {
        const auto oldGain=customGain;const auto oldPreamp=customPreamp;const auto oldCustom=hasCustom;
        customGain=eq.gain;customPreamp=eq.preamp;hasCustom=true;
        try {saveExtras();notice="Custom preset saved";}
        catch(const std::exception& error) {
            customGain=oldGain;customPreamp=oldPreamp;hasCustom=oldCustom;notice=error.what();
        }
    } else if(row==3 && !hasCustom) notice="No custom preset saved";
    else {
        eq.gain=row==3?customGain:gains[row];
        eq.preamp=row==3?customPreamp:row==0?0:-5;
        eq.on=row!=0;eq.rebuild();notice=presetName(row);
    }
    overlay=Overlay::None;
}
