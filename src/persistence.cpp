#include "persistence.hpp"
#include <string.h>
#if defined(__AVM__)
#include <avm.h>
#endif

namespace rogue {

bool restore_startup_save(bool loaded)
{
    if(loaded && game.magic == SAVE_MAGIC &&
       game.version == SAVE_VERSION)
        return game.valid != 0;
    memset(&game, 0, sizeof(game));
    return false;
}

#if defined(__AVM__)
bool load_saved_game()
{
    return restore_startup_save(avm_save_exists() && avm_load());
}

void invalidate_saved_game()
{
    game.valid = 0;
    avm_save();
}

void resume_saved_game()
{
    invalidate_saved_game(); // A crash cannot reload the run.
    game.valid = 1;
}

void save_resumable_game()
{
    game.valid = 1;
    avm_save();
}

void save_finished_game()
{
    avm_save();
}
#endif

} // namespace rogue
