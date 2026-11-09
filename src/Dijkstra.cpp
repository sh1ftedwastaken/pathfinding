#include "VoxelGrid.hpp"
#include "Dijkstra.hpp"
#include "MovementRules.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <queue>
#include <unordered_map>
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

struct DijkstraRun {
    std::vector<double> distance;
    std::vector<int> parent;
    std::vector<unsigned char> visited;
    std::vector<double> targetCosts;

    std::size_t nodesExpanded = 0;
    std::size_t peakFrontierSize = 0;
    std::size_t estimatedMemoryBytes = 0;

    bool found = false;
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

DijkstraRun runDijkstra(
    int* voxelGrid,
    const int xLength,
    const int yLength,
    const int zLength,
    const coord& minPos,
    const coord& startPos,
    const std::vector<coord>& targets,
    const bool needParents
) {
    DijkstraRun run;
    const double INF = std::numeric_limits<double>::infinity();

    const int cellCount = xLength * yLength * zLength;
    auto indexOf = [&](const int x, const int y, const int z) -> int {
        const int xIndex = x - minPos[0];
        const int yIndex = y - minPos[1];
        const int zIndex = z - minPos[2];

        if (xIndex < 0 || xIndex >= xLength ||
            yIndex < 0 || yIndex >= yLength ||
            zIndex < 0 || zIndex >= zLength) {
            return -1;
        }

        return xIndex + zIndex * xLength +
               yIndex * xLength * zLength;
    };

    run.targetCosts.assign(targets.size(), INF);
    if (targets.empty()) {
        return run;
    }

    const int startIndex = indexOf(startPos[0], startPos[1], startPos[2]);
    if (startIndex == -1) {
        return run;
    }

    std::unordered_map<int, std::vector<std::size_t>> targetSlots;
    std::size_t remainingTargets = 0;

    for (std::size_t index = 0; index < targets.size(); ++index) {
        const coord& target = targets[index];
        const int targetIndex = indexOf(target[0], target[1], target[2]);
        if (targetIndex == -1) {
            continue;
        }

        targetSlots[targetIndex].push_back(index);
        ++remainingTargets;
    }

    if (remainingTargets == 0) {
        return run;
    }

    run.distance.assign(cellCount, INF);
    run.visited.assign(cellCount, 0);
    if (needParents) {
        run.parent.assign(cellCount, -1);
    }

    std::priority_queue<
        QueueNode,
        std::vector<QueueNode>,
        CompareQueueNode
    > frontier;

    run.distance[startIndex] = 0.0;
    frontier.push({0.0, startPos});

    const MovementRules rules{};

    while (!frontier.empty()) {
        if (frontier.size() > run.peakFrontierSize) {
            run.peakFrontierSize = frontier.size();
        }

        const QueueNode top = frontier.top();
        frontier.pop();

        const coord currentPos = top.position;
        const int currentIndex = indexOf(
            currentPos[0], currentPos[1], currentPos[2]
        );

        if (currentIndex == -1 || run.visited[currentIndex]) {
            continue;
        }

        run.visited[currentIndex] = 1;
        run.nodesExpanded++;

        const auto targetIt = targetSlots.find(currentIndex);
        if (targetIt != targetSlots.end()) {
            for (const std::size_t slot : targetIt->second) {
                run.targetCosts[slot] = run.distance[currentIndex];
            }

            remainingTargets -= targetIt->second.size();
            if (remainingTargets == 0) {
                run.found = true;
                break;
            }
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

            int neighborHeadIndex = indexOf(
                neighborPos[0], neighborPos[1] + 1, neighborPos[2]
            );
            int neighborFootIndex = indexOf(
                neighborPos[0], neighborPos[1], neighborPos[2]
            );
            int neighborFloorIndex = indexOf(
                neighborPos[0], neighborPos[1] - 1, neighborPos[2]
            );

            // A player position needs a valid feet, head, and floor cell.
            if (neighborHeadIndex == -1 ||
                neighborFootIndex == -1 ||
                neighborFloorIndex == -1) {
                continue;
            }

            if (isBlocked(voxelGrid[neighborFootIndex]) ||
                isBlocked(voxelGrid[neighborHeadIndex])) {
                continue;
            }

            // check the extra block swept by a vertical diagonal.
            const bool diagonalMove = dx != 0 || dz != 0;
            if (diagonalMove && dy == 1) {
                const int aboveHeadIndex = indexOf(
                    currentPos[0], currentPos[1] + 2, currentPos[2]
                );
                if (aboveHeadIndex == -1 ||
                    isBlocked(voxelGrid[aboveHeadIndex])) {
                    continue;
                }
            }
            else if (diagonalMove && dy == -1) {
                const int frontHeadIndex = indexOf(
                    neighborPos[0], currentPos[1] + 1, neighborPos[2]
                );
                if (frontHeadIndex == -1 ||
                    isBlocked(voxelGrid[frontHeadIndex])) {
                    continue;
                }
            }

            if (voxelGrid[neighborFloorIndex] == BLOCK_WATER ||
                voxelGrid[neighborFloorIndex] == BLOCK_LAVA) {
                continue;
            }

            // If the neighboring position steps over air, scan downward
            // to find the landing position. This mirrors A*'s drop rule.
            int dropDistance = 0;
            if (voxelGrid[neighborFloorIndex] == BLOCK_AIR ||
                voxelGrid[neighborFloorIndex] == BLOCK_AIR_NO_BRIDGE) {

                int scanY = neighborPos[1] - 1;

                while (scanY >= minPos[1]) {
                    const int scanFloorIndex = indexOf(
                        neighborPos[0], scanY, neighborPos[2]
                    );

                    if (scanFloorIndex == -1) {
                        break;
                    }

                    const int blockId = voxelGrid[scanFloorIndex];
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

                neighborHeadIndex = indexOf(
                    neighborPos[0], neighborPos[1] + 1, neighborPos[2]
                );
                neighborFootIndex = indexOf(
                    neighborPos[0], neighborPos[1], neighborPos[2]
                );
                neighborFloorIndex = indexOf(
                    neighborPos[0], neighborPos[1] - 1, neighborPos[2]
                );

                if (neighborHeadIndex == -1 ||
                    neighborFootIndex == -1 ||
                    neighborFloorIndex == -1) {
                    continue;
                }

                if (isBlocked(voxelGrid[neighborFootIndex]) ||
                    isBlocked(voxelGrid[neighborHeadIndex])) {
                    continue;
                }

                if (voxelGrid[neighborFloorIndex] == BLOCK_WATER ||
                    voxelGrid[neighborFloorIndex] == BLOCK_LAVA) {
                    continue;
                }
            }

            const int neighborIndex = indexOf(
                neighborPos[0], neighborPos[1], neighborPos[2]
            );

            if (neighborIndex == -1 || run.visited[neighborIndex]) {
                continue;
            }

            double moveCost = baseMovementCost(
                voxelGrid[neighborFootIndex]
            );

            // Match the vertical movement penalties currently used by A*.
            if (dy == 1) {
                moveCost += 5.0;
            }
            else if (dy == -1 && (dx != 0 || dz != 0)) {
                moveCost += 5.0;
            }

            const double newCost = run.distance[currentIndex] + moveCost;

            // Dijkstra relaxation step.
            if (newCost < run.distance[neighborIndex]) {
                run.distance[neighborIndex] = newCost;
                if (needParents) {
                    run.parent[neighborIndex] = currentIndex;
                }
                frontier.push({newCost, neighborPos});
            }
        }
    }

    run.estimatedMemoryBytes =
        run.distance.size() * sizeof(double) +
        (needParents ? run.parent.size() * sizeof(int) : 0) +
        run.visited.size() * sizeof(unsigned char) +
        run.peakFrontierSize * sizeof(QueueNode);

    return run;
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

    const DijkstraRun run = runDijkstra(
        _voxel_grid,
        _x_length,
        _y_length,
        _z_length,
        _min_pos,
        start_pos,
        {end_pos},
        true
    );

    result.nodesExpanded = run.nodesExpanded;
    result.peakFrontierSize = run.peakFrontierSize;
    result.estimatedMemoryBytes = run.estimatedMemoryBytes;
    result.found = run.found;

    if (run.found) {
        const int startIndex = getIndex(
            start_pos[0], start_pos[1], start_pos[2]
        );
        int currentIndex = getIndex(end_pos[0], end_pos[1], end_pos[2]);

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

            currentIndex = run.parent[currentIndex];
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
            result.pathCost = run.targetCosts.front();
        }
    }

    return finish(result);
}

std::vector<double> VoxelGrid::DijkstraCosts(
    const coord& start_pos,
    const std::vector<coord>& targets
) {
    const DijkstraRun run = runDijkstra(
        _voxel_grid,
        _x_length,
        _y_length,
        _z_length,
        _min_pos,
        start_pos,
        targets,
        false
    );

    return run.targetCosts;
}
