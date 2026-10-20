#pragma once
#include <vector>

class VoxelGridProcessing
{
private:
    std::vector<int> _voxel_grid;
    std::vector<int> _initial_pos;
    std::vector<int> _min_pos;
    std::vector<int> _max_pos;

    int _x_length = _max_pos[0] - _min_pos[0] + 1;
    int _y_length = _max_pos[1] - _min_pos[1] + 1;
    int _z_length = _max_pos[2] - _min_pos[2] + 1;

    

public:
    VoxelGridProcessing(std::vector<int>& voxel_grid, std::vector<std::vector<int>>& metadata) : 
    _voxel_grid(voxel_grid), _initial_pos(metadata[0]), _min_pos(metadata[1]), _max_pos(metadata[2])
    {
    }

    int getIndex(const int& x, const int& y, const int& z);
    std::vector<int> extractPPVoxel();
};