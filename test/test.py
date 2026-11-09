"""Runtime diagnostics for ore mapping and cluster selection.

Run from Minecraft:
    \\pathfinding\\test

Stand in the overworld near both normal diamond ore and deepslate diamond
ore if possible.  If that is not possible, run this once near each type and
paste both outputs.

Output is intentionally plain text prefixed with ``[ore-test]``.
"""

from __future__ import annotations

import math
import sys
import time
import traceback
from array import array
from collections import Counter
from dataclasses import dataclass
from pathlib import Path


_PROJECT_ROOT = Path(__file__).resolve().parent
_CORE_PARENT = _PROJECT_ROOT / "pathfiding"
_DIST_DIR = _PROJECT_ROOT / "dist"

for _path in (_CORE_PARENT, _DIST_DIR):
    if str(_path) not in sys.path:
        sys.path.insert(0, str(_path))

import minescript as m  # noqa: E402
import pathfinding  # noqa: E402

from core.voxelGridExtraction import (  # noqa: E402
    BLOCK_VALUES,
    DIMENSIONS,
    voxelGrid,
)


ORE = "diamond_ore"
INITIAL_RADIUS = 16
STEP = 16
MAX_RADIUS = 48
MAX_CANDIDATES_TO_PRINT = 20
MAX_PATH_CELLS = 300_000

@dataclass(frozen=True)
class Candidate:
    center: tuple[int, int, int]
    size: int
    path_cost: float
    score: float


def _echo(text: str) -> None:
    m.echo(f"[ore-test] {text}")


def _build_grid(ore: str, dimension: str) -> voxelGrid | None:
    extraction = voxelGrid._extract_voxel_block_pack(
        dimension,
        ore,
        INITIAL_RADIUS,
        STEP,
        MAX_RADIUS,
    )
    if extraction is None:
        return None

    grid = voxelGrid.__new__(voxelGrid)
    grid.parser, grid.initial_pos, grid.min_pos, grid.max_pos = extraction
    grid.x_length = grid.max_pos[0] - grid.min_pos[0] + 1
    grid.y_length = grid.max_pos[1] - grid.min_pos[1] + 1
    grid.z_length = grid.max_pos[2] - grid.min_pos[2] + 1
    grid.total_size = grid.x_length * grid.y_length * grid.z_length
    grid.block_values = dict(BLOCK_VALUES)
    grid.block_values[f"minecraft:{ore}"] = 0
    grid.voxel_buffer = array(
        "i",
        [grid.block_values["minecraft:air"]] * grid.total_size,
    )
    return grid


def _raw_palette_counts(parser) -> Counter:
    counts: Counter = Counter()

    for tile in parser.tiles:
        for pos1, pos2, block_id in tile.iter_fill_params():
            dx = abs(pos2[0] - pos1[0]) + 1
            dy = abs(pos2[1] - pos1[1]) + 1
            dz = abs(pos2[2] - pos1[2]) + 1
            counts[block_id] += dx * dy * dz

        for _pos, block_id in tile.iter_setblock_params():
            counts[block_id] += 1

    return counts


def _buffer_counts(buffer: array) -> str:
    counts = Counter(buffer)
    return (
        f"ore0={counts.get(0, 0)} "
        f"solid1={counts.get(1, 0)} "
        f"air2={counts.get(2, 0)} "
        f"air3={counts.get(3, 0)} "
        f"water5={counts.get(5, 0)} "
        f"falling10={counts.get(10, 0)} "
        f"magma11={counts.get(11, 0)} "
        f"unbreakable998={counts.get(998, 0)} "
        f"lava999={counts.get(999, 0)} "
        f"bedrock9999={counts.get(9999, 0)}"
    )


def _find_candidates(
    buffer: array,
    grid: voxelGrid,
) -> list[Candidate]:
    min_x, min_y, min_z = grid.min_pos
    max_x, max_y, max_z = grid.max_pos
    visited: set[int] = set()
    candidates: list[Candidate] = []

    for x in range(min_x, max_x + 1):
        for y in range(min_y, max_y + 1):
            for z in range(min_z, max_z + 1):
                start_idx = grid.get_index(x, y, z)
                if start_idx in visited or buffer[start_idx] != 0:
                    continue

                stack = [(x, y, z)]
                visited.add(start_idx)
                sum_x = 0
                sum_y = 0
                sum_z = 0
                size = 0

                while stack:
                    current_x, current_y, current_z = stack.pop()
                    sum_x += current_x
                    sum_y += current_y
                    sum_z += current_z
                    size += 1

                    for dx in (-1, 0, 1):
                        for dy in (-1, 0, 1):
                            for dz in (-1, 0, 1):
                                if dx == 0 and dy == 0 and dz == 0:
                                    continue

                                nx = current_x + dx
                                ny = current_y + dy
                                nz = current_z + dz

                                if not (
                                    min_x <= nx <= max_x
                                    and min_y <= ny <= max_y
                                    and min_z <= nz <= max_z
                                ):
                                    continue

                                neighbor_idx = grid.get_index(nx, ny, nz)
                                if neighbor_idx in visited or buffer[neighbor_idx] != 0:
                                    continue

                                visited.add(neighbor_idx)
                                stack.append((nx, ny, nz))

                center = (
                    int(math.floor(sum_x / size)),
                    int(math.floor(sum_y / size)),
                    int(math.floor(sum_z / size)),
                )
                candidates.append(
                    Candidate(
                        center=center,
                        size=size,
                        path_cost=math.inf,
                        score=-math.inf,
                    )
                )

    return candidates


def _score_candidates(
    candidates: list[Candidate],
    path_costs: list[float],
) -> list[Candidate]:
    scored: list[Candidate] = []

    for candidate, path_cost in zip(candidates, path_costs):
        if path_cost == 0:
            score = math.inf
        elif math.isfinite(path_cost):
            score = candidate.size / path_cost
        else:
            score = -math.inf

        scored.append(
            Candidate(
                center=candidate.center,
                size=candidate.size,
                path_cost=path_cost,
                score=score,
            )
        )

    return scored


def _select_candidate(candidates: list[Candidate]) -> Candidate | None:
    reachable = [candidate for candidate in candidates if math.isfinite(candidate.path_cost)]
    if not reachable:
        return None

    return max(
        reachable,
        key=lambda candidate: (
            candidate.score,
            candidate.size,
            tuple(-value for value in candidate.center),
        ),
    )


def _short_center(center: tuple[int, int, int]) -> str:
    return f"({center[0]}, {center[1]}, {center[2]})"


def _print_candidates(
    label: str,
    candidates: list[Candidate],
    selected: Candidate | None,
) -> None:
    reachable_count = sum(
        1 for candidate in candidates if math.isfinite(candidate.path_cost)
    )
    _echo(
        f"candidates_{label} total={len(candidates)} "
        f"reachable={reachable_count}"
    )

    if not candidates:
        _echo(f"candidates_{label} none")
        return

    to_print = candidates
    if len(to_print) > MAX_CANDIDATES_TO_PRINT:
        to_print = sorted(
            candidates,
            key=lambda candidate: (
                -candidate.score,
                -candidate.size,
                candidate.center,
            ),
        )[:MAX_CANDIDATES_TO_PRINT]
        if selected is not None and selected not in to_print:
            to_print = [selected, *to_print]

    for index, candidate in enumerate(to_print):
        is_selected = candidate is selected
        _echo(
            f"candidates_{label} #{index} "
            f"center={_short_center(candidate.center)} "
            f"size={candidate.size} "
            f"cost={candidate.path_cost:.3f} "
            f"score={candidate.score:.3f} "
            f"selected={'YES' if is_selected else 'no'}"
        )


def _in_bounds(position: tuple[int, int, int], grid: voxelGrid) -> bool:
    return all(
        grid.min_pos[index] <= position[index] <= grid.max_pos[index]
        for index in range(3)
    )


def _test_astar(
    grid: voxelGrid,
    start: tuple[int, int, int],
    end: tuple[int, int, int],
) -> str:
    if grid.total_size > MAX_PATH_CELLS:
        return (
            f"astar skipped: grid has {grid.total_size} cells "
            f"(limit {MAX_PATH_CELLS})"
        )

    if not _in_bounds(start, grid):
        return f"astar skipped: start={start} outside grid"
    if not _in_bounds(end, grid):
        return f"astar skipped: end={end} outside grid"

    path_buffer = array("i", grid.voxel_buffer)
    path_grid = pathfinding.VoxelGrid(
        path_buffer,
        (grid.initial_pos, grid.min_pos, grid.max_pos),
    )

    started_at = time.perf_counter()
    try:
        path = path_grid.AStart(start, end)
    except Exception as exc:
        return f"astar error: {exc}"
    elapsed = time.perf_counter() - started_at

    if not path:
        return (
            f"astar start={start} end={end} nodes=0 "
            f"ok=NO elapsed={elapsed:.3f}s"
        )

    return (
        f"astar start={start} end={end} nodes={len(path)} "
        f"ok=YES elapsed={elapsed:.3f}s"
    )


def _extension_status() -> str:
    header = _PROJECT_ROOT / "include" / "VoxelGrid.hpp"
    extension_file = getattr(pathfinding, "__file__", None)
    extension = Path(extension_file) if extension_file else Path("<unknown>")

    try:
        header_mtime = header.stat().st_mtime
    except OSError:
        header_mtime = 0.0

    try:
        extension_mtime = extension.stat().st_mtime
    except OSError:
        extension_mtime = 0.0

    stale = header_mtime > extension_mtime
    return (
        f"extension={extension} "
        f"header_mtime={header_mtime:.0f} "
        f"extension_mtime={extension_mtime:.0f} "
        f"header_newer_than_extension={'YES' if stale else 'no'}"
    )


def run() -> None:
    dimension = DIMENSIONS.get(m.world_info().dimension, "unknown")
    raw_player_position = m.player().position
    player_position: tuple[int, int, int] = (
        int(raw_player_position[0]),
        int(raw_player_position[1]),
        int(raw_player_position[2]),
    )

    _echo(f"project={_PROJECT_ROOT}")
    _echo(f"python={sys.version.split()[0]}")
    _echo(f"dimension={dimension} player={player_position}")
    _echo(f"ore={ORE} extraction_max_radius={MAX_RADIUS}")

    grid = _build_grid(ORE, dimension)
    if grid is None:
        _echo("extraction returned None; no target found in configured radius")
        return

    _echo(
        f"region min={grid.min_pos} max={grid.max_pos} "
        f"size={grid.total_size} initial={grid.initial_pos}"
    )

    palette = grid.parser.palette
    raw_counts = _raw_palette_counts(grid.parser)
    ore_key = f"minecraft:{ORE}"
    matching: list[tuple[int, str, str, int, int]] = []

    for palette_index, raw_name in enumerate(palette):
        normalized = grid._parse_block_name_format(raw_name)
        if normalized == ore_key or "diamond" in raw_name.lower():
            matching.append(
                (
                    palette_index,
                    raw_name,
                    normalized,
                    grid.block_values.get(normalized, 1),
                    raw_counts.get(palette_index, 0),
                )
            )

    _echo(f"ore_key={ore_key} map_value={grid.block_values.get(ore_key)}")
    _echo(f"palette_diamond_entries={len(matching)}")
    for palette_index, raw_name, normalized, mapped, raw_count in matching:
        _echo(
            f"palette idx={palette_index} "
            f"raw={raw_name!r} "
            f"norm={normalized!r} "
            f"map={mapped} "
            f"raw_volume={raw_count}"
        )

    if not matching:
        _echo("no diamond palette entries; first 20 palette entries follow")
        for palette_index, raw_name in enumerate(palette[:20]):
            _echo(f"palette idx={palette_index} raw={raw_name!r}")

    has_target_substring = any(ORE in raw_name for raw_name in palette)
    mapped_palette_entries = sum(1 for item in matching if item[3] == 0)
    _echo(
        f"has_target_substring={has_target_substring} "
        f"mapped_palette_entries={mapped_palette_entries}"
    )

    started_at = time.perf_counter()
    buffer, _metadata = grid.extract_voxel_grid()
    elapsed = time.perf_counter() - started_at
    before = array("i", buffer)
    _echo(f"extract_voxel_grid elapsed={elapsed:.3f}s")
    _echo(f"buffer_before {_buffer_counts(before)}")

    pre_grid = pathfinding.VoxelGrid(
        before,
        (grid.initial_pos, grid.min_pos, grid.max_pos),
    )
    pre_raw = _find_candidates(before, grid)
    pre_candidates = _score_candidates(
        pre_raw,
        pre_grid.DijkstraCosts(
            grid.initial_pos,
            [candidate.center for candidate in pre_raw],
        ),
    )
    pre_selected = _select_candidate(pre_candidates)
    _print_candidates("pre", pre_candidates, pre_selected)

    if pre_selected is None:
        _echo("mirror_pre_selected=none")
    else:
        _echo(
            f"mirror_pre_selected={_short_center(pre_selected.center)} "
            f"size={pre_selected.size} "
            f"cost={pre_selected.path_cost:.3f} "
            f"score={pre_selected.score:.3f}"
        )

    # The Python voxelGrid only builds the buffer.  Clustering lives on the
    # compiled pathfinding.VoxelGrid and mutates this same buffer.
    cpp_grid = pathfinding.VoxelGrid(
        grid.voxel_buffer,
        (grid.initial_pos, grid.min_pos, grid.max_pos),
    )

    started_at = time.perf_counter()
    cpp_selected = cpp_grid.extractPreProcessVoxel()
    elapsed = time.perf_counter() - started_at
    cpp_center: tuple[int, int, int] | None
    if cpp_selected is None:
        cpp_center = None
    else:
        cpp_center = (
            int(cpp_selected[0]),
            int(cpp_selected[1]),
            int(cpp_selected[2]),
        )
    _echo(f"cpp_extractPreProcessVoxel elapsed={elapsed:.3f}s")
    _echo(f"cpp_selected={cpp_center}")
    _echo(f"buffer_after {_buffer_counts(grid.voxel_buffer)}")

    if cpp_center is not None and _in_bounds(cpp_center, grid):
        center_idx = grid.get_index(*cpp_center)
        _echo(
            f"cpp_center_block before={before[center_idx]} "
            f"after={grid.voxel_buffer[center_idx]}"
        )
    elif cpp_center is not None:
        _echo(f"cpp_center={cpp_center} outside grid")

    if cpp_center is None:
        _echo("mirror_match_pre=NO")
    elif pre_selected is None:
        _echo("mirror_match_pre=NO")
    else:
        _echo(
            f"mirror_match_pre="
            f"{'YES' if cpp_center == pre_selected.center else 'NO'}"
        )

    post_raw = _find_candidates(grid.voxel_buffer, grid)
    post_candidates = _score_candidates(
        post_raw,
        cpp_grid.DijkstraCosts(
            grid.initial_pos,
            [candidate.center for candidate in post_raw],
        ),
    )
    post_selected = _select_candidate(post_candidates)
    _print_candidates("post", post_candidates, post_selected)

    if post_selected is None:
        _echo("mirror_post_selected=none")
    else:
        _echo(
            f"mirror_post_selected={_short_center(post_selected.center)} "
            f"size={post_selected.size} "
            f"cost={post_selected.path_cost:.3f} "
            f"score={post_selected.score:.3f}"
        )
        _echo(
            f"mirror_match_post="
            f"{'YES' if cpp_center == post_selected.center else 'NO'}"
        )

    if cpp_center is not None:
        _echo(_test_astar(grid, player_position, cpp_center))

    _echo(_extension_status())


def main() -> None:
    try:
        run()
    except Exception:
        _echo("ERROR")
        for line in traceback.format_exc().splitlines():
            _echo(line)


if __name__ == "__main__":
    main()
