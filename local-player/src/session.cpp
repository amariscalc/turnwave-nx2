#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "session.hpp"
#include "player.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
extern "C" {
#include <libavutil/adler32.h>
}
namespace fs=std::filesystem;
namespace {
struct Snapshot {
    uint64_t generation=0;
    size_t index=0;
    bool active=false;
    uint64_t milliseconds=0;
    std::string name;
    std::vector<std::string> paths;
};
static const char* slots[]={".resume-a",".resume-b"};
uint32_t checksum(const std::string& bytes) {
    return av_adler32_update(1,reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size());
}
std::optional<Snapshot> readSlot(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);
    std::string magic;
    uint32_t expected;
    if(!std::getline(in,magic) || magic!="Turnwave session 1" ||
       !(in>>expected) || in.get()!='\n') return {};
    std::ostringstream raw;raw<<in.rdbuf();
    if(in.bad()) return {};
    auto bytes=raw.str();
    if(checksum(bytes)!=expected) return {};
    std::istringstream data(bytes);
    Snapshot s;
    size_t count;
    if(!(data>>s.generation>>s.active>>s.index>>s.milliseconds>>count>>std::quoted(s.name)) ||
       !s.generation || s.index>=count) return {};
    for(size_t i=0;i<count;i++) {
        std::string track;
        if(!(data>>std::quoted(track))) return {};
        s.paths.push_back(std::move(track));
    }
    data>>std::ws;
    if(!data.eof()) return {};
    return s;
}
std::optional<Snapshot> latest(const fs::path& dir) {
    auto a=readSlot(dir/slots[0]),b=readSlot(dir/slots[1]);
    if(!b || (a && a->generation>b->generation)) return a;
    return b;
}
}
void saveSession(const Player& p,const std::string& directory) {
    if(!p.hasSession || p.queue.empty()) return;
    fs::path dir(directory);
    if(mkdir(directory.c_str(),0777)!=0 && errno!=EEXIST)
        throw std::runtime_error("Cannot create session directory");
    auto previous=latest(dir);
    uint64_t generation=previous?previous->generation+1:1;
    bool active=p.current>=0 && p.audio;
    size_t index=active?p.current:p.selected;
    std::ostringstream out;
    out<<generation<<' '<<active<<' '<<index<<' '<<std::llround(p.position*1000)
       <<' '<<p.queue.size()<<'\n'<<std::quoted(p.queueName)<<'\n';
    for(const auto& track:p.queue)
        out<<std::quoted(fs::path(track.path).lexically_relative(p.root).generic_string())<<'\n';
    const auto body=out.str();
    auto target=(dir/slots[generation%2]).string();
    std::string temp=target+".XXXXXX";
    int fd=mkstemp(temp.data());
    if(fd<0) throw std::runtime_error("Cannot create playback session");
    FILE* stream=fdopen(fd,"wb");
    bool written=false;
    if(stream) {
        written=std::fprintf(stream,"Turnwave session 1\n%u\n",unsigned(checksum(body)))>0 &&
                std::fwrite(body.data(),1,body.size(),stream)==body.size();
        if(std::fclose(stream)!=0) written=false;
    } else close(fd);
    // Only retire the older slot. The newest valid file stays readable until publication.
    if(written && (std::remove(target.c_str())==0 || errno==ENOENT) &&
       std::rename(temp.c_str(),target.c_str())==0) return;
    std::remove(temp.c_str());
    throw std::runtime_error("Cannot save playback session");
}
void restoreSession(Player& p,const std::string& directory) {
    auto saved=latest(directory);
    if(!saved) return;
    std::vector<Entry> tracks;
    size_t selected=0;
    bool found=false,exact=false;
    for(size_t i=0;i<saved->paths.size();i++) {
        const auto& relative=saved->paths[i];
        auto e=resolveAudioEntry(p.root,relative);
        if(!e) continue;
        if(!found && i>=saved->index) {selected=tracks.size();exact=i==saved->index;found=true;}
        tracks.push_back(std::move(*e));
    }
    if(tracks.empty()) return;
    if(!found) selected=tracks.size()-1;
    p.queue=std::move(tracks);p.queueName=saved->name;p.selected=int(selected);
    p.hasSession=true;p.view=View::Playlist;
    if(!saved->active) return;
    p.play(p.selected,true);
    if(!p.audio) return;
    if(!exact) p.notice="Track missing: press A";
    else if(saved->milliseconds) {
        if(p.audio->duration>0) p.seek(double(saved->milliseconds)/1000);
        else p.notice="Position unavailable: press A";
    }
}
