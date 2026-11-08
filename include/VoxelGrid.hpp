#pragma once
#include <cstddef>
#include <functional>
#include <array>
#include <optional>
#include <vector>

constexpr int BLOCK_SOLID       = 1;
constexpr int BLOCK_AIR         = 2;
constexpr int BLOCK_AIR_NO_BRIDGE = 3;
constexpr int BLOCK_WATER       = 5;
constexpr int BLOCK_FALLING     = 10;
constexpr int BLOCK_MAGMA       = 11;
constexpr int BLOCK_LAVA        = 999;
constexpr int BLOCK_UNBREAKABLE = 998;  // Can walk on it but cannot be broken
constexpr int BLOCK_BEDROCK     = 9999;

constexpr double COST_AIR    = 1.0;
constexpr double COST_SOLID  = 15.0;
constexpr double COST_MAGMA  = 30.0;

constexpr int    CLUSTER_CLOSE_TO_PLAYER = 10;
constexpr double CLUSTER_DISTANCE_SCORE  = 1.5;
constexpr double CLUSTER_SIZE_SCORE      = 2.3;


using coord = std::array<int, 3>;

struct coordHasher
{
    std::size_t operator()(const coord& arr) const noexcept 
    {
        std::size_t h = 0;
        for (int val : arr) {
            h ^= std::hash<int>{}(val) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

class VoxelGrid
{
private:
    int* _voxel_grid;

    coord _initial_pos;
    coord _min_pos;
    coord _max_pos;

    int _x_length;
    int _y_length;
    int _z_length;

public:
    VoxelGrid(int* voxel_grid, const std::vector<coord>& metadata);

    int getIndex(const int& x, const int& y, const int& z);
    std::optional<coord> extractPreProcessVoxel();

    std::vector<coord> AStart(const coord& start_pos, const coord& end_pos);
    std::vector<coord> Dijkstra(const coord& start_pos, const coord& end_pos);
};

long long manhattanDistance(const coord& a, const coord& b);