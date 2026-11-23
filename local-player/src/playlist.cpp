#include "playlist.hpp"
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <stdexcept>
namespace fs=std::filesystem;
std::string upper(std::string s) {
    for(char& c:s) if(c>='a' && c<='z') c-=32;
    return s;
}
static std::string join(const std::string& path,const std::string& name) {
    return path+(path.back()=='/'?"":"/")+name;
}
static Entry entry(const std::string& full,const std::string& name,bool directory) {
    auto dot=name.rfind('.');
    std::string ext=dot==std::string::npos?"":upper(name.substr(dot+1));
    return {full,directory?name:name.substr(0,dot),ext,directory};
}
static bool isAudio(const std::string& ext) {
    static const std::set<std::string> types={"AAC","M4A","MP4","M4B","ALAC","FLAC","WAV","WAVE","AIF","AIFC","AIFF","MP3",
        "OGG","OGA","OPUS","WV","APE","MKA","WEBM","WMA","CAF"};
    return types.count(ext)>0;
}
std::vector<Entry> listMusic(const std::string& path) {
    std::unique_ptr<DIR,int(*)(DIR*)> dir(opendir(path.c_str()),closedir);
    if(!dir) throw std::runtime_error(std::string("Open folder: ")+std::strerror(errno));
    std::vector<Entry> entries;
    while(auto* item=readdir(dir.get())) {
        std::string name=item->d_name;
        if(name.empty() || name.front()=='.') continue;
        std::string full=join(path,name);
        struct stat info{};
        if(stat(full.c_str(),&info)!=0) continue;
        bool folder=S_ISDIR(info.st_mode);
        auto itemEntry=entry(full,name,folder);
        if(folder || (S_ISREG(info.st_mode) && (isAudio(itemEntry.ext) || itemEntry.playlist())))
            entries.push_back(std::move(itemEntry));
    }
    std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b) {
        if(a.directory!=b.directory) return a.directory;
        return upper(a.name)<upper(b.name);
    });
    return entries;
}

static std::string relativeTrackPath(const std::string& root,const std::string& list,std::string line) {
    std::replace(line.begin(),line.end(),'\\','/');
    fs::path base=fs::path(list).parent_path().lexically_relative(root);
    if(line.rfind("sdmc:/",0)==0) line.erase(0,5);
    if(line.empty() || line.find(':')!=std::string::npos || line.find('\0')!=std::string::npos ||
       line.rfind("//",0)==0) return {};
    if(line.front()=='/') {base.clear();line.erase(0,1);}
    auto relative=(base/line).lexically_normal();
    if(relative.empty() || *relative.begin()=="..") return {};
    return relative.generic_string();
}

std::optional<Entry> resolveAudioEntry(const std::string& root,const std::string& relative) {
    fs::path path=fs::path(relative).lexically_normal();
    if(path.empty() || path.is_absolute() || *path.begin()==".." ||
       relative.find(':')!=std::string::npos || relative.find('\0')!=std::string::npos) return {};
    auto full=join(root,path.generic_string());
    auto track=entry(full,path.filename().string(),false);
    struct stat st{};
    if(!isAudio(track.ext) || stat(full.c_str(),&st)!=0 || !S_ISREG(st.st_mode)) return {};
    return track;
}

LoadedMix readMix(const std::string& root,const std::string& file) {
    std::ifstream in(file,std::ios::binary);
    if(!in) throw std::runtime_error("Cannot open playlist");
    LoadedMix result;
    std::string line;
    bool first=true;
    while(std::getline(in,line)) {
        if(first && line.compare(0,3,"\xef\xbb\xbf")==0) line.erase(0,3);
        first=false;
        if(!line.empty() && line.back()=='\r') line.pop_back();
        if(line.empty() || line.front()=='#') continue;
        auto track=resolveAudioEntry(root,relativeTrackPath(root,file,line));
        if(track) result.tracks.push_back(std::move(*track));
        else ++result.skipped;
    }
    if(in.bad()) throw std::runtime_error("Could not read entire playlist");
    return result;
}
static void createFolder(const std::string& path) {
    if(mkdir(path.c_str(),0777)==0) return;
    struct stat st{};
    if(errno==EEXIST && stat(path.c_str(),&st)==0 && S_ISDIR(st.st_mode)) return;
    throw std::runtime_error("Cannot create playlist folder");
}
std::string mixDirectory(const std::string& musicFolder) {
    createFolder(musicFolder);
    auto folder=join(musicFolder,"Playlists");
    createFolder(folder);
    return folder;
}
std::string writeMix(const std::string& musicFolder,const std::vector<Entry>& tracks) {
    if(tracks.empty()) throw std::runtime_error("No tracks to save");
    const auto folder=mixDirectory(musicFolder);
    std::string contents="#EXTM3U\n";
    for(auto& track:tracks) {
        auto relative=fs::path(track.path).lexically_relative(folder).generic_string();
        if(relative.empty() || relative.find_first_of("\r\n")!=std::string::npos)
            throw std::runtime_error("Invalid track path");
        contents+="./"+relative+"\n";
    }
    for(unsigned id=1;id<1000000;id++) {
        char name[32];
        std::snprintf(name,sizeof name,"Mix-%03u.m3u8",id);
        auto target=join(folder,name), temp=target+".part";
        struct stat st{};
        if(stat(target.c_str(),&st)==0) continue;
        if(errno!=ENOENT) throw std::runtime_error("Cannot inspect playlist folder");
        int fd=open(temp.c_str(),O_CREAT|O_EXCL|O_WRONLY,0666);
        if(fd<0) {
            if(errno==EEXIST) continue;
            throw std::runtime_error("Cannot create playlist");
        }
        // Another writer may have published the same number before this reservation.
        if(stat(target.c_str(),&st)==0) {close(fd);unlink(temp.c_str());continue;}
        bool ok=errno==ENOENT;
        size_t offset=0;
        while(ok && offset<contents.size()) {
            ssize_t written=write(fd,contents.data()+offset,contents.size()-offset);
            if(written<0 && errno==EINTR) continue;
            if(written<=0) ok=false;
            else offset+=size_t(written);
        }
        if(close(fd)!=0) ok=false;
        if(ok && rename(temp.c_str(),target.c_str())==0) return name;
        unlink(temp.c_str());
        throw std::runtime_error("Could not finish saving playlist");
    }
    throw std::runtime_error("No free playlist number");
}
