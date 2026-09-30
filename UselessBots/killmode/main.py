import helper as unswbc
from helper import Direction, EdgeType
import random

ct: unswbc.Controller
game: unswbc.Game

# Seed so we get the same random generator every time.
random.seed(0)

def generate_map():
    
def execute_turn() -> None:
    """Step onto the first open neighbouring tile."""
    here = ct.get_position()
    here_tile = ct.get_tile(here)
    direction = Direction.NORTH


    
    
    ct.make_move(direction)
        

  

def main() -> None:
    global ct, game
    ct, game = unswbc.init()

    while unswbc.update(ct, game):
        execute_turn()
        unswbc.end_turn()

if __name__ == "__main__":
    main()
