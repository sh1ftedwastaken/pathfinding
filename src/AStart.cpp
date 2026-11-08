#include <iostream>
#include <cmath>
#include <vector>
#include <array>
#include <queue>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <memory>

#include "VoxelGrid.hpp"
#include "AStart.hpp"

using coord = std::array<int, 3>;

bool is_blocked(int v) {
    return v == BLOCK_BEDROCK || v == BLOCK_WATER || v == BLOCK_UNBREAKABLE || v == BLOCK_LAVA;
}

std::vector<coord> VoxelGrid::AStart(
    const coord& start_pos, 
    const coord& end_pos
)
{
   /*
    Grid values:
        1    solid
        2    air, bridging allowed
        3    air, bridging not allowed
        5    water
        10   falling block (sand/gravel)
        11   magma
        998  unbreakable / hazard
        999  lava
        9999 bedrock

        Costs:
        air    1
        solid  15
        magma  30
        step up/down adds 5

        Blocked for foot/head:
        5, 998, 999, 9999
   */
    std::vector<std::unique_ptr<Node>> buffer;
    auto make_node = [&buffer](coord pos, Node* parent) -> Node* 
    {
        buffer.push_back(std::make_unique<Node>(pos, parent)); 
        return buffer.back().get();
    };

    Node* start_node = make_node(start_pos, nullptr);

    std::priority_queue<Node*, std::vector<Node*>, CompareNodes> open_list;
    open_list.push(start_node);

    // Stores positions seen
    std::unordered_set<coord, coordHasher> closed_set;

    // Stores position with their lower G cost values {coordinates : Node->g}  
    std::unordered_map<coord, double, coordHasher> best_g;
    best_g[start_pos] = 0.0;
    
    while (!open_list.empty())
    {   
        // Get node with lower F cost
        Node* curr_node = open_list.top();
        open_list.pop();
        coord curr_pos = curr_node->position;
        
        // Base Case: Reach Goal + Backtrack path
        if (curr_pos == end_pos)
        {
            std::vector<coord> path;

            while (curr_node != nullptr)
            {   
                path.push_back(curr_node->position);
                curr_node = curr_node->parent;
            }

            std::reverse(path.begin(), path.end());
            return path;
        } 

        if (closed_set.count(curr_pos) == 1)
            continue;

        closed_set.insert(curr_pos);

        std::vector<coord> movements = {
            // orthogonal moves (dy = 0)
            {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1},
            // Step-up moves (dy = +1) - mining a 1-block vertical up
            {1, 1, 0}, {-1, 1, 0}, {0, 1, 1}, {0, 1, -1},
            // Drop-down moves (dy = -1) - mining a 1-block vertical down
            {1, -1, 0}, {-1, -1, 0}, {0, -1, 1}, {0, -1, -1},
            // Straight down
            {0, -1, 0}
        };

        for (const auto& move : movements)
        {   
            int dx = move[0];
            int dy = move[1];
            int dz = move[2];

            coord neighbor_pos = {
                curr_pos[0] + dx, 
                curr_pos[1] + dy, 
                curr_pos[2] + dz
            };

            int neighbor_head_idx  = getIndex(
                neighbor_pos[0], neighbor_pos[1] + 1, neighbor_pos[2]);
            int neighbor_foot_idx  = getIndex(
                neighbor_pos[0], neighbor_pos[1],     neighbor_pos[2]);
            int neighbor_floor_idx = getIndex(
                neighbor_pos[0], neighbor_pos[1] - 1, neighbor_pos[2]);

            // Check boundaries
            if (neighbor_foot_idx == -1 || 
                neighbor_head_idx == -1 || 
                neighbor_floor_idx == -1) 
                continue;
            
            // Check if floor is walkable
            if (_voxel_grid[neighbor_floor_idx] == BLOCK_WATER ||
                _voxel_grid[neighbor_floor_idx] == BLOCK_LAVA)
                continue;

            // Check if already seen
            if (closed_set.count(neighbor_pos) == 1)
                continue;
            
            // Check if is a obstacle
            if (is_blocked(_voxel_grid[neighbor_foot_idx]) ||
                is_blocked(_voxel_grid[neighbor_head_idx]))
                continue;

            // Check if the floor beneath the neighbor is air (stepping off a ledge)
            int drop_distance = 0;
            if (_voxel_grid[neighbor_floor_idx] == BLOCK_AIR || 
                _voxel_grid[neighbor_floor_idx] == BLOCK_AIR_NO_BRIDGE)
            {
                int scan_y = neighbor_pos[1] - 1;
                while (scan_y >= _min_pos[1])
                {
                    int scan_floor_idx = getIndex(
                        neighbor_pos[0], scan_y, neighbor_pos[2]);
                    if (scan_floor_idx == -1) 
                        break;

                    int block_val = _voxel_grid[scan_floor_idx];
                    if (block_val != BLOCK_AIR && 
                        block_val != BLOCK_AIR_NO_BRIDGE)
                        break;
                    
                    drop_distance++;
                    scan_y--;
                }
                
                if (drop_distance > 3)  // Doesn't handle water drop yet
                    continue;
                
                neighbor_pos[1] -= drop_distance;

                neighbor_head_idx  = getIndex(
                    neighbor_pos[0], neighbor_pos[1] + 1, neighbor_pos[2]);
                neighbor_foot_idx  = getIndex(
                    neighbor_pos[0], neighbor_pos[1],     neighbor_pos[2]);
                neighbor_floor_idx = getIndex(
                    neighbor_pos[0], neighbor_pos[1] - 1, neighbor_pos[2]);

                if (neighbor_foot_idx  == -1 || 
                    neighbor_head_idx  == -1 ||
                    neighbor_floor_idx == -1)
                    continue;
                if (is_blocked(_voxel_grid[neighbor_foot_idx]) ||
                    is_blocked(_voxel_grid[neighbor_head_idx]))
                    continue;
                if (_voxel_grid[neighbor_floor_idx] == BLOCK_WATER ||
                    _voxel_grid[neighbor_floor_idx] == BLOCK_LAVA)
                    continue;
            }

            // Determine base movement cost
            double move_cost = 1.0;
            int target_block_id = _voxel_grid[neighbor_foot_idx];

            if (target_block_id == BLOCK_AIR || target_block_id == BLOCK_AIR_NO_BRIDGE) {
                move_cost = 1.0;  // Air
            } 
            else if (target_block_id == BLOCK_SOLID) {
                move_cost = 15.0; // Solid block
            }
            else if (target_block_id == BLOCK_MAGMA) {
                move_cost = 30.0; // Magma block
            }

            // Vertical cost
            if (dy == 1) {
                move_cost += 5.0; // Cost for climbing abd and moving ortho
            } else if (dy == -1 && (dx != 0 || dz != 0)) {
                move_cost += 5.0; // Cost for mining down and moving ortho
            }
            
            double tentative_g = curr_node->g + move_cost;
            if (best_g.count(neighbor_pos) == 1 && tentative_g >= best_g[neighbor_pos])
                continue;
            Node* neighbor_node = make_node(neighbor_pos, curr_node);
            neighbor_node->g = tentative_g;
            neighbor_node->h = static_cast<double>(manhattanDistance(neighbor_pos, end_pos));
            neighbor_node->f = neighbor_node->g + neighbor_node->h;
            
            best_g[neighbor_pos] = tentative_g;
            open_list.push(neighbor_node);
        }
    }
    // There is not safe path connecting start to end
    return {};
}