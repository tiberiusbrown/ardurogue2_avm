#include "explorer_observation.hpp"
#include "game.hpp"
#include "world.hpp"
#include "game_internal.hpp"
#include "app_state.hpp"
#include "render.hpp"
#include <cstring>
#include <iostream>
uint8_t __avm_framebuffer[1024]{};
void (*avm_test_text_hook)(int16_t,int16_t,const char*)=nullptr;
uint8_t avm_test_buttons[16]{};
uint8_t avm_test_button_count=0,avm_test_button_index=0;
unsigned avm_test_displays=0;
uint16_t avm_test_millis=0;
namespace rogue { Game game{}; Ui ui{}; }
int main() {
    using namespace rogue;
    unsigned cases=0;
    for(uint16_t seed=1;seed<=32;++seed) {
        start_new(seed); Game base=game;
        for(int sample=0;sample<8;++sample) for(int radius=0;radius<=6;++radius) {
            game=base;
            if(sample) game.player={uint8_t(sample==7 ? 0 : (seed*7+sample*11)%64),uint8_t(sample==7 ? 0 : (seed+sample*3)%32)};
            std::memset(game.explored,0,sizeof game.explored);
            game.offhand_slot=game.amulet_slot=game.ring_slots[0]=game.ring_slots[1]=NONE;
            if(radius==0) game.blindness=1;
            else if(radius<=4) { game.inventory[0]={uint8_t(radius==2 ? CANDLE : radius==3 ? TORCH : LANTERN),100}; if(radius>1) game.offhand_slot=0; }
            else { game.inventory[0]={RING_ILLUMINATION,uint8_t(1|ITEM_IDENTIFIED)}; game.ring_slots[0]=0; game.inventory[1]={uint8_t(radius==5 ? TORCH : LANTERN),100}; game.offhand_slot=1; }
            auto before=game; auto o=sim::observe_explorer(0,0);
            if(std::memcmp(&before,&game,sizeof game)) { std::cerr<<"observer mutation\n"; return 1; }
            render_play();
            for(int n=0;n<2048;++n) if(bool(game.explored[n>>3]&(1u<<(n&7)))!=o.visible[n]) {
                std::cerr<<"renderer mismatch seed="<<seed<<" sample="<<sample<<" radius="<<radius<<" tile="<<n<<'\n'; return 1;
            }
            ++cases;
        }
    }
    std::cout<<cases<<" observer/production renderer masks agree\n";
}
