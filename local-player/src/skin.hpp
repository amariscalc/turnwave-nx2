#pragma once
#include "player.hpp"
struct Skin {
    std::array<uint32_t,width*height> pixels{};
    void draw(const Player& p);
    void touch(Player& p,const Input& event);
};
