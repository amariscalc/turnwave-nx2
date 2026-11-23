#pragma once
#include <cstdint>
#include <string>
#include <vector>

constexpr int width=640, height=360, blockFrames=2048, bufferCount=4;
enum class Key { Up, Down, Left, Right, Accept, Back, Browse, Eq, Prev, Next, VolDown, VolUp, Shuffle, Repeat, Mix, Quit, Tap, Drag };
struct Input {Key key; int x=0,y=0;};
class Platform {
    struct Impl;
    Impl* p;
public:
    Platform();
    ~Platform();
    void open();
    std::vector<Input> poll();
    void present(const uint32_t* pixels);
    void clearAudio();
    void pause(bool paused);
    int queued();
    bool canQueue();
    void submit(const int16_t* pcm,int frames);
};
