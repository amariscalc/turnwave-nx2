#include "player.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include "skin.hpp"
#include "session.hpp"
#include <map>
#include <set>
#include <thread>
#include <chrono>

static int rowFor(const Player& player,const std::string& name) {
    for(size_t i=0;i<player.files.size();i++) if(player.files[i].name==name) return int(i);
    throw std::runtime_error("Missing fixture: "+name);
}
static void checkMixes(Player& p,const std::string& root) {
    namespace fs=std::filesystem;
    auto album=fs::path(root)/"Albums";
    fs::copy_file(fs::path(root)/"flac.flac",album/"Música.flac");
    p.play(0);
    auto* playing=p.audio.get();
    p.input(Key::Browse);p.fileRow=rowFor(p,"wave");
    p.input(Key::Mix);p.input(Key::Accept);
    assert(p.draft.size()==1 && p.audio.get()==playing);
    p.fileRow=rowFor(p,"Albums");p.enter();
    p.fileRow=rowFor(p,"Música");
    Skin skin;
    skin.touch(p,{Key::Tap,603,69+20*(p.fileRow-p.topRow())+4});
    assert(p.draft.size()==2 && p.audio.get()==playing);
    p.mixAction(Player::Add);p.mixAction(Player::Undo);
    p.input(Key::Mix);p.input(Key::Down);p.input(Key::Down);p.input(Key::Accept);
    assert(p.draft.empty() && p.audio.get()==playing);
    auto dir=fs::path(root)/"Playlists";
    auto saved=dir/"Mix-001.m3u8";
    assert(fs::exists(saved));
    auto result=readMix(root,saved.string());
    assert(result.tracks.size()==2 && result.skipped==0);
    assert(result.tracks[0].name=="wave" && result.tracks[1].name=="Música");
    p.mixAction(Player::SaveQueue);
    assert(fs::exists(dir/"Mix-002.m3u8") && readMix(root,saved.string()).tracks.size()==2);
    p.mixAction(Player::OpenSaved);
    p.fileRow=rowFor(p,"Mix-001");p.enter();
    assert(p.queueName=="Mix-001" && p.current==0 && p.queue.size()==2 && p.audio);
    p.step(1);assert(p.current==1 && p.audio->format=="flac");
    p.mixAction(Player::MoveUp);assert(p.current==0 && p.queue.front().name=="Música");
    p.mixAction(Player::MoveDown);assert(p.current==1);
    p.mixAction(Player::Rename);assert(p.naming);p.nameKey(Key::Right);p.nameKey(Key::Accept);
    assert(!p.naming && fs::exists(dir/"A.m3u8"));
    p.selected=0;p.mixAction(Player::RemoveTrack);
    assert(p.current==0 && p.queue.size()==1 && fs::exists(fs::path(root)/"wave.wav"));
    assert(readMix(root,(dir/"A.m3u8").string()).tracks.size()==1);
    auto imported=dir/"External.m3u";
    std::ofstream(imported,std::ios::binary)<<"\xef\xbb\xbf#EXTM3U\r\n#EXTINF:3,title\r\n"
        <<"..\\Albums\\Música.flac\r\n/wave.wav\nsdmc:/wave.wav\nhttps://example.org/other.mp3\n"
        <<"missing.flac\n../../outside.wav\nMix-001.m3u8\n";
    result=readMix(root,imported.string());
    assert(result.skipped==4 && result.tracks.size()==3 && result.tracks[0].name=="Música");
    assert(result.tracks[1].path==result.tracks[2].path);
    std::ofstream(dir/"Empty.m3u8")<<"#EXTM3U\nmissing.mp3\n";
    p.mixAction(Player::OpenSaved);p.fileRow=rowFor(p,"Empty");
    playing=p.audio.get();p.enter();
    assert(p.audio.get()==playing && p.queue.size()==1 && !p.notice.empty());
    p.fileRow=rowFor(p,"External");p.enter();
    assert(p.queue.size()==3 && p.audio && p.notice=="Skipped 4 unavailable");
    p.mixAction(Player::Add);
    fs::rename(dir,fs::path(root)/"moved");
    std::ofstream(dir)<<"folder name occupied";
    p.mixAction(Player::SaveDraft);
    assert(p.draft.size()==1 && p.audio && !p.notice.empty());
    std::cout<<"Mix creation, touch and controller actions, import, playback and save failures passed.\n";
}

static void checkResume(Platform& device,const std::string& folder) {
    namespace fs=std::filesystem;
    auto state=fs::path(folder)/".test-state";
    Player first(device,folder,folder);
    restoreSession(first,state.string());assert(!first.audio);
    first.view=View::Files;first.fileRow=rowFor(first,"moved");first.enter();
    first.fileRow=rowFor(first,"External");first.enter();
    assert(first.queue.size()==3);
    first.play(2);first.seek(1.25);
    assert(!first.paused && first.current==2 && first.audio);
    saveSession(first,state.string());first.stop();
    Player second(device,folder,folder);
    restoreSession(second,state.string());
    assert(second.queueName=="External" && second.queue.size()==3 && second.current==2);
    assert(second.paused && second.selected==2 && second.position==1.25 && device.queued()==0);
    second.tick();assert(device.queued()==0 && second.position==1.25);
    second.input(Key::Accept);second.tick();assert(!second.paused && device.queued()>0);
    second.seek(.75);second.toggle();saveSession(second,state.string());
    // A truncated newest slot falls back to the previous complete exit.
    std::ofstream(state/".resume-a",std::ios::trunc)<<"incomplete";
    second.stop();
    Player fallback(device,folder,folder);restoreSession(fallback,state.string());
    assert(fallback.paused && fallback.current==2 && fallback.position==1.25);
    fs::remove(fs::path(folder)/"Albums"/"Música.flac");
    restoreSession(fallback,state.string());
    assert(fallback.queue.size()==2 && fallback.current==1 && fallback.position==1.25);
    fallback.stop();fallback.selected=1;saveSession(fallback,state.string());
    Player stopped(device,folder,folder);restoreSession(stopped,state.string());
    assert(!stopped.audio && stopped.queue.size()==2 && stopped.selected==1);
    std::cout<<"Session restore, duplicate entries, missing files, paused output and fallback passed.\n";
}
static void checkExtras(Platform& device,const std::string& folder) {
    namespace fs=std::filesystem;
    auto directory=fs::path(folder)/"Albums";
    Player p(device,folder,folder);p.loadExtras(directory.string());
    p.play(0);
    p.view=View::Files;p.fileRow=rowFor(p,"wave");
    p.mixAction(Player::Favorite);assert(p.favorites.size()==1);
    p.mixAction(Player::PlayNext);
    p.fileRow=rowFor(p,"flac");p.mixAction(Player::PlayNext);
    p.fileRow=rowFor(p,"mp3");p.mixAction(Player::QueueLast);
    assert(p.current==0 && p.queue[1].name=="wave" && p.queue[2].name=="flac" && p.queue.back().name=="mp3");
    p.shuffle=true;p.step(1);assert(p.current==1);
    p.step(1);assert(p.current==2);
    p.mixAction(Player::Presets);p.applyPreset(2);
    assert(p.eq.on && p.eq.gain[0]==5);
    p.eq.gain[0]=7;p.applyPreset(4);p.stop();
    Player restored(device,folder,folder);restored.loadExtras(directory.string());
    assert(restored.favorites.size()==1);
    restored.applyPreset(3);assert(restored.eq.gain[0]==7);
    restored.mixAction(Player::Favorites);restored.enter();assert(restored.audio && restored.queue.size()==1);
    restored.tick();restored.mixAction(Player::Visualizer);restored.input(Key::Accept);
    assert(restored.overlay==Player::Overlay::Wave);
    assert(std::any_of(restored.wave.begin(),restored.wave.end(),[](float v){return v!=0;}));
    Skin skin;skin.draw(restored);
    restored.input(Key::Back);restored.stop();
    Decoder cover;cover.open((directory/"art.mp3").string());cover.loadCover();
    assert(cover.album=="Covers" && cover.cover.size()==Decoder::coverSide*Decoder::coverSide);
}
static void checkLibrary(Platform& device,const std::string& folder) {
    Player player(device,folder,folder);
    player.play(0);
    auto* playing=player.audio.get();
    player.mixAction(Player::Library);
    assert(player.overlay==Player::Overlay::Search);
    player.searchQuery="cOvErS";
    player.keyPress(39);
    assert(player.audio.get()==playing && player.libraryView);
    assert(player.browserEntries().size()==1 && player.browserEntries().front().name=="art");
    player.mixAction(Player::PlayNext);
    assert(player.audio.get()==playing && player.queue.at(1).name=="art");
    player.enter();
    assert(player.audio && player.audio->album=="Covers" && player.queueName=="LIBRARY");
    player.searchQuery="no matching track";
    player.searchLibrary();
    assert(player.browserEntries().empty());
    player.enter();
    assert(player.audio && player.audio->album=="Covers");
    assert(std::abs(player.audio->trackGain-0.501187)<0.0001);
    assert(std::abs(player.audio->albumGain-0.707946)<0.0001);
    player.input(Key::Back);
    assert(!player.libraryView && player.view==View::Playlist);
}
static void checkLibraryIndex(Platform& device,const std::string& folder) {
    namespace fs=std::filesystem;
    auto settings=(fs::path(folder)/"Albums").string();
    Player player(device,folder,folder);
    player.loadExtras(settings);
    player.refreshLibrary();
    assert(player.indexReady && fs::exists(fs::path(settings)/"library-index.txt"));
    player.browseLibrary(Player::Albums);
    auto album=std::find_if(player.libraryResults.begin(),player.libraryResults.end(),
        [](const Entry& e){return e.name.find("Covers / ")==0;});
    assert(album!=player.libraryResults.end());
    player.fileRow=int(album-player.libraryResults.begin());
    player.mixAction(Player::QueueLast);
    assert(player.queue.back().name=="art");
    player.chooseFile();
    player.enter();
    assert(player.audio && player.audio->album=="Covers");
    assert(std::abs(player.audio->trackGain-0.501187)<0.0001);
    assert(std::abs(player.audio->albumGain-0.707946)<0.0001);
    player.tick();
    assert(player.history.front().name=="art");
    player.saveHistory();
    player.sleepDeadline=Player::Clock::now();
    player.tick();
    assert(!player.audio);
    Player restored(device,folder,folder);
    restored.loadExtras(settings);
    restored.loadLibraryState();
    assert(restored.indexReady && restored.history.front().name=="art");
    restored.browseLibrary(Player::Recent);
    assert(!restored.libraryResults.empty());
    restored.playbackOption(2);
    assert(restored.gainMode==2);
    restored.playbackOption(7);
    assert(restored.sleepAtEnd);
}
static void checkPlaybackTools(Platform& device,const std::string& folder) {
    namespace fs=std::filesystem;
    auto tools=fs::path(folder)/"Albums";
    fs::copy_file(fs::path(folder)/"wave.wav",tools/"chapters.wav");
    std::ofstream(tools/"chapters.lrc")<<"[00:00.00]First\n[00:01.00]Second\n";
    std::ofstream(tools/"chapters.cue")<<"FILE \"chapters.wav\" WAVE\n TRACK 01 AUDIO\n INDEX 01 00:00:00\n TRACK 02 AUDIO\n INDEX 01 00:01:00\n";
    Player player(device,folder,folder);
    player.loadExtras(tools.string());
    player.loadFolder(tools.string());player.view=View::Files;
    player.fileRow=rowFor(player,"chapters");player.chooseFile();
    assert(player.audio);
    player.mixAction(Player::Lyrics);
    assert(player.overlay==Player::Overlay::Lyrics && player.timedLines.size()==2);
    player.mixAction(Player::Cue);
    player.input(Key::Down);player.input(Key::Accept);
    assert(player.position==1.);
    player.bookmark(true);player.seek(0);player.bookmark(false);
    assert(player.position==1.);
    player.mixAction(Player::Tags);player.tags={"New title","New artist","New album","2"};
    player.saveTags();
    assert(player.audio->title=="New title" && player.audio->trackNumber==2);
    player.refreshLibrary();
    player.searchQuery="new title";player.searchLibrary();
    assert(player.libraryResults.size()==1);
    auto* active=player.audio.get();
    player.mixAction(Player::QueueEdit);
    player.input(Key::Right);
    assert(player.audio.get()==active);
    player.stop();
}
static void checkTempo(const std::string& folder) {
    Decoder decoder;
    decoder.open(folder+"/wave.wav");
    decoder.setSpeed(1.5);
    size_t frames=0;
    std::vector<float> output;
    while(decoder.read(output)) frames+=output.size()/2;
    assert(frames>85000 && frames<110000);
}
// Run with the fixtures created by check-audio.sh.
int main(int argc,char** argv) {
    assert(argc==2);
    std::string folder=argv[1];
    std::map<std::string,std::vector<int16_t>> output;
    auto entries=listMusic(folder);
    Equalizer eq;
    for(auto& file:entries) {
        if(file.directory) continue;
        Decoder d;d.open(file.path);
        auto& pcm=output[file.name];
        std::vector<float> chunk;
        while(d.read(chunk)) {
            size_t start=pcm.size();pcm.resize(start+chunk.size());
            eq.render(chunk.data(),chunk.size()/2,pcm.data()+start);
        }
        assert(pcm.size()>48000*2 && pcm.size()<48000*5*2);
        assert(std::any_of(pcm.begin(),pcm.end(),[](int v){return v!=0;}));
        assert(d.seek(1.0));assert(d.read(chunk) && !chunk.empty());
        std::cout<<file.name<<" "<<d.format<<" "<<d.sourceRate<<" Hz, "<<pcm.size()/2<<" stereo frames\n";
    }
    for(auto name:{"flac","alac","wavpack","aiff"}) assert(output.at(name)==output.at("wave"));
    assert(output.size()==11);
    Platform device;device.open();
    Player p(device,folder,folder);
    p.play(0);assert(p.audio);p.tick();
    p.toggle();assert(p.paused);p.seek(.7);p.toggle();p.tick();assert(!p.paused);
    p.step(1);assert(p.current==1);p.tick();
    p.input(Key::Eq);p.input(Key::Right);p.input(Key::Up);assert(p.eq.on && p.eq.gain[0]==1);
    p.input(Key::Browse);assert(p.view==View::Files);
    p.fileRow=0;p.chooseFile();assert(p.folder!=folder);p.input(Key::Back);assert(p.folder==folder);
    p.input(Key::Browse);assert(p.view==View::Playlist);
    p.play(p.queue.size()-1);p.seek(p.audio->duration-.4);
    for(int i=0;i<200 && p.audio;i++) {p.tick();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    assert(!p.audio);
    p.play(0);p.seek(p.audio->duration-.4);
    for(int i=0;i<200 && p.current==0;i++) {p.tick();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    assert(p.current==1);
    p.stop();
    checkMixes(p,folder);p.stop();
    checkResume(device,folder);
    checkExtras(device,folder);
    checkLibrary(device,folder);
    checkLibraryIndex(device,folder);
    checkTempo(folder);
    checkPlaybackTools(device,folder);
    std::cout<<"PCM equality, pause, seek, EQ, folder navigation and end-of-track transitions passed.\n";
}
