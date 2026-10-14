import numpy as np

import minescript as m
import constants as C

from core import player 


def _safe_await_loaded_region(x_min, z_min, x_max, z_max) -> bool:
    """
    Waits 2 ticks (0.1s) for the batch region to be loaded.
    Allows rapid radius fallback if chunks are outside server bounds.
    """
    try:
        m.await_loaded_region.as_async(x_min, z_min, x_max, z_max).wait(timeout=C.ONE_TICK_TIME * 2)
        return True
    except TimeoutError:
        return False
    
def extract(dimension = "over_world") -> np.ndarray:
    bx, by, bz = player.x, player.y, player.z
    chunks = 16
    
    BLOCK_IDS = {
        "minecraft:lava" : 99,
        "minecraft:"
        "minecraft:water" : 2,
        "minecraft:air" : 1,
    }
    
    _safe_await_loaded_region(
        bx - C.CHUNK_SIZE * chunks, bz - C.CHUNK_SIZE * chunks, 
        bx + C.CHUNK_SIZE * chunks, bz + C.CHUNK_SIZE * chunks
    )
    
    min_y = max(by - 100, -62) if dimension == "over_world" else max(by - 100, 0)
    max_y =  min(by + 100, 60) if dimension == "over_world" else min(by + 100, 100)
    coord1 = [bx - C.CHUNK_SIZE * chunks, min_y, bz - C.CHUNK_SIZE * chunks]
    coord2 = [bx + C.CHUNK_SIZE * chunks, min(by + 100, 60), bz + C.CHUNK_SIZE * chunks]
    
    block_names = m.getblocklist([coord1, coord2])
    voxel_buffer = np.array(
        [BLOCK_IDS.get(block, 1) for block in block_names], dtype=np.int32
    ) #TODO: TEST Using large chunks
    return voxel_buffer