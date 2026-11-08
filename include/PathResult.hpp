#pragma once

#include <cstddef>
#include <vector>
#include "Position.hpp"

struct PathResult {
    std::vector<Position> path;

    double runtimeMs = 0.0;
    std::size_t nodesExpanded = 0;
    std::size_t peakFrontierSize = 0;

    double pathCost = 0.0;
    std::size_t estimatedMemoryBytes = 0;

    bool found = false;
};
