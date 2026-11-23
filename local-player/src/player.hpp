#pragma once
#include "decoder.hpp"
#include "playlist.hpp"
#include "dsp.hpp"
#include "platform.hpp"
#include <memory>
#include <chrono>
#include <ctime>
#include <random>

enum class View {Playlist,Files,Eq};
struct Player {
    Platform& device;
    std::string root, folder, notice, queueName="LOCAL PLAYLIST", musicFolder;
    std::vector<Entry> queue,files,draft;
    bool coverAttempted=false;
    bool menu=false, hasSession=false;
    int menuRow=0;
    std::string mixPath, editName;
    bool naming=false;
    int letter=0;
    static constexpr const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_";
    void nameKey(Key key);
    void editMix(int action);
    enum MixAction {Add, Undo, SaveDraft, SaveQueue, OpenSaved, Rename, MoveUp, MoveDown, RemoveTrack, PlayNext, QueueLast, Favorite, Favorites, Presets, TrackInfo, Visualizer, Library, Artists, Albums, Genres, Recent, History, RefreshLibrary, ReplayGain, Sleep, QueueEdit, BookmarkSave, BookmarkGo, Lyrics, Cue, Speed, Tags, ActionCount};
    static constexpr int menuVisible=9;
    int menuTop() const {return std::max(0,menuRow-menuVisible+1);}
    enum class Overlay {None, Presets, Info, Spectrum, Wave, Search, PlaybackOptions, Queue, Lyrics, Cue, Speed, Tags};
    Overlay overlay=Overlay::None;
    int panelRow=0, keyboard=0, tagField=-1;
    std::array<std::string,4> tags{};
    std::string tagPath;
    double speed=1.;
    void setPlaybackSpeed(double rate);
    void loadTags(Decoder& decoder,const std::string& path) const;
    void saveTags();
    void applyLibraryTags();
    bool keyboardMode() const {return naming || overlay==Overlay::Search || tagField>=0;}
    static const char* keyLabel(int index);
    void keyPress(int index);
    void typeKey(Key key);
    std::vector<std::pair<double,std::string>> timedLines;
    void openTimed(bool cue);
    void panelKey(Key key);
    void bookmark(bool save);
    bool favoritesView=false, libraryView=false;
    std::vector<Entry> libraryResults;
    std::string searchQuery;
    int searchCharacter=0;
    struct LibraryTrack {
        Entry entry;
        std::string title, artist, album, genre;
        int disc=0, number=0;
        std::time_t added=0;
    };
    std::vector<LibraryTrack> libraryIndex;
    std::vector<Entry> history;
    bool indexReady=false, historyDirty=false, historyRecorded=false;
    int grouping=-1;
    void loadLibraryState();
    void saveHistory() const;
    void refreshLibrary();
    void browseLibrary(int mode);
    std::vector<Entry> libraryGroupTracks(const std::string& label) const;
    void openLibraryGroup(const std::string& label);
    void searchLibrary();
    void showLibraryResults(std::vector<Entry> results,int group=-1);
    void rememberTrack();
    using Clock=std::chrono::steady_clock;
    Clock::time_point sleepDeadline{};
    bool sleepAtEnd=false;
    int gainMode=0, optionRow=0;
    void playbackOption(int row);
    std::string playbackOptionLabel(int row) const;
    void searchKey(Key key);
    std::vector<Entry> favorites;
    std::array<float,256> wave{};
    int presetRow=0, nextCount=0;
    bool hasCustom=false;
    int customPreamp=0;
    std::array<int,10> customGain{};
    std::string settingsDir;
    void loadExtras(const std::string& directory);
    void saveExtras() const;
    const std::vector<Entry>& browserEntries() const {return libraryView?libraryResults:favoritesView?favorites:files;}
    void applyPreset(int row);
    static const char* presetName(int row);
    const Entry* highlightedTrack() const;
    bool isFavorite(const Entry& track) const;
    void mixAction(MixAction action);
    void openMenu();
    std::string menuLabel(MixAction action) const;
    std::unique_ptr<Decoder> audio;
    Equalizer eq;
    std::array<float,32> bars{};
    View view=View::Playlist;
    int current=-1, selected=0, fileRow=0, band=0;
    bool paused=false, shuffle=false, repeat=false;
    double position=0;
    explicit Player(Platform& device,std::string root,std::string folder);
    void play(int index,bool startPaused=false,bool keepPriority=false);
    void toggle();
    void stop();
    void seek(double seconds);
    void step(int delta,bool automatic=false);
    void tick();
    void input(Key key);
    void enter();
    void loadFolder(std::string path);
    void chooseFile();
    void setGain(int value);
    void toggleEq();
    void fail(const std::string& message);
    int topRow() const;
    static constexpr int visible=9;
private:
    std::vector<float> decoded;
    size_t offset=0;
    bool ended=false;
    double submitted=0;
    std::mt19937 rng;
};
