#include <avm.h>
#include "app_state.hpp"
#include "game.hpp"
#include "persistence.hpp"
#include "render.hpp"
#include "ui.hpp"
#if defined(ARDUROGUE2_BENCH)
#include "../bench/bench.hpp"
#endif

using namespace rogue;

extern "C" int main()
{
    avm_set_text_font(AVM_FONT_BR5D);
#if defined(ARDUROGUE2_BENCH)
    bench_select();
    bench_setup();
    ui = {};
    ui.mode = PLAY;
#else
    ui.has_save = load_saved_game();
    ui.mode = TITLE;
    session.repeat_slot = NONE;
#endif
    ui.dirty = true;
    render();

    for(;;) {
        // Sleep until an interrupt, then process button edges or a held repeat.
        avm_idle();
        uint8_t buttons = avm_buttons();
        if(buttons != ui.previous_buttons ||
           (ui.held_direction && !ui.repeat_suppressed &&
            static_cast<int16_t>(avm_millis() -
               ui.next_repeat_ms) >= 0)) {
            InputAction input = handle_input(buttons);
            if(input == INPUT_MOVED) {
                prompt_ground_items();
                prompt_stairs();
            } else if(input >= INPUT_PROJECTILE_UP && input <= INPUT_PROJECTILE_LEFT) {
                int8_t dx = input == INPUT_PROJECTILE_RIGHT ? 1 : input == INPUT_PROJECTILE_LEFT ? -1 : 0;
                int8_t dy = input == INPUT_PROJECTILE_UP ? -1 : input == INPUT_PROJECTILE_DOWN ? 1 : 0;
                if(!throw_or_shoot(ui.selection, dx, dy)) ui.mode = PROJECTILE_DIRECTION;
            } else if(input >= INPUT_WAND_UP &&
                      input <= INPUT_WAND_IMMEDIATE) {
                int8_t dx = input == INPUT_WAND_RIGHT ? 1 :
                            input == INPUT_WAND_LEFT ? -1 : 0;
                int8_t dy = input == INPUT_WAND_UP ? -1 :
                            input == INPUT_WAND_DOWN ? 1 : 0;
                if(!use_wand(ui.selection, dx, dy) &&
                   input != INPUT_WAND_IMMEDIATE)
                    ui.mode = WAND_DIRECTION;
            }
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
