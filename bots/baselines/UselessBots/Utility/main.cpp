#include "helper.hpp"

#include <algorithm>
#include <random>
#define LIVE_PEARL 1
#define DEAD_PEARL 0
#define KELP -1 //Divide by distance?
#define OWN_BODY -0.5 // Multiply by number of nearby body in 3x3
#define ENEMY_HEAD = -5  // Divide by distance
#define ENEMY_BODY KELP // Divide by distance
#define PORTAL 0
#define ALLY_HEAD ENEMY_BODY //Divide by distance
#define ALLY_BODY ENEMY_BODY // Divide by distance 
// USE Manhattan Distance
#define NEG_SCORE -INFINITY
#define POS_SCORE INFINITY
#define DEPTH 12

// MAX_SIZE = GRID_SIZE/4 (MINIMUM 5) SPLIT INTO 2 and SIZE - 2 
// Game Tree Function : 

//utlity function : assign weights like this: 
// LIVE_PEARL / (every_live_pearl_distance) 
// + DEAD_PEARL / (every_dead_pearl_distance)
// + KELP / (every_kelp_distance)
// + (number_of_own_body_cells_in_3*3_radius)/8 * OWN_BODY 
// + ENEMY_HEAD / (every_enemy_head_distance)  
// + ENEMY_BODY/(every_enemy_body_cell_distance) 
// + (every_portal_distance)/14 * PORTAL
// + ALLY_HEAD / (every_ally_head_distance)
// + ALLY_BODY / (every_ally_body_distance)
// ALL DISTANCE IS MANHATTAN DISTANCE
int main()
{
    auto [ct, game] = unswbc::init();

    // Seed so we get the same random generator every time.
    std::mt19937 shuffler(0);

    while (unswbc::update(ct, game))
    {
        // Step onto the first open neighbouring tile.
        auto const here = ct.get_position();
        auto const* hereTile = ct.get_tile(here);
        bool moved = false;

        auto directions = unswbc::Direction::get_direction_list();
        std::shuffle(directions.begin(), directions.end(), shuffler);

        for (auto const direction : directions)
        {
            if (hereTile && !hereTile->get_edge(direction).is_passable())
            {
                continue;
            }

            auto const* ahead = ct.get_tile(here.add_dir(direction));
            if (ahead && ahead->get_dragon())
            {
                continue;
            }

            ct.output_log("Moving in", direction);
            ct.make_move(direction);
            moved = true;
            break;
        }

        if (!moved)
        {
            ct.make_move(unswbc::Direction::NORTH);
        }
        unswbc::end_turn();
    }
}
