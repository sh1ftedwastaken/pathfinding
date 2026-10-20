from core.lib_blockpack_parser import BlockPackParser
from minescript import BlockPack, echo
# import minescript as m
# from core.voxelGridExtraction import voxelGrid

# gridClass = voxelGrid()
# grid = gridClass.extract_voxel_grid()

pos1 = (-1, -60, -1)
pos2 = (1, -58, 1)

blockpack = BlockPack.read_world(pos1, pos2)
parser = BlockPackParser.parse_blockpack(blockpack)

echo(f"Palette: {parser.palette}")
echo("--- Printing All Coordinates and Block Names ---")

count = 0
for tile in parser.tiles:
  # 1. Unpack and print every coordinate inside the fill volumes
  for fill_pos1, fill_pos2, palette_idx in tile.iter_fill_params():
    block_name = parser.palette[palette_idx]
    x1, y1, z1 = fill_pos1
    x2, y2, z2 = fill_pos2

    # Loop inclusively through the 3D box to print every individual coordinate
    for x in range(min(x1, x2), max(x1, x2) + 1):
      for y in range(min(y1, y2), max(y1, y2) + 1):
        for z in range(min(z1, z2), max(z1, z2) + 1):
          echo(f"Fill Coord ({x}, {y}, {z}): {block_name}")
          count += 1

  # 2. Print every individual sparse block
  for pos, palette_idx in tile.iter_setblock_params():
    block_name = parser.palette[palette_idx]
    echo(f"Setblock Coord {pos}: {block_name}")
    count += 1
    
  echo(f"{count}")