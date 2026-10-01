#pragma once

#include "model.hpp"

namespace rogue {

// Compatible completed saves retain the best score, but cannot be continued.
bool restore_startup_save(bool loaded);
bool load_saved_game();
void invalidate_saved_game();
void resume_saved_game();
void save_resumable_game();
void save_finished_game();

} // namespace rogue
