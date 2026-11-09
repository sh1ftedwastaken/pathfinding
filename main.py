"""Minescript entry point for the pathfinding project.

Launch with: \\pathfinding\\main

Commands:
    .pathfind astar
    .pathfind dijkstra 1.5
    .pathfind astar (-10, -60, 100)
    .pathfind help
    .pathfind stop
    .pathfind stop all
"""

from __future__ import annotations

import math
import re
import sys
import threading
from dataclasses import dataclass
from pathlib import Path


_PROJECT_ROOT = Path(__file__).resolve().parent
_CORE_PARENT = _PROJECT_ROOT / "pathfiding"
_DIST_DIR = _PROJECT_ROOT / "dist"

for _path in (_CORE_PARENT, _DIST_DIR):
    if str(_path) not in sys.path:
        sys.path.insert(0, str(_path))

import minescript as m  

from core import minescript_extra as m_extra  
from core.path_planner import (  
    PathfindingError,
    PathResult,
    build_glass_path,
    find_path,
    format_coord,
)
from core.player import Player, get_player  


PREFIX = ".pathfind"

_COORD_RE = re.compile(
    r"\(\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\)"
)


@dataclass(frozen=True)
class PathfindRequest:
    algorithm: str
    wait_seconds: float
    target: tuple[int, int, int] | None


_STOP_EVENT = threading.Event()
_BUILD_LOCK = threading.Lock()


def parse_request(message: str) -> PathfindRequest:
    """Parse a ``.pathfind`` message, raising ValueError on bad input."""
    text = message.strip()
    if text.lower().startswith(PREFIX):
        text = text[len(PREFIX):].strip()

    if not text:
        raise ValueError("missing algorithm")

    target = None
    coordinate_match = _COORD_RE.search(text)
    if coordinate_match:
        target = (
            int(coordinate_match.group(1)),
            int(coordinate_match.group(2)),
            int(coordinate_match.group(3)),
        )
        text = (text[:coordinate_match.start()] + " " + text[coordinate_match.end():]).strip()

    if "(" in text or ")" in text:
        raise ValueError("target must be formatted as (x, y, z)")

    parts = text.split()
    if not parts:
        raise ValueError("missing algorithm")

    algorithm = parts.pop(0).lower()
    if algorithm not in {"astar", "dijkstra"}:
        raise ValueError(f"unknown algorithm '{algorithm}'")

    wait_seconds = 0.0
    if parts:
        try:
            wait_seconds = float(parts.pop(0))
        except ValueError as exc:
            raise ValueError("wait time must be a number") from exc

    if parts:
        raise ValueError(f"unexpected arguments: {' '.join(parts)}")

    if not math.isfinite(wait_seconds) or wait_seconds < 0:
        raise ValueError("wait time must be a finite, non-negative number")

    return PathfindRequest(
        algorithm=algorithm,
        wait_seconds=wait_seconds,
        target=target,
    )


def _echo_help() -> None:
    m.echo(f"{m_extra.txt_clr('g')}Pathfinding commands:")
    m.echo(f"{m_extra.txt_clr('y')}.pathfind astar")
    m.echo(f"{m_extra.txt_clr('y')}.pathfind dijkstra 1.5")
    m.echo(f"{m_extra.txt_clr('y')}.pathfind astar (-10, -60, 100)")
    m.echo(f"{m_extra.txt_clr('y')}.pathfind help")
    m.echo(f"{m_extra.txt_clr('y')}.pathfind stop")
    m.echo(f"{m_extra.txt_clr('y')}.pathfind stop all")


def _request_stop(*, all_jobs: bool) -> None:
    _STOP_EVENT.set()
    m_extra.toggle_all(False)

    if all_jobs:
        m.echo(f"{m_extra.txt_clr('y')}Stopping all pathfinding jobs...")
        for job in m.job_info():
            if getattr(job, "self", False):
                continue
            m.execute(f"\\killjob {job.job_id}")
    else:
        m.echo(f"{m_extra.txt_clr('y')}Stopping pathfinding...")


def _echo_benchmark(result: PathResult) -> None:
    algorithm_label = "3D A*" if result.algorithm == "astar" else "3D Dijkstra's"

    m.echo("===================================")
    m.echo("[3D VOXEL PATHFINDING ENGINE]")
    m.echo("===================================")
    m.echo(f"Enter Start Coord: {format_coord(result.start)}")
    m.echo(f"Enter Target Coord: {format_coord(result.end)}")
    m.echo(f"Selected Algorithm: {algorithm_label}")
    m.echo("--------------------------------------------")
    m.echo(
        f"[+] Loading {result.radius} blocks NSEW via pybind11... "
        f"OK ({result.voxel_count:,} voxels loaded)"
    )
    m.echo("[+] Validating Voxel Grid & Hazards... OK")
    m.echo("[+] Running Algorithm...")
    m.echo("--------------------------------------------")
    m.echo(f"Execution Time: {result.elapsed_seconds * 1000:.1f} ms")
    m.echo(f"Nodes Expanded: {result.nodes_expanded:,}")
    m.echo(f"Path Length: {len(result.path)} blocks")
    m.echo("Status: SUCCESS")
    m.echo("===================================")


def _run_request(request: PathfindRequest, player: Player) -> None:
    try:
        result = find_path(
            request.algorithm,
            player.position,
            request.target,
            benchmark=request.wait_seconds > 0,
        )

        m.echo(
            f"{m_extra.txt_clr('g')}{result.algorithm.upper()} path: "
            f"{m_extra.txt_clr('p')}{len(result.path)} nodes "
            f"{m_extra.txt_clr('g')}in {result.elapsed_seconds:.3f}s "
            f"{m_extra.txt_clr('a')}"
            f"({format_coord(result.start)} -> {format_coord(result.end)})"
        )

        m.echo(f"{m_extra.txt_clr('g')}Path coordinates:")
        for index in range(0, len(result.path), 8):
            chunk = " ".join(
                format_coord(node)
                for node in result.path[index:index + 8]
            )
            m.echo(f"{m_extra.txt_clr('a')}{chunk}")

        if _STOP_EVENT.is_set():
            if request.wait_seconds > 0:
                _echo_benchmark(result)
            return

        issued = build_glass_path(
            result.path,
            wait_seconds=request.wait_seconds,
            stop_event=_STOP_EVENT,
        )

        if _STOP_EVENT.is_set():
            m.echo(f"{m_extra.txt_clr('y')}Path building stopped")
        else:
            m.echo(f"{m_extra.txt_clr('g')}Built path with {issued} setblock commands")

        if request.wait_seconds > 0:
            _echo_benchmark(result)

    except PathfindingError as exc:
        m.echo(f"{m_extra.txt_clr('r')}{exc}")
    except Exception as exc:
        m.echo(f"{m_extra.txt_clr('r')}Unexpected pathfinding error: {exc}")
    finally:
        _BUILD_LOCK.release()


def _handle_message(message: str, player: Player) -> None:
    text = message.strip()
    if not text.lower().startswith(PREFIX):
        return

    body = text[len(PREFIX):].strip()
    body_lower = body.lower()

    if body_lower in {"stop", "stop all"}:
        _request_stop(all_jobs=body_lower == "stop all")
        return

    if not body or body_lower in {"help", "?"}:
        _echo_help()
        return

    try:
        request = parse_request(text)
    except ValueError as exc:
        m.echo(f"{m_extra.txt_clr('r')}Invalid command: {exc}")
        m.echo(f"{m_extra.txt_clr('y')}Use '.pathfind help'")
        return

    if not _BUILD_LOCK.acquire(blocking=False):
        m.echo(f"{m_extra.txt_clr('y')}A path is already being built")
        return

    worker = threading.Thread(
        target=_run_request,
        args=(request, player),
        name="pathfinding-worker",
        daemon=True,
    )

    try:
        worker.start()
    except Exception as exc:
        _BUILD_LOCK.release()
        m.echo(f"{m_extra.txt_clr('r')}Could not start pathfinding worker: {exc}")


def _job_command(job) -> str:
    command = " ".join(str(part) for part in job.command)
    return command.replace("/", "\\").lower()


def main_running() -> bool:
    """Return True when another pathfinding main job is already active."""
    for job in m.job_info():
        if getattr(job, "self", False):
            continue

        command = _job_command(job)
        if re.search(
            r"(?:^|\\)(?:pathfiding|pathfinding)\\main(?:\.py)?$",
            command,
        ):
            m.echo(
                f"{m_extra.txt_clr('y')}"
                "Pathfinding main is already running"
            )
            return True

    return False


def main() -> None:
    _STOP_EVENT.clear()
    player = get_player()

    m.echo(f"{m_extra.txt_clr('g')}Pathfinding ACTIVATED")
    m.echo(f"{m_extra.txt_clr('g')}Use '.pathfind help' for commands")

    try:
        with m.EventQueue() as events:
            events.register_outgoing_chat_interceptor(prefix=PREFIX)

            while not _STOP_EVENT.is_set():
                event = events.get()

                if event.type == m.EventType.OUTGOING_CHAT_INTERCEPT:
                    _handle_message(event.message, player)
    finally:
        _STOP_EVENT.set()
        m_extra.toggle_all(False)
        player.stop()


if __name__ == "__main__":
    if not main_running():
        main()
