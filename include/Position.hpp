#pragma once

#include <array>

// Keep Richard's pathfinding code compatible with David's VoxelGrid,
// which represents every 3D coordinate as std::array<int, 3>.
using Position = std::array<int, 3>;
