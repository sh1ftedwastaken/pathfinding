from __future__ import annotations

import math
import threading
import time

import minescript as m

from . import constants as C
from . import minescript_extra as m_extra


class Player:
    """Continuously tracks the local player's position and interaction state."""

    _POSITION_INTERVAL = C.ONE_TICK_TIME
    _HAND_INTERVAL = C.ONE_TICK_TIME

    def __init__(self, *, track_hand_items: bool = True) -> None:
        self.health = 20.0

        self.x = 0
        self.y = 0
        self.z = 0
        self.precise_position = (0.0, 0.0, 0.0)

        self.yaw = 0.0
        self.pitch = 0.0
        self.velocity = (0.0, 0.0, 0.0)

        self.targeted_block = None
        self.targeted_entity = None
        self.main_hand_item = ""
        self.off_hand_item = ""

        self.auto_tracking = False

        self._last_position: tuple[int, int, int] | None = None
        self._last_move_time = time.monotonic()
        self._stop_event = threading.Event()
        self._threads: list[threading.Thread] = []
        self._started = False
        self._lock = threading.Lock()

        try:
            self._sample_position()
        except Exception as exc:  # Player may not be fully loaded yet
            m.echo(f"{m_extra.txt_clr('y')}[player] initial sample failed: {exc}")

        self.start(track_hand_items=track_hand_items)


    @property
    def position(self) -> tuple[int, int, int]:
        """Floored foot position, suitable for pathfinding coordinates."""
        return (self.x, self.y, self.z)

    @property
    def head_position(self) -> tuple[int, int, int]:
        """Floored head position."""
        return (self.x, self.y + 1, self.z)

    @property
    def seconds_since_movement(self) -> float:
        return time.monotonic() - self._last_move_time

    @property
    def facing(self) -> float:
        """Nearest cardinal yaw in Minecraft's yaw convention."""
        yaw = ((self.yaw + 180) % 360) - 180

        if -135 <= yaw < -45:
            return C.YAW_FACING_EAST
        if -45 < yaw <= 45:
            return C.YAW_FACING_SOUTH
        if 45 < yaw < 135:
            return C.YAW_FACING_WEST
        return C.YAW_FACING_NORTH

    def start(self, *, track_hand_items: bool = True) -> None:
        """Start background tracking once"""
        with self._lock:
            if self._started:
                return
            self._stop_event.clear()
            self._started = True

        self._start_thread(self._track_position, "pathfinding-player-position")
        if track_hand_items:
            self._start_thread(self._track_hand_items, "pathfinding-player-hands")

    def stop(self) -> None:
        """Stop all tracking threads without touching Minescript jobs."""
        self._stop_event.set()

        for thread in self._threads:
            thread.join(timeout=1.0)

        self._threads.clear()
        self._started = False

    # ------------------------------------------------------------------
    # Background loops
    # ------------------------------------------------------------------
    def _start_thread(self, target, name: str) -> None:
        thread = threading.Thread(target=target, name=name, daemon=True)
        thread.start()
        self._threads.append(thread)

    def _track_position(self) -> None:
        while not self._stop_event.is_set():
            try:
                self._sample_position()
            except Exception as exc:  # Minescript calls can fail while loading
                m.echo(f"{m_extra.txt_clr('r')}[player] position update failed: {exc}")
                self._stop_event.wait(self._POSITION_INTERVAL * 10)
                continue

            self._stop_event.wait(self._POSITION_INTERVAL)

    def _track_hand_items(self) -> None:
        while not self._stop_event.is_set():
            if self.auto_tracking:
                try:
                    self._sample_hands()
                except Exception as exc:  # Minescript calls can fail while loading
                    m.echo(f"{m_extra.txt_clr('r')}[player] hand update failed: {exc}")
                    self._stop_event.wait(self._HAND_INTERVAL * 10)
                    continue

            self._stop_event.wait(self._HAND_INTERVAL)

    # ------------------------------------------------------------------
    # Sampling
    # ------------------------------------------------------------------
    def _sample_position(self) -> None:
        data = m.player()

        px, py, pz = (float(value) for value in data.position)
        self.precise_position = (px, py, pz)
        self.x, self.y, self.z = map(math.floor, (px, py, pz))

        self.health = float(data.health)
        self.yaw = float(data.yaw)
        self.pitch = float(data.pitch)
        self.velocity = tuple(float(value) for value in data.velocity)

        current_position = (self.x, self.y, self.z)
        if current_position != self._last_position:
            self._last_position = current_position
            self._last_move_time = time.monotonic()

    def _sample_hands(self) -> None:
        self.targeted_block = m.player_get_targeted_block(5)
        self.targeted_entity = m.player_get_targeted_entity(5)

        hands = m.player_hand_items()
        self.main_hand_item = hands.main_hand.item if hands.main_hand else ""
        self.off_hand_item = hands.off_hand.item if hands.off_hand else ""


_default_player: Player | None = None
_default_player_lock = threading.Lock()


def get_player() -> Player:
    """Return the process-wide shared Player instance."""
    global _default_player

    with _default_player_lock:
        if _default_player is None:
            _default_player = Player()
        return _default_player
