#pragma once

#include <array>
#include <cstddef>
#include "Position.hpp"

// Single source of truth for the moves used by Dijkstra.
// These match the movement offsets currently used by A*.
// Coordinate order is {x, y, z}, with y vertical.
struct MovementRules {
    bool allowStraightUp = false;
};

// 13 default moves + 1 optional straight-up move.
constexpr std::size_t kDefaultMoveCount = 13;
constexpr std::size_t kTotalMoveCount   = 14;

constexpr std::array<Position, kTotalMoveCount> kMoveOffsets = {{
    // Orthogonal moves (dy = 0)
    { 1,  0,  0}, {-1,  0,  0}, { 0,  0,  1}, { 0,  0, -1},

    // Step-up moves (dy = +1)
    { 1,  1,  0}, {-1,  1,  0}, { 0,  1,  1}, { 0,  1, -1},

    // Drop-down moves (dy = -1)
    { 1, -1,  0}, {-1, -1,  0}, { 0, -1,  1}, { 0, -1, -1},

    // Straight down
    { 0, -1,  0},

    // Optional straight up
    { 0,  1,  0}
}};

constexpr std::size_t activeMoveCount(const MovementRules& rules) {
    return rules.allowStraightUp ? kTotalMoveCount : kDefaultMoveCount;
}
