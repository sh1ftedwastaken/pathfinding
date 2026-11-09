# VoxelRoute: 3D Minecraft Pathfinding

**COP3530 Project 2**  
**Team Members:** David Zou and Richard Shiau

## Overview

VoxelRoute is a 3D pathfinding system for Minecraft that compares **A\*** and **Dijkstra's algorithm** on the same voxel-based world data.

The program extracts a region of the Minecraft world using Minescript, converts the blocks into a contiguous voxel grid, preprocesses hazards and ore locations, and then sends the grid to a C++ pathfinding engine through pybind11. The resulting route is returned to Python and visualized directly in Minecraft using an animated glass path.

If no destination is provided, the program searches for nearby diamond ore clusters and selects a reachable cluster based on both cluster size and path cost.

## Features

- 3D Minecraft voxel extraction using Minescript
- A* pathfinding implemented from scratch in C++
- Dijkstra's algorithm implemented from scratch in C++
- Shared weighted movement rules for both algorithms
- Hazard preprocessing for lava, water, falling blocks, bedrock, and unsafe areas
- Automatic ore-cluster detection and target selection
- Support for user-specified target coordinates
- Step-up, step-down, and limited drop movement
- Animated in-game path visualization using colored glass
- Python/C++ integration through pybind11
- Prebuilt Windows Python extensions for Python 3.12-3.15

## How It Works

The project is split into four main stages:

```text
Minecraft World
      |
      v
Minescript / Python
Extract voxel data
      |
      v
C++ VoxelGrid
Preprocess hazards and ore clusters
      |
      v
A* or Dijkstra
Find a lowest-cost path
      |
      v
Python / Minescript
Render the path in Minecraft
```

### 1. Voxel Extraction

`pathfiding/core/voxelGridExtraction.py` reads a region around the player and converts Minecraft blocks into integer values stored in a flat voxel buffer.

The extraction begins with a radius of 16 blocks and expands until the target ore is found or the maximum search radius is reached.

### 2. Preprocessing

`VoxelGrid::extractPreProcessVoxel()` prepares the grid before pathfinding.

It:

- Detects ore blocks and groups connected ore blocks into clusters
- Marks unsafe areas near lava and water
- Handles falling blocks such as sand and gravel
- Identifies unreachable or unbreakable areas
- Uses Dijkstra path costs to help select the best reachable ore cluster

### 3. Pathfinding

Both algorithms operate on the same voxel grid and movement-cost system.

#### A*

A* uses:

```text
f(n) = g(n) + h(n)
```

where:

- `g(n)` is the accumulated movement cost
- `h(n)` is the Manhattan distance to the target

The heuristic guides the search toward the destination.

#### Dijkstra

Dijkstra explores positions based only on the lowest accumulated path cost. It does not use a heuristic.

Dijkstra also supports calculating costs to multiple targets, which is used during ore-cluster selection.

## Movement and Cost Model

The grid uses integer block categories such as:

| Block Type | Value |
| --- | ---: |
| Solid | 1 |
| Air / bridge allowed | 2 |
| Air / bridge not allowed | 3 |
| Water | 5 |
| Falling block | 10 |
| Magma | 11 |
| Unbreakable | 998 |
| Lava | 999 |
| Bedrock | 9999 |

Basic movement costs are:

| Movement / Block | Cost |
| --- | ---: |
| Air | 1 |
| Solid block | 15 |
| Magma | 30 |
| Step up | +5 |
| Diagonal step down | +5 |

The pathfinder supports:

- Horizontal movement
- One-block step-up movement
- One-block step-down movement
- Straight-down movement
- Drops of up to three blocks

Water, lava, bedrock, and unbreakable cells are treated as blocked for player movement.

## Repository Structure

```text
pathfinding/
├── include/
│   ├── AStart.hpp
│   ├── Dijkstra.hpp
│   ├── MovementRules.hpp
│   ├── PathResult.hpp
│   ├── Position.hpp
│   └── VoxelGrid.hpp
│
├── src/
│   ├── AStart.cpp
│   ├── Dijkstra.cpp
│   ├── VoxelGrid.cpp
│   └── binding.cpp
│
├── pathfiding/
│   └── core/
│       ├── constants.py
│       ├── lib_blockpack_parser.py
│       ├── minescript_extra.py
│       ├── path_planner.py
│       ├── player.py
│       └── voxelGridExtraction.py
│
├── dist/
│   ├── pathfinding.cp312-win_amd64.pyd
│   ├── pathfinding.cp313-win_amd64.pyd
│   ├── pathfinding.cp314-win_amd64.pyd
│   └── pathfinding.cp315-win_amd64.pyd
│
├── test/
│   └── test.py
│
├── main.py
├── CmakeLists.txt
└── README.md
```

> **Note:** The folder name `pathfiding/` is intentionally written this way in the current project and is referenced by `main.py`.

## Requirements

The included prebuilt extension files are intended for **64-bit Windows**.

Recommended environment:

- Minecraft Java Edition
- Minescript 5.0 or newer
- CPython 3.12, 3.13, 3.14, or 3.15
- A matching `.pyd` file from the `dist/` directory

Building the C++ extension from source additionally requires:

- CMake 3.18+
- A C++20-compatible compiler
- pybind11
- Python development files for the Python version being built

## Installation

### Using the Prebuilt Python Extension

1. Install Minescript and launch Minecraft once so that the `minescript` directory is created.

2. Place the project folder inside the Minecraft Minescript directory so that the layout is similar to:

```text
.minecraft/
└── minescript/
    └── pathfinding/
        ├── main.py
        ├── dist/
        ├── pathfiding/
        ├── include/
        └── src/
```

3. Configure Minescript to use a supported CPython version that matches one of the files in `dist/`.

For example:

```text
Python 3.12 -> pathfinding.cp312-win_amd64.pyd
Python 3.13 -> pathfinding.cp313-win_amd64.pyd
Python 3.14 -> pathfinding.cp314-win_amd64.pyd
Python 3.15 -> pathfinding.cp315-win_amd64.pyd
```

4. Open a Minecraft world.

5. Start the project from the Minecraft chat:

```text
\pathfinding\main
```

The chat should display:

```text
Pathfinding ACTIVATED
Use '.pathfind help' for commands
```

## Commands

After starting the main script, use `.pathfind` commands in normal Minecraft chat.

### Show Help

```text
.pathfind help
```

### Run A*

```text
.pathfind astar
```

Without a target coordinate, the program automatically selects the best reachable diamond-ore cluster.

### Run Dijkstra

```text
.pathfind dijkstra
```

### Specify a Target Coordinate

```text
.pathfind astar (-10, -60, 100)
```

or:

```text
.pathfind dijkstra (-10, -60, 100)
```

Coordinates must use the format:

```text
(x, y, z)
```

### Control Visualization Speed

A number after the algorithm specifies the delay between path-building commands.

Example:

```text
.pathfind dijkstra 1.5
```

This waits 1.5 seconds between visualization steps.

A custom target and delay can also be combined:

```text
.pathfind astar 0.1 (-10, -60, 100)
```

### Stop Pathfinding

```text
.pathfind stop
```

To stop all pathfinding jobs:

```text
.pathfind stop all
```

## Path Visualization

After a path is found, Minecraft commands are used to visualize the route.

The trail changes through three colors:

```text
Green -> Yellow -> Red -> Removed
```

This creates an animated path through the environment while keeping only a short section visible at a time.

Because the visualization uses Minecraft `setblock` commands, it modifies blocks in the world. Testing in a backup or disposable world is recommended.

## Building From Source

The repository includes a pybind11 submodule.

After cloning:

```bash
git submodule update --init --recursive
```

To build for one Python version, for example Python 3.12:

```bash
cmake -S . -B build -DPATHFINDING_BUILD_ALL_PYTHON=OFF -DPATHFINDING_PYTHON_VERSION=3.12
```

Then build:

```bash
cmake --build build --config Release
```

The compiled Python extension is copied into the `dist/` directory.

The CMake configuration can also build versions for Python 3.12 through 3.15 when all matching Python installations are available.

## Testing

`test/test.py` contains runtime diagnostics for:

- Voxel extraction
- Ore detection
- Ore-cluster grouping
- Dijkstra path costs
- Cluster scoring
- A* path generation

The test is intended to be executed through Minescript while inside a Minecraft world.

## Team Contributions

### David Zou

- 3D A* implementation
- Minecraft/Minescript integration
- Voxel data extraction
- pybind11 integration
- In-game path rendering

### Richard Shiau

- 3D Dijkstra implementation
- Voxel movement and validation integration
- Dijkstra multi-target cost calculation
- Path reconstruction
- Performance and memory benchmarking support

## Algorithms and Data Structures

This project makes use of:

- A* search
- Dijkstra's algorithm
- Priority queues / min-heaps
- Hash maps and hash sets
- Flat contiguous voxel arrays
- Graph traversal
- Connected-component search for ore clustering

## Project Goal

The goal of VoxelRoute is to demonstrate how algorithm design affects pathfinding performance in a large 3D environment.

A* and Dijkstra both solve the same weighted pathfinding problem, but A* uses a heuristic to focus its search toward the destination while Dijkstra explores solely according to accumulated path cost. Minecraft provides a practical environment for visualizing and comparing the behavior of both algorithms.