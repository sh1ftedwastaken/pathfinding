#pragma once
#include <array>
#include <vector>

using coord = std::array<int, 3>;

constexpr int BLOCK_SOLID      = 1;
constexpr int BLOCK_AIR        = 2;
constexpr int BLOCK_AIR_NO_BRIDGE = 3;
constexpr int BLOCK_WATER      = 5;
constexpr int BLOCK_FALLING    = 10;
constexpr int BLOCK_MAGMA      = 11;
constexpr int BLOCK_LAVA       = 999;
constexpr int BLOCK_UNBREAKABLE = 998;  // Can walk on it but cannot be broken
constexpr int BLOCK_BEDROCK    = 9999;

constexpr double COST_AIR    = 1.0;
constexpr double COST_SOLID  = 15.0;
constexpr double COST_MAGMA  = 30.0;

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
    void extractPreProcessVoxel();

    std::vector<coord> AStart(const coord& start_pos, const coord& end_pos);
    std::vector<coord> Dijkstra(const coord& start_pos, const coord& end_pos);
};