from array import array
import minescript as m

from . import constants as C
from .lib_blockpack_parser import BlockPackParser

NETHER_ORES = {
    "ancient_debris", "nether_gold_ore", "nether_quartz_ore"
}

DIMENSIONS = {
    "minecraft:overworld"  : "overworld",
    "minecraft:the_nether" : "nether",
    "minecraft:the_swift"  : "swift",
    "minecraft:the_end"    : "end",
}

BLOCK_VALUES = {
    "minecraft:bedrock"     : 9999,
    "minecraft:lava"        : 999,
    "minecraft:magma_block" : 11,
    "minecraft:gravel"      : 10,
    "minecraft:sand"        : 10,
    "minecraft:water"       : 5, 
    "minecraft:air"         : 3,  # Not bridging allow
}

class voxelGrid:
    def __init__(self, ore: str ="diamond_ore", allow_bridging: bool = False) -> None:
        dimension = DIMENSIONS.get(m.world_info().dimension, "unknown")
        extraction = self._extract_voxel_block_pack(dimension, ore)
        
        if not extraction:
            raise RuntimeError()
        
        self.parser, self.initial_pos, self.min_pos, self.max_pos = extraction
        
        self.x_length   = self.max_pos[0] - self.min_pos[0] + 1
        self.y_length   = self.max_pos[1] - self.min_pos[1] + 1
        self.z_length   = self.max_pos[2] - self.min_pos[2] + 1
        self.total_size = self.x_length * self.y_length * self.z_length
        
        # Default value for target
        BLOCK_VALUES[f"minecraft:{ore}"] = 0 
        if allow_bridging:
            BLOCK_VALUES["minecraft:air"] = 2
        
        # Initialize flat buffer with "i" (signed 32-bit int) 
        self.voxel_buffer = array(
            "i", [BLOCK_VALUES["minecraft:air"]] * self.total_size 
        )
        
    @staticmethod
    def _parse_block_name_format(block_name: str) -> str:
        return block_name.partition("[")[0].replace("deepslate_", "")
        
        
    @staticmethod
    def _extract_voxel_block_pack(
        dimension: str = "overworld", ore: str = "diamond_ore", 
        initial_r: int = 16, step: int = 16, max_r: int = 256
        ) -> tuple[BlockPackParser, tuple, tuple, tuple] | None:
        """ Extracts a voxel grid using an incremental shell expansion

        Args:
            dimension (str, optional): "overworld" or "nether". Defaults to "overworld".
            ore (str, optional): any ore using this format: "{ore_name}_ore" or "ancient_debris". 
            Example: "emerald_ore". Defaults to "diamond_ore".
            initial_r (int, optional): initial searching radius, keeps expanding if dont find any 
            ore nearby. Defaults to 16.
            step (int, optional): radius expansion value. Defaults to 16.
            max_r (int, optional): max searching radius. Defaults to 256 (16 chunks each side).

        Returns:
            tuple (parser, tuple, tuple, tuple): parser, initial position, min_pos, max_pos.
        """
        if (
            (ore in NETHER_ORES and dimension != "nether") or
            (ore not in NETHER_ORES and dimension in {"nether", "end"})
        ):
            m.echo(f"Invalid Search: There is not {ore} in the {dimension}.")
            return None
        
        bx, by, bz = map(int, m.player().position)
        initial_pos = (bx, by, bz)
        
        r = initial_r
        block_pack:m.BlockPack
        parser:BlockPackParser

        while r <= max_r:
            min_y = max(-64, by - r) if dimension == "overworld" else max(0, by - r)
            max_y = min(64, by + r) if ore == "emerald_ore" else min(250, by + r)
            
            pos1 = (bx - r, min_y, bz - r)
            pos2 = (bx + r, max_y, bz + r)

            block_pack = m.BlockPack.read_world(pos1, pos2, safety_limit=False)
            parser = BlockPackParser.parse_blockpack(block_pack)

            has_target = any(
                ore in block_name for block_name in parser.palette
            )
            
            if has_target:
                m.echo(f"Successfully extracted grid at radius r={r}")
                break
            
            if r >= max_r:
                m.echo(f"Extraction FAIL. No {ore} found in a radius of {r}")
                return None
            else:
                r += step
        
        return parser, initial_pos, pos1, pos2

    def get_index(self, x: int, y: int, z: int) -> int:
        """Gets the index into `blocks` sequence for position (x, y, z)."""

        x_index = x - self.min_pos[0]
        y_index = y - self.min_pos[1]
        z_index = z - self.min_pos[2]

        if not (0 <= x_index < self.x_length and
                0 <= y_index < self.y_length and
                0 <= z_index < self.z_length):
            raise IndexError(
                f"Block position {(x, y, z)} out of bounds for BlockRegion covering {self.min_pos} to {self.max_pos}"
            )

        return x_index + z_index * self.x_length + y_index * self.x_length * self.z_length
    

    def extract_voxel_grid(self) -> tuple[array, tuple]:
        for tile in self.parser.tiles:
            # Populate fill regions
            for fill_pos1, fill_pos2, block_id in tile.iter_fill_params():
                val = BLOCK_VALUES.get(
                    self._parse_block_name_format(self.parser.palette[block_id]), 1
                )
                x1, y1, z1 = fill_pos1
                x2, y2, z2 = fill_pos2

                for x in range(min(x1, x2), max(x1, x2) + 1):
                    for y in range(min(y1, y2), max(y1, y2) + 1):
                        for z in range(min(z1, z2), max(z1, z2) + 1):
                            if (
                                self.min_pos[0] <= x <= self.max_pos[0]
                                and self.min_pos[1] <= y <= self.max_pos[1]
                                and self.min_pos[2] <= z <= self.max_pos[2]
                            ):
                                self.voxel_buffer[self.get_index(x, y, z)] = val

            # Populate individual set_blocks
            for pos, block_id in tile.iter_setblock_params():
                val = BLOCK_VALUES.get(
                    self._parse_block_name_format(self.parser.palette[block_id]), 1
                )
                x, y, z = pos
                if (
                    self.min_pos[0] <= x <= self.max_pos[0]
                    and self.min_pos[1] <= y <= self.max_pos[1]
                    and self.min_pos[2] <= z <= self.max_pos[2]
                ):
                    self.voxel_buffer[self.get_index(x, y, z)] = val
        
        metadata = (self.initial_pos, self.min_pos, self.max_pos)
        return self.voxel_buffer, metadata

        
        
        
                
            
            
        
    
    
    
    