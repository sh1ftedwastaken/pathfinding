import time
from typing import Iterable

import minescript as m
import bot.core.constants as C

def txt_clr(color:str = '') -> str:
    """
    Docstring for text_color
    
    :param color: '0' = black, 'a' = aqua,'b' = blue 'r' = red, 'g' = green, 'y' = yellow, 'p' = purple
    :type color: str
    """
    if (not color):
        return ""
    match color[0]:
        case '0': return "§0"
        case 'a': return "§b"
        case 'r': return "§c"
        case 'g': return "§a"
        case 'y': return "§e"
        case 'b': return "§9"
        case 'p': return "§d"
        case  _ : return "§f"
        


def select_slot(slot:int ,items:Iterable[str]) -> None:
    """
    Makes 100% sure that slot item is selected in the game
    
    :param slot: player slot from 0 to 8
    :type slot: int
    :param item_name: items selected (complete item name)
    :type item_name: Iterable[str]
    """
    
    while ((not m.player_hand_items().main_hand) or \
           (m.player_hand_items().main_hand.get("item") not in items)):
        m.player_inventory_select_slot(slot)
        time.sleep(C.ONE_TICK_TIME)
        
        
def tap_key(action_key, t=C.ONE_TICK_TIME) -> None:
    action_key(True)
    time.sleep(t)
    action_key(False)
    

def eat_food() -> None:
    m.player_inventory_select_slot(C.FOOD_SLOT)
    
    if m.player_hand_items() and (m.player_hand_items().main_hand.get("item") in C.FOOD_ITEMS):
        m.player_press_use(True)
    else: m.echo(f"{txt_clr('y')}\nNO food in hotbar\n")
    time.sleep(C.ONE_TICK_TIME * 34)
    

def kill_jobs(all=False):
    if all: m.echo("Including main script")
    running_scripts = m.job_info()
    
    for job in running_scripts:
        if not(all) and (job.command == ["bot\\main"]): 
            continue
        m.execute(f"\\killjob {job.job_id}")


def toggle_all(boolean=False):
    m.player_press_attack(boolean)
    m.player_press_use(boolean)
    m.player_press_sprint(boolean)
    m.player_press_sneak(boolean)
    m.player_press_jump(boolean)
    m.player_press_forward(boolean)
    m.player_press_backward(boolean)
    m.player_press_left(boolean)
    m.player_press_right(boolean)
    

def _help():
    m.echo(f"{txt_clr('y')}Build-in commands:")
    m.echo(f"{txt_clr('y')}\\Usage: bot <mode>")
    m.echo(f"{txt_clr('y')}\tdescend")
    m.echo(f"{txt_clr('y')}\tauto")
    m.echo(f"{txt_clr('y')}\tscan")
    m.echo(f"{txt_clr('y')}\trestart")
    m.echo(f"{txt_clr('y')}\tstop")

