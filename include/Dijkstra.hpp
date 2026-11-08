#pragma once

#include "PathResult.hpp"

// VoxelGrid.hpp already declares VoxelGrid::Dijkstra(start, end).
// This accessor keeps Richard's benchmarking data available without
// changing David's VoxelGrid public interface.
const PathResult& getLastDijkstraResult();
