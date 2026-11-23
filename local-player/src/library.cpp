#include "player.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <tuple>

namespace fs=std::filesystem;
namespace {
std::ifstream openFile(const fs::path& path) {
    std::ifstream input(path);
    if(!input) input.open(path.string()+".bak");
    return input;
}
template<class Write>
void saveFile(const fs::path& path,Write write) {
    auto temp=path.string()+".part", backup=path.string()+".bak";
    std::ofstream out(temp,std::ios::trunc);
    write(out);
    out.close();
    if(out) {
        if(std::rename(temp.c_str(),path.c_str())==0) {std::remove(backup.c_str());return;}
        if(!fs::exists(backup) && std::rename(path.c_str(),backup.c_str())==0) {
            if(std::rename(temp.c_str(),path.c_str())==0) {std::remove(backup.c_str());return;}
            std::rename(backup.c_str(),path.c_str());
        }
    }
    std::remove(temp.c_str());
    throw std::runtime_error("Cannot write library data");
}
void scan(const std::string& folder,std::vector<Player::LibraryTrack>& result,
          const std::map<std::string,std::time_t>& previous,bool first,int depth) {
    if(depth>32) return;
    for(auto entry:listMusic(folder)) {
        struct stat info{};
        if(lstat(entry.path.c_str(),&info)!=0) continue;
        if(entry.directory && S_ISDIR(info.st_mode)) scan(entry.path,result,previous,first,depth+1);
        else if(entry.track() && S_ISREG(info.st_mode)) {
            Player::LibraryTrack track;
            auto found=previous.find(entry.path);
            track.added=found!=previous.end()?found->second:first?info.st_mtime:std::time(nullptr);
            track.entry=std::move(entry);
            try {
                Decoder decoder;
                decoder.open(track.entry.path);
                track.title=decoder.title;track.artist=decoder.artist;track.album=decoder.album;
                track.genre=decoder.genre;track.disc=decoder.discNumber;track.number=decoder.trackNumber;

            } catch(const std::runtime_error&) { /* Keep the filename searchable. */ }
            result.push_back(std::move(track));
        }
    }
}
std::string groupName(const Player::LibraryTrack& track,int mode) {
    if(mode==Player::Artists) return track.artist.empty()?"Unknown artist":track.artist;
    if(mode==Player::Genres) return track.genre.empty()?"Unknown genre":track.genre;
    return (track.album.empty()?"Unknown album":track.album)+" / "+(track.artist.empty()?"Unknown artist":track.artist);
}
}

void Player::loadLibraryState() {
    if(settingsDir.empty()) return;
    auto index=openFile(fs::path(settingsDir)/"library-index.txt");
    std::string magic, relative;
    if(std::getline(index,magic) && magic=="Turnwave index 1") {
        LibraryTrack track;
        while(index>>std::quoted(relative)>>track.added>>track.disc>>track.number
                   >>std::quoted(track.title)>>std::quoted(track.artist)>>std::quoted(track.album)>>std::quoted(track.genre)) {
            if(auto entry=resolveAudioEntry(root,relative)) {
                track.entry=std::move(*entry);libraryIndex.push_back(track);
            }
        }
        indexReady=index.eof();
    }
    applyLibraryTags();
    auto historyFile=openFile(fs::path(settingsDir)/"listening-history.txt");
    while(history.size()<100 && historyFile>>std::quoted(relative))
        if(auto entry=resolveAudioEntry(root,relative)) history.push_back(std::move(*entry));
}

void Player::refreshLibrary() {
    std::map<std::string,std::time_t> previous;
    for(const auto& track:libraryIndex) previous.emplace(track.entry.path,track.added);
    std::vector<LibraryTrack> refreshed;
    scan(musicFolder,refreshed,previous,!indexReady,0);
    if(!settingsDir.empty()) saveFile(fs::path(settingsDir)/"library-index.txt",[&](auto& out) {
        out<<"Turnwave index 1\n";
        for(const auto& t:refreshed)
            out<<std::quoted(fs::path(t.entry.path).lexically_relative(root).generic_string())<<' '
               <<t.added<<' '<<t.disc<<' '<<t.number<<' '<<std::quoted(t.title)<<' '
               <<std::quoted(t.artist)<<' '<<std::quoted(t.album)<<' '<<std::quoted(t.genre)<<'\n';
    });
    libraryIndex=std::move(refreshed);indexReady=true;
    applyLibraryTags();
}
void Player::showLibraryResults(std::vector<Entry> results,int group) {
    libraryResults=std::move(results);
    grouping=group;libraryView=true;favoritesView=false;view=View::Files;fileRow=0;
    notice=std::to_string(libraryResults.size())+" results";
}
void Player::browseLibrary(int mode) {
    if(mode==History) {showLibraryResults(history);return;}
    if(!indexReady) refreshLibrary();
    std::vector<Entry> results;
    if(mode==Recent) {
        auto sorted=libraryIndex;
        std::stable_sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.added>b.added;});
        for(size_t i=0;i<std::min<size_t>(100,sorted.size());++i) results.push_back(sorted[i].entry);
        showLibraryResults(std::move(results));return;
    }
    std::set<std::string> groups;
    for(const auto& track:libraryIndex) groups.insert(groupName(track,mode));
    for(const auto& label:groups) results.push_back({"",label,"",true});
    showLibraryResults(std::move(results),mode);
}
std::vector<Entry> Player::libraryGroupTracks(const std::string& label) const {
    std::vector<const LibraryTrack*> tracks;
    for(const auto& track:libraryIndex) if(groupName(track,grouping)==label) tracks.push_back(&track);
    std::stable_sort(tracks.begin(),tracks.end(),[](const auto* a,const auto* b) {
        return std::tie(a->album,a->artist,a->disc,a->number,a->entry.name)<
               std::tie(b->album,b->artist,b->disc,b->number,b->entry.name);
    });
    std::vector<Entry> results;
    for(const auto* track:tracks) results.push_back(track->entry);
    return results;
}
void Player::openLibraryGroup(const std::string& label) {
    showLibraryResults(libraryGroupTracks(label));
}
void Player::searchLibrary() {
    try {
        if(!indexReady) refreshLibrary();
        auto query=upper(searchQuery);
        std::vector<Entry> results;
        for(const auto& t:libraryIndex)
            if(upper(t.entry.name+" "+t.title+" "+t.artist+" "+t.album+" "+t.genre).find(query)!=std::string::npos)
                results.push_back(t.entry);
        showLibraryResults(std::move(results));
    } catch(const std::exception& error) {notice=error.what();}
}
void Player::rememberTrack() {
    historyRecorded=true;
    const auto& entry=queue[current];
    history.erase(std::remove_if(history.begin(),history.end(),[&](const Entry& item){return item.path==entry.path;}),history.end());
    history.insert(history.begin(),entry);
    if(history.size()>100) history.resize(100);
    historyDirty=true;
}
void Player::saveHistory() const {
    if(!historyDirty || settingsDir.empty()) return;
    saveFile(fs::path(settingsDir)/"listening-history.txt",[&](auto& out) {
        for(const auto& entry:history)
            out<<std::quoted(fs::path(entry.path).lexically_relative(root).generic_string())<<'\n';
    });
}
std::string Player::playbackOptionLabel(int row) const {
    static const char* labels[]={"ReplayGain off","ReplayGain track","ReplayGain album","Cancel sleep timer",
                                 "Stop in 15 minutes","Stop in 30 minutes","Stop in 60 minutes","Stop at end of track"};
    return labels[row];
}
void Player::playbackOption(int row) {
    if(row<3) gainMode=row;
    else {
        sleepAtEnd=row==7;
        constexpr int minutes[]={15,30,60};
        sleepDeadline=row>=4 && row<=6?Clock::now()+std::chrono::minutes(minutes[row-4]):Clock::time_point{};
    }
    notice=playbackOptionLabel(row);overlay=Overlay::None;
}
void Player::searchKey(Key key) {
    const int count=std::strlen(alphabet);
    if(key==Key::Back || key==Key::Mix) {overlay=Overlay::None;return;}
    if(key==Key::Up || key==Key::Down)
        searchCharacter=(searchCharacter+count+(key==Key::Up?1:-1))%count;
    if(key==Key::Left && !searchQuery.empty()) searchQuery.pop_back();
    if(key==Key::Right && searchQuery.size()<64) searchQuery+=alphabet[searchCharacter];
    if(key==Key::Accept) {overlay=Overlay::None;searchLibrary();}
}
