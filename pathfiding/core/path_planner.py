"""Path computation and glass-tunnel construction for the pathfinding script."""

from __future__ import annotations

import math
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path

import minescript as m

from . import constants as C
from . import minescript_extra as m_extra
from .voxelGridExtraction import voxelGrid

_PROJECT_ROOT = Path(__file__).resolve().parents[2]
_DIST_DIR = _PROJECT_ROOT / "dist"          # C++ binding
if str(_DIST_DIR) not in sys.path:
    sys.path.insert(0, str(_DIST_DIR))

import pathfinding


Coord = tuple[int, int, int]

DEFAULT_ORE = f"{C.MINING_ORE}_ore"
GLASS_BLOCK = "minecraft:lime_stained_glass"
COMMAND_FLUSH_INTERVAL = 128


class PathfindingError(RuntimeError):
    """Raised when a path request have a problem."""


@dataclass(frozen=True)
class PathResult:
    algorithm:str
    start:Coord
    end:Coord
    path:tuple[Coord, ...]
    min_y:int
    elapsed_seconds:float



# ------------------------------------------------------------------
# Pathfinding
# ------------------------------------------------------------------
def _as_coord(value) -> Coord:
    if len(value) != 3:
        raise PathfindingError(f"Expected 3 coordinates (x, y, z), but got {value!r}")
    return (int(value[0]), int(value[1]), int(value[2]))


def _in_bounds(position:Coord, min_pos:Coord, max_pos:Coord) -> bool:
    return all(
        min_pos[axis] <= position[axis] <= max_pos[axis]
        for axis in range(3)
    )


def format_coord(position: Coord) -> str:
    return f"({position[0]}, {position[1]}, {position[2]})"


def _create_pathfinder(ore:str = DEFAULT_ORE):
    voxel = voxelGrid(ore=ore)
    voxel_buffer, metadata = voxel.extract_voxel_grid()

    initial_pos, min_pos, max_pos = map(_as_coord, metadata)
    
    grid = pathfinding.VoxelGrid(
        voxel_buffer,
        (initial_pos, min_pos, max_pos),
    )

    return grid, voxel_buffer, min_pos, max_pos


def find_path(
    algorithm:str,
    start:Coord,
    target:Coord | None = None,
    *,
    ore:str = DEFAULT_ORE,
) -> PathResult:
    """Compute a path from ``start`` to ``target`` or the best ore cluster."""
    if algorithm not in {"astar", "dijkstra"}:
        raise PathfindingError(f"Unknown algorithm: {algorithm}")

    start = _as_coord(start)

    m.echo(f"{m_extra.txt_clr('g')}Extracting {ore} voxel grid...")
    grid, _, min_pos, max_pos = _create_pathfinder(ore=ore)

    if not _in_bounds(start, min_pos, max_pos):
        raise PathfindingError(
            f"Start {format_coord(start)} is outside the voxel grid "
            f"{format_coord(min_pos)}..{format_coord(max_pos)}"
        )

    # This also preprocesses hazards and returns cluster center (target)
    cluster_center = grid.extractPreProcessVoxel()

    if not target:
        if not cluster_center:
            raise PathfindingError(f"No {ore} cluster found in the extraction radius")
        end = _as_coord(cluster_center)
        m.echo(
            f"{m_extra.txt_clr('g')}Using best {ore} cluster center: "
            f"{m_extra.txt_clr('a')}{format_coord(end)}"
        )
    else:
        end = _as_coord(target)

    if not _in_bounds(end, min_pos, max_pos):
        raise PathfindingError(
            f"Target {format_coord(end)} is outside the voxel grid "
            f"{format_coord(min_pos)}..{format_coord(max_pos)}"
        )

    m.echo(
        f"{m_extra.txt_clr('g')}Searching with {algorithm.upper()}: "
        f"{m_extra.txt_clr('a')}{format_coord(start)} -> {format_coord(end)}"
    )

    started_at = time.perf_counter()
    if algorithm == "astar":
        raw_path = grid.AStart(start, end)
    else:
        raw_path = grid.Dijkstra(start, end)
    elapsed = time.perf_counter() - started_at

    if not raw_path:
        raise PathfindingError(
            f"No path found from {format_coord(start)} to {format_coord(end)}"
        )

    path = tuple(_as_coord(node) for node in raw_path)

    if path[0] != start or path[-1] != end:
        raise PathfindingError("Pathfinder returned an invalid endpoint")

    return PathResult(
        algorithm=algorithm,
        start=start,
        end=end,
        path=path,
        min_y=min_pos[1],
        elapsed_seconds=elapsed,
    )


# ------------------------------------------------------------------
# Minecraft CLI for Path Generation 
# ------------------------------------------------------------------

def _diagonal_clearance_cell(previous:Coord, current:Coord) -> Coord | None:
    """Adds extra sweep block for diagonal up / down movements"""
    px, py, pz = previous
    cx, cy, cz = current

    if cx == px and cz == pz:
        return None
    if cy > py:
        return (px, py + 2, pz)
    if cy < py:
        return (cx, py + 1, cz)
    return None


def iter_glass_cells(path:tuple[Coord, ...]):
    seen:set[Coord] = set()

    for i, (x, y, z) in enumerate(path):
        cells = [(x, y, z), (x, y + 1, z)]

        if i + 1 < len(path):
            clearance = _diagonal_clearance_cell((x, y, z), path[i + 1])
            if clearance is not None:
                cells.append(clearance)

        for cell in cells:
            if cell in seen:
                continue
            seen.add(cell)
            
            # Hold value until it is ask 
            yield cell


def iter_glass_commands(path:tuple[Coord, ...]):
    """Yield path generator commands"""
    for x, y, z in iter_glass_cells(path):
        yield f"setblock {x} {y} {z} {GLASS_BLOCK} replace"


def build_glass_path(
    path:tuple[Coord, ...],
    wait_seconds: float = 0.0,
    *,
    stop_event: threading.Event | None = None,
) -> int:
    """Build the path in-game, returning the number of commands."""
    if wait_seconds < 0:
        raise ValueError("wait_seconds cannot be negative")

    count = 0
    for command in iter_glass_commands(path):
        if stop_event is not None and stop_event.is_set():
            break

        m.execute(command)
        count += 1

        if wait_seconds > 0:
            m.flush()
            if stop_event is not None:
                stop_event.wait(wait_seconds)
            else:
                time.sleep(wait_seconds)
        elif count % COMMAND_FLUSH_INTERVAL == 0:
            m.flush()

    m.flush()
    return count
