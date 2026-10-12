import minescript as m

def searchOresLava(r=16, step=4, ore=C.MINING_ORE, caption=True) -> tuple[set[tuple], set[tuple], set[tuple]]:
    sx, sy, sz = player.x, player.y, player.z
    
    prev_r = 0    
    ores_coords = set()
    lava_coords = set()
    region_coord = set()
    min_y = C.INVALID_Y_LEVEL[0] + 1
    
    
    while True:
        min_dy = max(min_y - sy, - r)
        pos1 = (sx - r, min_y, sz - r)
        pos2 = (sx + r, sy + r, sz + r)
        
        m.await_loaded_region(sx - r, sz - r, 
                              sx + r, sz + r)
        
        region = m.get_block_region(pos1, pos2)
        
        for dx in range(-r, r + 1):
            for dy in range(min_dy, r + 1):
                for dz in range(-r, r + 1):
                    
                    if max(abs(dx), abs(dy), abs(dz)) <= prev_r: # Shell expansion
                        continue
                    
                    coord = (x, y, z) = (sx + dx, sy + dy, sz + dz)
                    block = region.get_block(x, y, z)
                    region_coord.add(coord)
                    
                    if not block: continue
                    
                    if block.endswith(f"{ore}_ore"):
                        ores_coords.add(coord)
                    elif (block.startswith("minecraft:lava")) and \
                         (C.Y_LEVEL_LAVA_CHECK[0] <= y <= C.Y_LEVEL_LAVA_CHECK[1]):
                        lava_coords.add(coord)
                        
        if ores_coords:
            return ores_coords, lava_coords, region_coord

        if r >= C.MAX_SEARCHING_RADIUS:
            player._restart = True
        
        prev_r = r
        r += step
        if caption: 
            m.echo(f"{m_extra.txt_clr('y')}No {ore}s nearby, increasing searching radius to a offset of {m_extra.txt_clr('p')}{r}")