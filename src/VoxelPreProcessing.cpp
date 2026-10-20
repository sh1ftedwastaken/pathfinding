#include <iostream>
#include <vector>
#include <array>
#include <format>

#include "voxelPreProcessing.hpp"

int VoxelGridProcessing::getIndex(const int& x, const int& y, const int& z)
{
    // Gets the index into `blocks` sequence for position (x, y, z).
    int x_index = x - _min_pos[0];
    int y_index = y - _min_pos[1];
    int z_index = z - _min_pos[2];

    if (x_index < 0 || x_index >= _x_length ||
        y_index < 0 || y_index >= _y_length || 
        z_index < 0 || z_index >= _z_length)
    {
        throw std::out_of_range(
            std::format("Block position ({}, {}, {})} out of bounds for BlockRegion covering {} to {}", x, y, z, _min_pos, _max_pos)
        );
    }
    
    return x_index + z_index * _x_length + y_index * _x_length * _z_length;
}


std::vector<int> VoxelGridProcessing::extractPPVoxel()
{
    std::vector<std::array<int, 3>> adj_direction = {
        {0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}
    };

    int block_id;
    for (int bx = _min_pos[0]; bx <= _max_pos[0]; bx++)
        for (int by = _min_pos[1]; by <= _max_pos[1]; by++)
            for (int bz = _min_pos[2]; bz <= _max_pos[2]; bz++)
            {
                block_id = _voxel_grid[getIndex(bx, by, bz)];
                
                // Skip processed / impossible blocks (9999)
                if (block_id == 9999) continue;

                // Environmental Hazards & Padding
                // Lava - Water proximity (Water = 5 / Lava = 999)
                if (block_id == 5 || block_id == 999)
                {
                    for (const auto& [dx, dy, dz] : adj_direction)
                    {
                        int nx = bx + dx; 
                        int ny = by + dy; 
                        int nz = bz + dz;

                        if (_min_pos[0] <= nx && nx <= _max_pos[0] &&
                            _min_pos[1] <= ny && ny <= _max_pos[1] &&
                            _min_pos[2] <= nz && nz <= _max_pos[2])
                            {
                                // Mark adjacent blocks as bedrock (impossible path) 
                                _voxel_grid[getIndex(nx, ny, nz)] = 9999;
                            }
                    }
                }

                // Falling Block Safety (Sand / Gravel = 10)
                if (block_id == 10)
                {
                    // 1. Scan upwards to find the exact peak (top layer) of this gravel column
                    int peak_y = by;
                    while (peak_y + 1 <= _max_pos[1])
                    {
                        if (_voxel_grid[getIndex(bx, peak_y + 1, bz)] == 10)
                            peak_y++;
                        else
                            break;
                    }    
                    
                    if (by == peak_y)
                    {
                        int depth_from_top = 0;

                        // walk down from peak to last gravel block
                        for (int y = peak_y; y >= _min_pos[1]; y--)
                        {
                            int idx = getIndex(bx, y, bz);
                            
                            // Stop non gravel
                            if (_voxel_grid[idx] != 10) 
                            {
                                // Not breakable since top is still gravel
                                _voxel_grid[idx] = 9999;
                                break; 
                            }

                            // Top 2 layers mark as as safe standard block / walkable
                            if (depth_from_top < 2)
                                _voxel_grid[idx] = 1; 
                            // Deeper layers underneath mark as unwalkable
                            else
                                _voxel_grid[idx] = 9999; // Impassable hazard
                            depth_from_top++;
                        }
                    }
                }              
            }
}