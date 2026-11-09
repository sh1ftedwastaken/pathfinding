#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <queue>
#include <vector>

#include "VoxelGrid.hpp"
#include "AStart.hpp"
#include "MovementRules.hpp"

using coord = std::array<int, 3>;

thread_local PathResult g_lastAStarResult;

namespace {

struct FrontierNode
{
    double f;
    double g;
    coord position;
};

struct CompareFrontierNodes
{
    bool operator()(const FrontierNode& l, const FrontierNode& r) const
    {
        if (l.f != r.f)
            return l.f > r.f;

        // Prefer the node closer to the goal when F values tie.
        return l.g < r.g;
    }
};

double aStarHeuristic(const coord& from, const coord& to)
{
    const long long dx = std::llabs(
        static_cast<long long>(to[0]) - from[0]);
    const long long dz = std::llabs(
        static_cast<long long>(to[2]) - from[2]);
    const long long dy = static_cast<long long>(to[1]) - from[1];

    const long long horizontalMoves = dx + dz;
    const long long verticalMoves = dy > 0 ? dy : ((-dy) + 3) / 4;

    return static_cast<double>(std::max(horizontalMoves, verticalMoves));
}

}

bool is_blocked(int v) {
    return v == BLOCK_BEDROCK || v == BLOCK_WATER || v == BLOCK_UNBREAKABLE || v == BLOCK_LAVA;
}

const PathResult& getLastAStarResult() {
    return g_lastAStarResult;
}

std::vector<coord> VoxelGrid::AStart(
    const coord& start_pos,
    const coord& end_pos,
    const bool track_metrics
)
{
    const auto search_started = std::chrono::high_resolution_clock::now();
    std::size_t nodes_expanded = 0;
    std::size_t peak_frontier_size = 0;

    g_lastAStarResult = PathResult{};

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

    const int startIndex = getIndex(
        start_pos[0], start_pos[1], start_pos[2]);
    const int endIndex = getIndex(
        end_pos[0], end_pos[1], end_pos[2]);

    if (startIndex == -1 || endIndex == -1)
        return {};

    const int cellCount = _x_length * _y_length * _z_length;
    const double infinity = std::numeric_limits<double>::infinity();

    std::vector<double> best_g(
        static_cast<std::size_t>(cellCount), infinity);
    std::vector<int> parent(
        static_cast<std::size_t>(cellCount), -1);
    std::vector<unsigned char> closed(
        static_cast<std::size_t>(cellCount), 0);

    auto positionOf = [&](const int index) -> coord {
        const int layerSize = _x_length * _z_length;
        const int yIndex = index / layerSize;
        const int remainder = index % layerSize;
        const int zIndex = remainder / _x_length;
        const int xIndex = remainder % _x_length;

        return {
            _min_pos[0] + xIndex,
            _min_pos[1] + yIndex,
            _min_pos[2] + zIndex
        };
    };

    std::priority_queue<
        FrontierNode,
        std::vector<FrontierNode>,
        CompareFrontierNodes
    > open_list;

    best_g[startIndex] = 0.0;
    open_list.push({
        aStarHeuristic(start_pos, end_pos),
        0.0,
        start_pos
    });

    while (!open_list.empty())
    {
        if (track_metrics && open_list.size() > peak_frontier_size)
            peak_frontier_size = open_list.size();

        const FrontierNode current = open_list.top();
        open_list.pop();

        const int currentIndex = getIndex(
            current.position[0],
            current.position[1],
            current.position[2]);

        if (currentIndex == -1 || closed[currentIndex])
            continue;

        if (current.g > best_g[currentIndex])
            continue;

        closed[currentIndex] = 1;
        if (track_metrics)
            ++nodes_expanded;

        if (currentIndex == endIndex)
        {
            std::vector<coord> path;
            int cursor = endIndex;

            while (cursor != -1)
            {
                path.push_back(positionOf(cursor));
                if (cursor == startIndex)
                    break;
                cursor = parent[cursor];
            }

            if (path.empty() || path.back() != start_pos)
                path.clear();
            else
                std::reverse(path.begin(), path.end());

            if (track_metrics)
            {
                PathResult result;
                result.found = !path.empty();
                result.nodesExpanded = nodes_expanded;
                result.peakFrontierSize = peak_frontier_size;
                result.pathCost = current.g;
                result.estimatedMemoryBytes =
                    best_g.size() * sizeof(double) +
                    parent.size() * sizeof(int) +
                    closed.size() * sizeof(unsigned char);
                result.runtimeMs = std::chrono::duration<double, std::milli>(
                    std::chrono::high_resolution_clock::now() - search_started
                ).count();
                g_lastAStarResult = std::move(result);
            }

            return path;
        }

        for (std::size_t moveIndex = 0;
             moveIndex < kDefaultMoveCount;
             ++moveIndex)
        {
            const Position& move = kMoveOffsets[moveIndex];
            const int dx = move[0];
            const int dy = move[1];
            const int dz = move[2];

            coord neighbor_pos = {
                current.position[0] + dx,
                current.position[1] + dy,
                current.position[2] + dz
            };

            int neighbor_head_idx = getIndex(
                neighbor_pos[0], neighbor_pos[1] + 1, neighbor_pos[2]);
            int neighbor_foot_idx = getIndex(
                neighbor_pos[0], neighbor_pos[1], neighbor_pos[2]);
            int neighbor_floor_idx = getIndex(
                neighbor_pos[0], neighbor_pos[1] - 1, neighbor_pos[2]);

            // Check boundaries
            if (neighbor_foot_idx == -1 ||
                neighbor_head_idx == -1 ||
                neighbor_floor_idx == -1)
                continue;

            // Check if is a obstacle
            if (is_blocked(_voxel_grid[neighbor_foot_idx]) ||
                is_blocked(_voxel_grid[neighbor_head_idx]))
                continue;

            // Check the extra block swept by a vertical diagonal.
            const bool diagonal_move = dx != 0 || dz != 0;
            if (diagonal_move && dy == 1)
            {
                const int above_head_idx = getIndex(
                    current.position[0],
                    current.position[1] + 2,
                    current.position[2]);
                if (above_head_idx == -1 ||
                    is_blocked(_voxel_grid[above_head_idx]))
                    continue;
            }
            else if (diagonal_move && dy == -1)
            {
                const int front_head_idx = getIndex(
                    neighbor_pos[0],
                    current.position[1] + 1,
                    neighbor_pos[2]);
                if (front_head_idx == -1 ||
                    is_blocked(_voxel_grid[front_head_idx]))
                    continue;
            }

            // Check if floor is walkable
            if (_voxel_grid[neighbor_floor_idx] == BLOCK_WATER ||
                _voxel_grid[neighbor_floor_idx] == BLOCK_LAVA)
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

                neighbor_head_idx = getIndex(
                    neighbor_pos[0], neighbor_pos[1] + 1, neighbor_pos[2]);
                neighbor_foot_idx = getIndex(
                    neighbor_pos[0], neighbor_pos[1], neighbor_pos[2]);
                neighbor_floor_idx = getIndex(
                    neighbor_pos[0], neighbor_pos[1] - 1, neighbor_pos[2]);

                if (neighbor_foot_idx == -1 ||
                    neighbor_head_idx == -1 ||
                    neighbor_floor_idx == -1)
                    continue;
                if (is_blocked(_voxel_grid[neighbor_foot_idx]) ||
                    is_blocked(_voxel_grid[neighbor_head_idx]))
                    continue;
                if (_voxel_grid[neighbor_floor_idx] == BLOCK_WATER ||
                    _voxel_grid[neighbor_floor_idx] == BLOCK_LAVA)
                    continue;
            }

            const int neighborIndex = getIndex(
                neighbor_pos[0], neighbor_pos[1], neighbor_pos[2]);
            if (neighborIndex == -1 || closed[neighborIndex])
                continue;

            // Determine base movement cost
            double move_cost = COST_AIR;
            const int target_block_id = _voxel_grid[neighbor_foot_idx];

            if (target_block_id == BLOCK_AIR ||
                target_block_id == BLOCK_AIR_NO_BRIDGE)
            {
                move_cost = COST_AIR;
            }
            else if (target_block_id == BLOCK_SOLID)
            {
                move_cost = COST_SOLID;
            }
            else if (target_block_id == BLOCK_MAGMA)
            {
                move_cost = COST_MAGMA;
            }

            // Vertical cost
            if (dy == 1)
            {
                move_cost += 5.0;
            }
            else if (dy == -1 && (dx != 0 || dz != 0))
            {
                move_cost += 5.0;
            }

            const double tentative_g = current.g + move_cost;
            if (tentative_g >= best_g[neighborIndex])
                continue;

            best_g[neighborIndex] = tentative_g;
            parent[neighborIndex] = currentIndex;

            const double h = aStarHeuristic(neighbor_pos, end_pos);
            open_list.push({tentative_g + h, tentative_g, neighbor_pos});
        }
    }

    // There is not safe path connecting start to end
    if (track_metrics)
    {
        PathResult result;
        result.found = false;
        result.nodesExpanded = nodes_expanded;
        result.peakFrontierSize = peak_frontier_size;
        result.estimatedMemoryBytes =
            best_g.size() * sizeof(double) +
            parent.size() * sizeof(int) +
            closed.size() * sizeof(unsigned char);
        result.runtimeMs = std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - search_started
        ).count();
        g_lastAStarResult = std::move(result);
    }

    return {};
}
