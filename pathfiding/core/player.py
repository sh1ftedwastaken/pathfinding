import time
import math
import threading

import minescript as m
import minescript_extra as m_extra
import constants as C

class PlayerTracker:
    def __init__(self):
        self.health = 20
        
        self.x, self.y, self.z = map(math.floor, m.player().position)
        self.yaw, self.pitch = C.YAW_FACING_EAST, C.PITCH_LOOK_AHEAD
        self.x_vel, self.y_vel, self.z_vel = 0, 0, 0
        self._last_pos = (self.x, self.y, self.z)
        
        self.targeted_block = ""
        self.targeted_entity = None
        self.main_hand_item:str = ''
        self.off_hand_item:str = ''
        
        self.lava_around = set()
        self._restart = False
        self.stop_tracking = False
        
        self._last_time_movement = time.time()
        self.auto_tracking = False
        
        #Threads
        self._player_info_update_thread = threading.Thread(target=self.player_info) 
        self._tool_update_thread = threading.Thread(target=self.tool_in_main_hand)
        # self._hazard_detection_update_thread = threading.Thread(target=self.hazard_detection)
        # self._auto_stop_update_thread = threading.Thread(target=self.auto_stop_restart)
        self._player_info_update_thread.daemon = True
        self._tool_update_thread.daemon = True
        # self._hazard_detection_update_thread.daemon = True
        # self._auto_stop_update_thread.daemon = True
        self._player_info_update_thread.start()
        self._tool_update_thread.start()
        # self._hazard_detection_update_thread.start()
        # self._auto_stop_update_thread.start()
    
    
    def player_info(self):
        while (not self.stop_tracking):

            self.health = m.player().health
            
            self.x, self.y, self.z = map(math.floor, m.player().position)
            
            yaw = ((m.player().yaw + 180) % 360) - 180
            pitch = ((m.player().pitch + 90) % 180) - 90

            if -135 <= yaw < -45: self.yaw = C.YAW_FACING_EAST
            elif -45 < yaw <= 45: self.yaw = C.YAW_FACING_SOUTH
            elif 45 < yaw < 135: self.yaw = C.YAW_FACING_WEST
            else: self.yaw = C.YAW_FACING_NORTH
            self.pitch = pitch

            self.x_vel, self.y_vel, self.z_vel = map(
                lambda v: round(v, 5), (v for v in m.player().velocity)
                )
            
            current_pos = (self.x, self.y, self.z)
            if current_pos != self._last_pos:
                self._last_time_movement = time.time()
                self._last_pos = current_pos
            
            time.sleep(C.ONE_TICK_TIME / 10)
            
            
    def tool_in_main_hand(self):
        while (not self.stop_tracking):
            time.sleep(C.ONE_TICK_TIME / 10)
            
            if (not self.auto_tracking):
                continue
            
            self.targeted_block = m.player_get_targeted_block(5)
            
            main_hand_item = m.player_hand_items().main_hand
            off_hand_item = m.player_hand_items().off_hand
            
            self.main_hand_item = main_hand_item.item if main_hand_item else ''
            self.off_hand_item = off_hand_item.item if off_hand_item else ''
                
            
            if self.main_hand_item:
                if self.main_hand_item in C.HAND_ITEMS:
                    continue

                if m.player_get_targeted_block(5):
                    try:
                        if m.player_get_targeted_block()[3] in C.SHOVEL_BREAKABLE: 
                            m.player_inventory_select_slot(C.SHOVEL_SLOT)
                        else: 
                            m.player_inventory_select_slot(C.PICKAXE_SLOT)
                    except Exception as e:
                        m.echo(f"{m_extra.txt_clr('r')}Error: {e}")
                        continue
                    
                    
                
    # def hazard_detection(self):
    #     while (not self.stop_tracking):
    #         self.targeted_entity = m.player_get_targeted_entity(5)
            
    #         px, py, pz = self.x, self.y, self.z
    #         pos1 = (px - 1, py - 1, pz -1)
    #         pos2 = (px + 1, py + 2, pz + 1)
            
    #         m.await_loaded_region(px - 2, pz - 2, 
    #                               px + 2, pz + 2)
    #         region = m.get_block_region(pos1, pos2)
            
    #         for dx, dy, dz in C.BLOCKS_AROUND_PLAYER:
     
    #                     x, y, z = px + dx, py + dy, pz + dz
                        
    #                     block = region.get_block(x, y, z)
    #                     if block and block.startswith("minecraft:lava"):
    #                         self.lava_around.add((x, y, z))

    #         time.sleep(C.ONE_TICK_TIME)


    # def auto_stop_restart(self):
        
    #     def stop() -> None:
    #         m.echo(f"{m_extra.txt_clr('y')}\nSTOPPING SCRIPT\n")
    #         m_extra.toggle_all(False)

    #         self.stop_tracking = True
    #         self._player_info_update_thread.join(timeout=1)
    #         self._tool_update_thread.join(timeout=1)
    #         self._hazard_detection_update_thread.join(timeout=1)
            
    #         m_extra.kill_jobs()
        
    #     while (not self.stop_tracking):
    #         time.sleep(C.ONE_TICK_TIME)
            
    #         if (not self.auto_tracking):
    #             continue
            
    #         time_stuck = time.time() - self._last_time_movement
            
    #         if (self.targeted_entity) and (not self.targeted_entity.name.endswith("Gravel")):
    #             m.echo(f"{m_extra.txt_clr('g')}\nENTITY DETECTED: {m_extra.txt_clr('p')}{self.targeted_entity.name}\n")
    #             stop()
                
    #         elif (self.lava_around):
    #             m.echo(f"{m_extra.txt_clr('r')}\nVERY CLOSE TO LAVA\n")
    #             safety.lavaSave()
    #             stop()
            
    #         elif (self.main_hand_item) and (self.main_hand_item.endswith("sword") or \
    #              self.main_hand_item.endswith("_axe")):
    #             m.echo(f"{m_extra.txt_clr('r')}\nPLAYER PULLET OUT HIS COMBAT WEAPON\n")
    #             stop()
                
    #         elif (self.health <= C.MIN_HEALTH):
    #             m.echo(f"{m_extra.txt_clr('r')}\nPLAYER HAS LOW HEALTH... RECOVER AND TRY AGAIN\n")
    #             stop()
            
    #         elif (time_stuck > C.STUCK_TIMEOUT * 2.5):
    #             if (not self._restart):
    #                 continue
    #             if self.targeted_block and self.targeted_block.type.endswith(f"{C.MINING_ORE}_ore"):
    #                 self._last_time_movement = time.time()
    #                 continue
                
    #             restart()
    #             self.stop_tracking = True