#include "player.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <cstdio>

namespace fs=std::filesystem;

const char* Player::keyLabel(int index) {
    static constexpr const char* keys[]={
        "1","2","3","4","5","6","7","8","9","0",
        "Q","W","E","R","T","Y","U","I","O","P",
        "A","S","D","F","G","H","J","K","L","-",
        "Z","X","C","V","B","N","M","SPACE","DEL","OK"};
    return keys[index];
}
void Player::keyPress(int index) {
    if(index<0 || index>=40) return;
    keyboard=index;
    auto& value=tagField>=0?tags[tagField]:naming?editName:searchQuery;
    if(index==39) {
        if(tagField>=0) tagField=-1;else if(naming) nameKey(Key::Accept);else searchKey(Key::Accept);
    } else if(index==38) {
        if(!value.empty()) {
            size_t end=value.size()-1;
            while(end && (static_cast<unsigned char>(value[end])&0xc0)==0x80) --end;
            value.resize(end);
        }
    } else if(value.size()<64) value+=index==37?' ':keyLabel(index)[0];
}
void Player::typeKey(Key key) {
    if(key==Key::Back) {
        if(tagField>=0) tagField=-1;else if(naming) nameKey(key);else searchKey(key);
        return;
    }
    if(key==Key::Accept) {keyPress(keyboard);return;}
    int x=keyboard%10,y=keyboard/10;
    if(key==Key::Left) x=(x+9)%10;
    if(key==Key::Right) x=(x+1)%10;
    if(key==Key::Up) y=(y+3)%4;
    if(key==Key::Down) y=(y+1)%4;
    keyboard=y*10+x;
}
void Player::bookmark(bool save) {
    if(!audio || settingsDir.empty()) {notice="Play a track first";return;}
    auto file=fs::path(settingsDir)/"bookmarks.txt";
    auto key=fs::path(queue[current].path).lexically_relative(root).generic_string();
    std::ifstream input(file);
    std::string entry;
    double value=0, found=-1;
    while(input>>std::quoted(entry)>>value)
        if(entry==key && std::isfinite(value) && value>=0) found=value;
    if(!save) {
        if(found>=0) seek(found);else notice="No bookmark for this track";
        return;
    }
    input.close();
    std::ofstream output(file,std::ios::app);
    output<<std::quoted(key)<<' '<<std::setprecision(17)<<position<<'\n';
    output.close();
    notice=output?"Bookmark saved":"Cannot save bookmark";
}
void Player::openTimed(bool cue) {
    if(!audio) {notice="Play a track first";return;}
    fs::path media(queue[current].path), sidecar=media;
    sidecar.replace_extension(cue?".cue":".lrc");
    std::ifstream input(sidecar);
    std::vector<std::pair<double,std::string>> lines;
    std::string line,title;
    bool sameFile=true,audioTrack=false;
    while(lines.size()<10000 && std::getline(input,line)) {
        if(!line.empty() && line.back()=='\r') line.pop_back();
        if(line.rfind("\xef\xbb\xbf",0)==0) line.erase(0,3);
        if(cue) {
            std::istringstream row(line);
            std::string command;row>>command;
            if(command=="FILE") {
                std::string filename;row>>std::quoted(filename);
                sameFile=upper(fs::path(filename).filename().string())==upper(media.filename().string());
                audioTrack=false;
            } else if(command=="TRACK") {
                int track=0;std::string kind;row>>track>>kind;
                audioTrack=kind=="AUDIO";title="Track "+std::to_string(track);
            } else if(command=="TITLE" && audioTrack) row>>std::quoted(title);
            else if(command=="INDEX" && sameFile && audioTrack) {
                int index=0,m=0,s=0,f=0;std::string stamp;row>>index>>stamp;
                if(index==1 && std::sscanf(stamp.c_str(),"%d:%d:%d",&m,&s,&f)==3 &&
                   m>=0 && s>=0 && s<60 && f>=0 && f<75)
                    lines.emplace_back(m*60.+s+f/75.,title);
            }
        } else {
            size_t start=0;
            std::vector<double> stamps;
            while(start<line.size() && line[start]=='[' && stamps.size()<64) {
                auto end=line.find(']',start);
                if(end==std::string::npos) break;
                int m=0;double s=0;char tail=0;
                auto stamp=line.substr(start+1,end-start-1);
                if(std::sscanf(stamp.c_str(),"%d:%lf%c",&m,&s,&tail)!=2 ||
                   m<0 || !std::isfinite(s) || s<0 || s>=60) break;
                stamps.push_back(m*60.+s);start=end+1;
            }
            for(double stamp:stamps) lines.emplace_back(stamp,line.substr(start));
        }
    }
    std::stable_sort(lines.begin(),lines.end());
    if(lines.empty()) {notice=cue?"No matching CUE chapters":"No synced LRC lyrics";return;}
    timedLines=std::move(lines);panelRow=0;overlay=cue?Overlay::Cue:Overlay::Lyrics;
}
void Player::panelKey(Key key) {
    if(key==Key::Back || key==Key::Mix) {overlay=Overlay::None;return;}
    if(overlay==Overlay::Tags || overlay==Overlay::Speed) {
        int last=overlay==Overlay::Tags?4:2;
        if(key==Key::Up || key==Key::Down) panelRow=std::clamp(panelRow+(key==Key::Up?-1:1),0,last);
        if(key==Key::Accept) {
            try {
                if(overlay==Overlay::Speed) setPlaybackSpeed(panelRow==0?1.:panelRow==1?1.25:1.5);
                else if(panelRow==4) saveTags();
                else {tagField=panelRow;keyboard=0;}
            } catch(const std::exception& error) {notice=error.what();}
        }
        return;
    }
    const bool editing=overlay==Overlay::Queue;
    const int count=editing?int(queue.size()):int(timedLines.size());
    if(!count || overlay==Overlay::Lyrics) return;
    panelRow=std::clamp(panelRow,0,count-1);
    if(key==Key::Up || key==Key::Down) {
        panelRow=std::clamp(panelRow+(key==Key::Up?-1:1),0,count-1);return;
    }
    if(!editing) {
        if(key==Key::Accept) seek(timedLines[panelRow].first);
        return;
    }
    if(key==Key::Accept && panelRow!=current) {
        queue.erase(queue.begin()+panelRow);
        if(current>panelRow) --current;
        panelRow=std::min(panelRow,std::max(0,int(queue.size())-1));
    } else if(key==Key::Left || key==Key::Right) {
        int target=panelRow+(key==Key::Left?-1:1);
        if(target<0 || target>=count) return;
        std::swap(queue[panelRow],queue[target]);
        if(current==panelRow) current=target;else if(current==target) current=panelRow;
        panelRow=target;
    } else return;
    selected=current>=0?current:panelRow;
    nextCount=0;mixPath.clear();hasSession=true;
}

void Player::setPlaybackSpeed(double rate) {
    const double previous=speed;
    speed=rate;
    if(audio && (audio->duration<=0 || !audio->seek(position))) {
        speed=previous;notice="Track does not support seeking";return;
    }
    if(audio) {
        try {audio->setSpeed(speed);}
        catch(...) {speed=1.;fail("Could not initialize playback speed");return;}
        device.clearAudio();decoded.clear();offset=0;ended=false;submitted=position;
        if(!paused) device.pause(false);
    }
    overlay=Overlay::None;
}

void Player::loadTags(Decoder& decoder,const std::string& path) const {
    if(settingsDir.empty()) return;
    std::ifstream input(fs::path(settingsDir)/"track-tags.txt");
    const auto relative=fs::path(path).lexically_relative(root).generic_string();
    std::string key,title,artist,album;
    int number=0;
    while(input>>std::quoted(key)>>std::quoted(title)>>std::quoted(artist)>>std::quoted(album)>>number) {
        if(key!=relative) continue;
        decoder.title=title;decoder.artist=artist;decoder.album=album;decoder.trackNumber=number;
    }
}

void Player::saveTags() {
    if(settingsDir.empty()) {notice="Track settings unavailable";return;}
    int number=0;
    if(!tags[3].empty()) {
        if(tags[3].size()>4 || tags[3].find_first_not_of("0123456789")!=std::string::npos) {
            notice="Enter a track number from 0 to 9999";return;
        }
        number=std::stoi(tags[3]);
    }
    std::ofstream out(fs::path(settingsDir)/"track-tags.txt",std::ios::app);
    out<<std::quoted(fs::path(tagPath).lexically_relative(root).generic_string())<<' '
       <<std::quoted(tags[0])<<' '<<std::quoted(tags[1])<<' '<<std::quoted(tags[2])<<' '<<number<<'\n';
    out.close();
    if(!out) {notice="Cannot save tags";return;}
    if(audio && queue[current].path==tagPath) {
        audio->title=tags[0];audio->artist=tags[1];audio->album=tags[2];audio->trackNumber=number;
    }
    for(auto& track:libraryIndex) if(track.entry.path==tagPath) {
        track.title=tags[0];track.artist=tags[1];track.album=tags[2];track.number=number;
    }
    notice="Library tags saved";overlay=Overlay::None;
}

void Player::applyLibraryTags() {
    if(settingsDir.empty()) return;
    std::ifstream input(fs::path(settingsDir)/"track-tags.txt");
    std::map<std::string,LibraryTrack*> tracks;
    for(auto& track:libraryIndex)
        tracks.emplace(fs::path(track.entry.path).lexically_relative(root).generic_string(),&track);
    std::string key,title,artist,album;
    int number=0;
    while(input>>std::quoted(key)>>std::quoted(title)>>std::quoted(artist)>>std::quoted(album)>>number) {
        auto found=tracks.find(key);
        if(found==tracks.end()) continue;
        auto& track=*found->second;
        track.title=title;track.artist=artist;track.album=album;track.number=number;
    }
}
