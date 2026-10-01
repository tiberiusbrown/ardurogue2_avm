#include <avm.h>
#include "app_state.hpp"
#include "game.hpp"
#include "persistence.hpp"
#include "render.hpp"
#include "ui.hpp"

using namespace rogue;

extern "C" int main()
{
    avm_set_text_font(AVM_FONT_BR5D);
    ui.has_save = load_saved_game();
    ui.mode = TITLE;
    session.repeat_slot = NONE;
    ui.dirty = true;
    render();

    for(;;) {
        // Sleep until an interrupt, then process button edges or a held repeat.
        avm_idle();
        uint8_t buttons = avm_buttons();
        if(buttons != ui.previous_buttons ||
           (ui.held_direction && !ui.repeat_suppressed &&
            static_cast<int16_t>(avm_millis() -
               ui.next_repeat_ms) >= 0))
            if(handle_input(buttons)) {
                prompt_ground_items();
                prompt_stairs();
            }
        if(session.ended) {
            save_finished_game();
            ui.has_save = false;
            ui.mode = END;
            session.ended = false;
            ui.dirty = true;
        }
        if(ui.dirty)
            render();
    }
}
