#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>
#include <unordered_set>

#include "VoxelGrid.hpp"

struct Cluster
{
    coord center;
    int size;
    double path_cost;
};

long long manhattanDistance(const coord& a, const coord& b)
{
    return std::abs(static_cast<long long>(a[0]) - b[0]) +
           std::abs(static_cast<long long>(a[1]) - b[1]) +
           std::abs(static_cast<long long>(a[2]) - b[2]);
}

VoxelGrid::VoxelGrid(int* voxel_grid, const std::vector<coord>& metadata) :
    _voxel_grid(voxel_grid),
    _initial_pos(metadata[0]),
    _min_pos(metadata[1]),
    _max_pos(metadata[2]),
    _x_length(_max_pos[0] - _min_pos[0] + 1),
    _y_length(_max_pos[1] - _min_pos[1] + 1),
    _z_length(_max_pos[2] - _min_pos[2] + 1)
{
}

int VoxelGrid::getIndex(const int& x, const int& y, const int& z)
{
    // Gets the index into `blocks` sequence for position (x, y, z).
    int x_index = x - _min_pos[0];
    int y_index = y - _min_pos[1];
    int z_index = z - _min_pos[2];

    if (x_index < 0 || x_index >= _x_length ||
        y_index < 0 || y_index >= _y_length || 
        z_index < 0 || z_index >= _z_length)
        return -1;
    
    return x_index + z_index * _x_length + y_index * _x_length * _z_length;
}

std::optional<coord> VoxelGrid::extractPreProcessVoxel()
{
    std::vector<coord> adj_direction = {
        {0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}
    };

    std::vector<coord> ore_coords;

    // Preprocessing hazard and ore blocks
    int block_id;
    for (int bx = _min_pos[0]; bx <= _max_pos[0]; bx++)
    {
        for (int by = _min_pos[1]; by <= _max_pos[1]; by++)
        {
            for (int bz = _min_pos[2]; bz <= _max_pos[2]; bz++)
            {
                block_id = _voxel_grid[getIndex(bx, by, bz)];
                
                // Skip processed / impossible blocks
                if (block_id == BLOCK_BEDROCK ||
                    block_id == BLOCK_UNBREAKABLE) continue;

                // Python marks the requested ore with id 0.
                if (block_id == 0)
                    ore_coords.push_back({bx, by, bz});

                // Environmental Hazards & Padding
                // Lava - Water proximity
                if (block_id == BLOCK_WATER || block_id == BLOCK_LAVA)
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
                                int& id = _voxel_grid[getIndex(nx, ny, nz)];
                                if (id == 1)
                                    id = BLOCK_UNBREAKABLE;
                            }
                    }
                }

                // Falling Block Safety 
                if (block_id == BLOCK_FALLING)
                {
                    // 1. Scan upwards to find the exact peak (top layer) of this gravel column
                    int peak_y = by;
                    while (peak_y + 1 <= _max_pos[1])
                    {
                        if (_voxel_grid[getIndex(bx, peak_y + 1, bz)] == BLOCK_FALLING)
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
                            if (_voxel_grid[idx] != BLOCK_FALLING) 
                            {
                                // Not breakable since top is still gravel
                                _voxel_grid[idx] = BLOCK_UNBREAKABLE;
                                break; 
                            }

                            // Top 2 layers mark as as safe standard block / walkable
                            if (depth_from_top < 2)
                                _voxel_grid[idx] = BLOCK_SOLID; 
                            // Deeper layers underneath mark as unwalkable
                            else
                                _voxel_grid[idx] = BLOCK_UNBREAKABLE;
                            depth_from_top++;
                        }
                    }
                }
            }
        }
    }

    // Find coords of the best ore cluster
    if (ore_coords.empty())
        return std::nullopt;

    std::unordered_set<int> visited;
    std::vector<Cluster> clusters;

    for (const coord& c : ore_coords)
    {
        int c_idx = getIndex(c[0], c[1], c[2]);
        if (visited.count(c_idx) == 1)
            continue;

        long long sum_x = 0;
        long long sum_y = 0;
        long long sum_z = 0;
        int size = 0;

        std::vector<coord> stack = {c};
        visited.insert(c_idx);

        while (!stack.empty())
        {
            coord current = stack.back();
            stack.pop_back();

            sum_x += current[0];
            sum_y += current[1];
            sum_z += current[2];
            ++size;

            for (int dx = -1; dx <= 1; ++dx)
            {
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dz = -1; dz <= 1; ++dz)
                    {
                        if (dx == 0 && dy == 0 && dz == 0)
                            continue;

                        coord neighbor = {
                            current[0] + dx,
                            current[1] + dy,
                            current[2] + dz
                        };

                        int neighbor_idx = getIndex(
                            neighbor[0], neighbor[1], neighbor[2]);

                        if (neighbor_idx == -1 ||
                            visited.count(neighbor_idx) == 1 ||
                            _voxel_grid[neighbor_idx] != 0)
                            continue;

                        visited.insert(neighbor_idx);
                        stack.push_back(neighbor);
                    }
                }
            }
        }

        coord center = {
            static_cast<int>(std::floor(static_cast<double>(sum_x) / size)),
            static_cast<int>(std::floor(static_cast<double>(sum_y) / size)),
            static_cast<int>(std::floor(static_cast<double>(sum_z) / size))
        };

        clusters.push_back({center, size, 0.0});
    }

    std::vector<coord> centers;
    centers.reserve(clusters.size());
    for (const Cluster& cluster : clusters)
        centers.push_back(cluster.center);

    const std::vector<double> path_costs = DijkstraCosts(_initial_pos, centers);
    for (std::size_t index = 0; index < clusters.size(); ++index)
        clusters[index].path_cost = path_costs[index];

    const Cluster* best_cluster = nullptr;
    double best_score = -std::numeric_limits<double>::infinity();

    for (const Cluster& cluster : clusters)
    {
        if (!std::isfinite(cluster.path_cost))
            continue;

        const double score = cluster.path_cost <= 0.0
            ? std::numeric_limits<double>::infinity()
            : static_cast<double>(cluster.size) / cluster.path_cost;

        if (best_cluster == nullptr ||
            score > best_score ||
            (score == best_score && cluster.size > best_cluster->size) ||
            (score == best_score && cluster.size == best_cluster->size &&
             cluster.center < best_cluster->center))
        {
            best_cluster = &cluster;
            best_score = score;
        }
    }

    if (best_cluster == nullptr)
        return std::nullopt;

    return best_cluster->center;
}