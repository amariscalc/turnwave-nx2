#pragma once
#include <string>

struct Player;

// Save on normal exit. Alternate slots keep the previous complete save available
// even on SD filesystems that cannot replace a file in a single rename.
void restoreSession(Player& player,const std::string& directory);
void saveSession(const Player& player,const std::string& directory);
