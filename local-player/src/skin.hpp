#pragma once
#include "player.hpp"
struct Skin {
    std::array<uint32_t,renderWidth*renderHeight> pixels{};
    void draw(const Player& p);
    void touch(Player& p,const Input& event);
};
