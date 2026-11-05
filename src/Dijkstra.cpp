#include "VoxelGrid.hpp"
#include "Dijkstra.hpp"
#include "MovementRules.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <queue>
#include <vector>

namespace {

struct QueueNode {
    double cost;
    coord position;
};

struct CompareQueueNode {
    bool operator()(const QueueNode& a, const QueueNode& b) const {
        // Reverse the normal priority_queue ordering so the
        // lowest-cost position is processed first.
        return a.cost > b.cost;
    }
};

thread_local PathResult g_lastDijkstraResult;

bool isBlocked(const int blockId) {
    return blockId == BLOCK_BEDROCK ||
           blockId == BLOCK_WATER ||
           blockId == BLOCK_UNBREAKABLE ||
           blockId == BLOCK_LAVA;
}

double baseMovementCost(const int blockId) {
    if (blockId == BLOCK_SOLID) {
        return COST_SOLID;
    }

    if (blockId == BLOCK_MAGMA) {
        return COST_MAGMA;
    }

    // Air and any remaining traversable block use the normal cost.
    return COST_AIR;
}

} // namespace

const PathResult& getLastDijkstraResult() {
    return g_lastDijkstraResult;
}

std::vector<coord> VoxelGrid::Dijkstra(
    const coord& start_pos,
    const coord& end_pos
) {
    PathResult result;
    const auto begin = std::chrono::high_resolution_clock::now();

    auto finish = [&](PathResult& completed) -> std::vector<coord> {
        const auto end = std::chrono::high_resolution_clock::now();
        completed.runtimeMs =
            std::chrono::duration<double, std::milli>(end - begin).count();

        g_lastDijkstraResult = completed;
        return completed.path;
    };

    const int startIndex = getIndex(start_pos[0], start_pos[1], start_pos[2]);
    const int goalIndex  = getIndex(end_pos[0], end_pos[1], end_pos[2]);

    if (startIndex == -1 || goalIndex == -1) {
        return finish(result);
    }

    const int cellCount = _x_length * _y_length * _z_length;
    const double INF = std::numeric_limits<double>::infinity();

    std::vector<double> distance(cellCount, INF);
    std::vector<int> parent(cellCount, -1);
    std::vector<unsigned char> visited(cellCount, 0);

    std::priority_queue<
        QueueNode,
        std::vector<QueueNode>,
        CompareQueueNode
    > frontier;

    distance[startIndex] = 0.0;
    frontier.push({0.0, start_pos});

    const MovementRules rules{};

    while (!frontier.empty()) {
        if (frontier.size() > result.peakFrontierSize) {
            result.peakFrontierSize = frontier.size();
        }

        const QueueNode top = frontier.top();
        frontier.pop();

        const coord currentPos = top.position;
        const int currentIndex = getIndex(
            currentPos[0], currentPos[1], currentPos[2]
        );

        if (currentIndex == -1 || visited[currentIndex]) {
            continue;
        }

        visited[currentIndex] = 1;
        result.nodesExpanded++;

        if (currentIndex == goalIndex) {
            result.found = true;
            break;
        }

        for (std::size_t moveIndex = 0;
             moveIndex < activeMoveCount(rules);
             ++moveIndex) {

            const Position& move = kMoveOffsets[moveIndex];
            const int dx = move[0];
            const int dy = move[1];
            const int dz = move[2];

            coord neighborPos = {
                currentPos[0] + dx,
                currentPos[1] + dy,
                currentPos[2] + dz
            };

            int neighborHeadIndex = getIndex(
                neighborPos[0], neighborPos[1] + 1, neighborPos[2]
            );
            int neighborFootIndex = getIndex(
                neighborPos[0], neighborPos[1], neighborPos[2]
            );
            int neighborFloorIndex = getIndex(
                neighborPos[0], neighborPos[1] - 1, neighborPos[2]
            );

            // A player position needs a valid feet, head, and floor cell.
            if (neighborHeadIndex == -1 ||
                neighborFootIndex == -1 ||
                neighborFloorIndex == -1) {
                continue;
            }

            if (isBlocked(_voxel_grid[neighborFootIndex]) ||
                isBlocked(_voxel_grid[neighborHeadIndex])) {
                continue;
            }

            // If the neighboring position steps over air, scan downward
            // to find the landing position. This mirrors A*'s drop rule.
            int dropDistance = 0;
            if (_voxel_grid[neighborFloorIndex] == BLOCK_AIR ||
                _voxel_grid[neighborFloorIndex] == BLOCK_AIR_NO_BRIDGE) {

                int scanY = neighborPos[1] - 1;

                while (scanY >= _min_pos[1]) {
                    const int scanFloorIndex = getIndex(
                        neighborPos[0], scanY, neighborPos[2]
                    );

                    if (scanFloorIndex == -1) {
                        break;
                    }

                    const int blockId = _voxel_grid[scanFloorIndex];
                    if (blockId != BLOCK_AIR &&
                        blockId != BLOCK_AIR_NO_BRIDGE) {
                        break;
                    }

                    dropDistance++;
                    scanY--;
                }

                if (dropDistance > 3) {
                    continue;
                }

                neighborPos[1] -= dropDistance;

                neighborHeadIndex = getIndex(
                    neighborPos[0], neighborPos[1] + 1, neighborPos[2]
                );
                neighborFootIndex = getIndex(
                    neighborPos[0], neighborPos[1], neighborPos[2]
                );
                neighborFloorIndex = getIndex(
                    neighborPos[0], neighborPos[1] - 1, neighborPos[2]
                );

                if (neighborHeadIndex == -1 ||
                    neighborFootIndex == -1 ||
                    neighborFloorIndex == -1) {
                    continue;
                }

                if (isBlocked(_voxel_grid[neighborFootIndex]) ||
                    isBlocked(_voxel_grid[neighborHeadIndex])) {
                    continue;
                }

                if (_voxel_grid[neighborFloorIndex] == BLOCK_WATER ||
                    _voxel_grid[neighborFloorIndex] == BLOCK_LAVA) {
                    continue;
                }
            }

            const int neighborIndex = getIndex(
                neighborPos[0], neighborPos[1], neighborPos[2]
            );

            if (neighborIndex == -1 || visited[neighborIndex]) {
                continue;
            }

            double moveCost = baseMovementCost(
                _voxel_grid[neighborFootIndex]
            );

            // Match the vertical movement penalties currently used by A*.
            if (dy == 1) {
                moveCost += 5.0;
            }
            else if (dy == -1 && (dx != 0 || dz != 0)) {
                moveCost += 5.0;
            }

            const double newCost = distance[currentIndex] + moveCost;

            // Dijkstra relaxation step.
            if (newCost < distance[neighborIndex]) {
                distance[neighborIndex] = newCost;
                parent[neighborIndex] = currentIndex;
                frontier.push({newCost, neighborPos});
            }
        }
    }

    if (result.found) {
        int currentIndex = goalIndex;

        while (currentIndex != -1) {
            const int yIndex = currentIndex / (_x_length * _z_length);
            const int remainder = currentIndex % (_x_length * _z_length);
            const int zIndex = remainder / _x_length;
            const int xIndex = remainder % _x_length;

            result.path.push_back({
                _min_pos[0] + xIndex,
                _min_pos[1] + yIndex,
                _min_pos[2] + zIndex
            });

            if (currentIndex == startIndex) {
                break;
            }

            currentIndex = parent[currentIndex];
        }

        // If the parent chain did not actually reach the start,
        // treat the result as invalid rather than returning a partial path.
        if (result.path.empty() || result.path.back() != start_pos) {
            result.path.clear();
            result.found = false;
            result.pathCost = 0.0;
        }
        else {
            std::reverse(result.path.begin(), result.path.end());
            result.pathCost = distance[goalIndex];
        }
    }

    result.estimatedMemoryBytes =
        distance.size() * sizeof(double) +
        parent.size() * sizeof(int) +
        visited.size() * sizeof(unsigned char) +
        result.peakFrontierSize * sizeof(QueueNode);

    return finish(result);
}
