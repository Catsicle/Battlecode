#include "../../../src/helper.hpp"

namespace {

unswbc::Direction scripted_direction(int round, int dragon_id, unsigned seed) {
    unsigned value = (seed + static_cast<unsigned>(round * 3 + dragon_id * 5)) % 4U;
    switch (value) {
    case 0: return unswbc::Direction::NORTH;
    case 1: return unswbc::Direction::EAST;
    case 2: return unswbc::Direction::SOUTH;
    default: return unswbc::Direction::WEST;
    }
}

} // namespace

int main() {
    auto [controller, game] = unswbc::init();
    while (unswbc::update(controller, game)) {
        int round = game.get_round_num() - 1;
        controller.make_move(scripted_direction(round, controller.get_id(), 0));
        unswbc::end_turn();
    }
}
