#include "simulator.hpp"
#include "trace.hpp"
#include "game_internal.hpp"
#include "world.hpp"
#include <cstring>
#include <stdexcept>

namespace {
using namespace sim;
using namespace rogue;
void check(bool condition, const char* message) { if(!condition) throw std::runtime_error(message); }
void arena(int hp = 100) {
    game = {}; session = {NONE,DEATH,false}; game.run_seed = game.random_state = 123;
    game.hp = static_cast<uint8_t>(hp); game.max_hp = 100; game.level = 1; game.strength = 5;
    game.dexterity = 4; game.speed = 4; game.hunger = 220;
    game.player = {10,10}; game.up = {2,2}; game.down = {20,20};
    game.weapon_slot = game.armor_slot = game.amulet_slot = game.ring_slots[0] = game.ring_slots[1] = NONE;
}
void monster(uint8_t type, Position pos, bool stunned = false) {
    game.monsters[0] = {pos,type,monster_health(type),uint8_t(stunned ? 15 : 0),{0,0},MON_AGGRO};
}
void wand(uint8_t type, WandModifier modifier = WAND_NORMAL) {
    game.inventory[0] = {type,5}; set_wand_modifier(game.inventory[0],modifier);
}
Action choose(OmniscientAgent& agent) {
    Game before = game;
    Action action = agent.choose_action({game,0,0});
    check(std::memcmp(&before,&game,sizeof game) == 0,"v2 policy changed state/RNG");
    return action;
}
bool fire(const Action& a) { return a.kind == ActionKind::Wand && game.inventory[a.slot].type == WAND_FIRE; }
void fire_checks() {
    OmniscientAgent agent;
    arena(); monster(ORC,{13,10},true); game.monsters[0].hp = 250; wand(WAND_FIRE);
    auto a = choose(agent); check(fire(a),"normal safe fire declined");
    check(dispatch(a) && game.hp == 100,"safe fire injured player through production effects");
    arena(); agent.reset(); monster(ORC,{11,10},true); wand(WAND_FIRE);
    check(!fire(choose(agent)),"normal adjacent fire allowed");
    arena(); agent.reset(); monster(ORC,{12,10},true); wand(WAND_FIRE,WAND_POWERFUL);
    check(!fire(choose(agent)),"powerful radius-two self-fire allowed");
    for(auto modifier : {WAND_SPREADING,WAND_OVERPOWERED}) {
        arena(); agent.reset(); monster(ORC,{13,10},true); wand(WAND_FIRE,modifier);
        // Selected east ray is safe, north ray terminates at the player's tile.
        auto bit = (9*MAP_W+10); game.walls[bit>>3] |= static_cast<uint8_t>(1u << (bit&7));
        check(!fire(choose(agent)),"non-target spreading/overpowered burst ignored");
        game.inventory[1] = {RING_FIRE_IMMUNITY,1}; game.ring_slots[0] = 1;
        game.monsters[0].hp = 250;
        a = choose(agent); check(fire(a),"fire immunity did not allow unsafe geometry");
        check(dispatch(a) && game.hp == 100,"immune spreading fire caused damage");
    }
    arena(); agent.reset(); monster(ORC,{13,10},true); wand(WAND_FIRE,WAND_SPREADING);
    game.monsters[1] = {{10,9},BAT,1,15,{0,0},0};
    check(!fire(choose(agent)),"monster ending another ray caused unnoticed self-fire");
    arena(); agent.reset(); monster(ORC,{13,10},true); game.monsters[0].hp = 250;
    wand(WAND_FIRE,WAND_OVERPOWERED);
    a = choose(agent); check(fire(a),"safe overpowered fire rejected");
    check(dispatch(a) && game.hp == 100,"safe overpowered fire injured player");
    for(auto modifier : {WAND_CURSED,WAND_UNRELIABLE}) {
        arena(); agent.reset(); monster(ORC,{13,10},true); wand(WAND_FIRE,modifier);
        check(!fire(choose(agent)),"afflicted fire selected");
    }
    // Exhaustive positions/radii verify the shared helper against the original
    // coordinate inequalities, independently of the agent's safety preference.
    for(int center=0;center<=255;++center) for(int value=0;value<=255;++value)
        for(uint8_t r : {uint8_t(1),uint8_t(2)}) {
            int delta=value-center;
            bool expected=delta>=-r && delta<=r;
            Position c{static_cast<uint8_t>(center),static_cast<uint8_t>(center)};
            Position p{static_cast<uint8_t>(value),static_cast<uint8_t>(value)};
            check(square_contains(c,p,r)==expected,"shared square geometry changed production coverage");
            check(square_contains(c,{p.x,c.y},r)==expected && square_contains(c,{c.x,p.y},r)==expected,
                "shared square geometry changed one-axis coverage");
        }
}
void tactical_wands() {
    OmniscientAgent agent;
    for(uint8_t type : {uint8_t(WAND_TELEPORT),uint8_t(WAND_FORCE),uint8_t(WAND_POLYMORPH)}) {
        for(auto modifier : {WAND_NORMAL,WAND_POWERFUL,WAND_SPREADING,WAND_OVERPOWERED}) {
            arena(12); agent.reset(); monster(DRAGON,{11,10}); wand(type,modifier);
            auto a = choose(agent);
            check(a.kind == ActionKind::Wand && a.slot == 0,"severe encounter did not select tactical wand");
            check(wand_needs_direction(game.inventory[0]) ? a.dx==1 && a.dy==0 : a.dx==0 && a.dy==0,
                "tactical wand modifier dispatch wrong");
            Position before = game.monsters[0].pos;
            check(dispatch(a) && wand_charges(game.inventory[0])==4,"tactical wand bypassed production charge use");
            if(type == WAND_FORCE) check(game.monsters[0].pos != before || game.monsters[0].stun,
                "force did not actually displace/stun");
        }
        arena(); agent.reset(); monster(SNAKE,{11,10}); wand(type);
        auto a = choose(agent);
        check(a.kind != ActionKind::Wand,"tactical wand wasted on healthy trivial encounter");
        for(auto modifier : {WAND_CURSED,WAND_UNRELIABLE}) {
            arena(12); agent.reset(); monster(DRAGON,{11,10}); wand(type,modifier);
            check(choose(agent).kind != ActionKind::Wand,"afflicted tactical wand selected");
        }
    }
    // A powerful ray affects neighbors of the endpoint, not only its first hit.
    arena(12); agent.reset(); monster(DRAGON,{11,11}); wand(WAND_TELEPORT,WAND_POWERFUL);
    game.monsters[1] = {{12,10},BAT,1,0,{0,0},0};
    check(choose(agent).kind == ActionKind::Wand,"powerful teleport area missed nearby threat");
    arena(12); agent.reset(); monster(LORD,{11,10}); wand(WAND_POLYMORPH);
    check(choose(agent).kind != ActionKind::Wand,"ineffective polymorph attempted on Lord");
    arena(12); agent.reset(); monster(DRAGON,{11,10}); game.monsters[0].hp = 1; wand(WAND_POLYMORPH);
    check(choose(agent).kind != ActionKind::Wand,"polymorph replenished a nearly defeated threat");
    arena(12); monster(DRAGON,{11,10}); wand(WAND_POLYMORPH);
    std::string decision;
    for(uint16_t state : {uint16_t(1),uint16_t(123),uint16_t(0xace1),uint16_t(65535)}) {
        game.random_state = state; agent.reset(); auto a = choose(agent);
        if(decision.empty()) decision = action_text(a);
        check(action_text(a)==decision,"polymorph choice inspected future RNG");
    }
    arena(12); agent.reset(); monster(DRAGON,{11,10}); wand(WAND_FORCE);
    game.inventory[1] = {POTION_HEALING,1};
    auto a = choose(agent); check(a.kind==ActionKind::Use && a.slot==1,"healing did not precede emergency wand");
}
void retreat_and_inventory() {
    OmniscientAgent agent;
    arena(12); monster(DRAGON,{11,10});
    game.doors[0] = {{10,9}}; game.door_count = 1;
    auto a = choose(agent);
    check(a.kind==ActionKind::Move && a.dx==0 && a.dy==1 && a.goal.find("retreat")!=std::string::npos,
        "safer retreat not selected or closed-door turn mistaken for movement");
    check(monster_at(a.destination)==NONE && !blocked(a.destination.x,a.destination.y),"retreat attacks/opens a door");
    check(dispatch(a) && game.player==a.destination,"retreat bypassed production movement");
    arena(12); agent.reset(); monster(DRAGON,{11,10});
    Position previous{NONE,NONE};
    for(int step=0; step<3; ++step) {
        a=choose(agent);
        check(a.goal.find("retreat")!=std::string::npos,"bounded retreat stopped prematurely");
        check(a.destination!=previous,"retreat immediately reversed");
        // Construct successive dangerous states without predicting monster RNG.
        previous=game.player; game.player=a.destination;
        game.monsters[0].pos={static_cast<uint8_t>(game.player.x+1),game.player.y};
    }
    check(choose(agent).goal=="melee combat","retreat budget allowed indefinite kiting");
    // Crossing distance two/three must not reset the same encounter's budget.
    arena(12); agent.reset(); monster(DRAGON,{11,10});
    int wallbit=10*MAP_W+12; game.walls[wallbit>>3] |= static_cast<uint8_t>(1u<<(wallbit&7));
    for(int step=0; step<3; ++step) {
        a=choose(agent); check(a.goal.find("retreat")!=std::string::npos,"encounter retreat missing");
        game.monsters[0].pos={13,10};
        check(choose(agent).goal.find("retreat")==std::string::npos,"non-emergency state kept retreating");
        game.monsters[0].pos={11,10};
    }
    check(choose(agent).goal=="melee combat","safe threshold reset retreat budget for same hostile");
    game.monsters[0].type=NO_MONSTER;
    game.monsters[1]={{11,10},DRAGON,monster_health(DRAGON),0,{0,0},MON_AGGRO};
    check(choose(agent).goal.find("retreat")!=std::string::npos,"defeated encounter failed to reset retreat budget");
    arena(); agent.reset(); monster(SNAKE,{11,10});
    a = choose(agent); check(a.kind==ActionKind::Move && a.dx==1 && a.dy==0 && a.goal=="melee combat",
        "reasonable fight became extreme cowardice");
    for(uint8_t type : {uint8_t(WAND_FORCE),uint8_t(WAND_TELEPORT),uint8_t(WAND_POLYMORPH)}) {
        arena(); agent.reset(); for(auto& i : game.inventory) i = {SCROLL_IDENTIFY,1};
        Item useful{type,5}; game.ground[0] = {game.player,useful};
        a = choose(agent); check(a.kind==ActionKind::Take && a.slot==0,"useful tactical wand lost to worthless inventory");
        check(dispatch(a) && game.inventory[0].type==type,"tactical wand full-pack swap failed");
        a = choose(agent); check(a.kind != ActionKind::Take,"tactical wand swap immediately reversed");
    }
    arena(); agent.reset(); game.ground[0] = {game.player,{WAND_DIGGING,5}};
    check(choose(agent).kind != ActionKind::Take,"intentionally unvalued digging was acquired");
    arena(12); agent.reset(); monster(DRAGON,{11,10}); wand(WAND_DIGGING);
    check(choose(agent).kind != ActionKind::Wand,"digging used artificially in an emergency");
}
}
void check_v2_policy() {
    fire_checks(); tactical_wands(); retreat_and_inventory();
    using namespace rogue; using namespace sim;
    OmniscientAgent agent;
    arena(); monster(HOBGOBLIN,{15,10});
    game.inventory[0]=make_equipment(SPEAR,0); game.weapon_slot=0;
    game.inventory[1]=make_equipment(LONG_BOW,0); game.inventory[2]={ARROWS,20};
    auto a=choose(agent);
    check(a.kind==ActionKind::Use && a.slot==1,"bow policy missed useful approach shot");
    check(dispatch(a) && game.weapon_slot==1,"bow switch did not use production equipment turn");
    a=choose(agent); check(a.kind==ActionKind::Throw && a.slot==2,"equipped bow failed to fire visible opportunity");
    check(dispatch(a) && game.inventory[2].info==19,"sim Throw failed production arrow consumption");
    for(uint16_t rng:{1,17,300,65000}) {
        arena(); agent.reset(); monster(HOBGOBLIN,{15,10});
        game.inventory[0]=make_equipment(LONG_BOW,0); game.weapon_slot=0; game.inventory[1]={ARROWS,20};
        game.random_state=rng;
        a=choose(agent); check(a.kind==ActionKind::Throw,"bow decision inspected future RNG");
    }
    for(uint8_t type:{PHANTOM,HOBGOBLIN}) {
        arena(); agent.reset(); monster(type,{15,10});
        if(type==HOBGOBLIN) set_monster_effect(game.monsters[0],MON_INVISIBLE,10);
        game.inventory[0]=make_equipment(LONG_BOW,0); game.weapon_slot=0; game.inventory[1]={ARROWS,20};
        check(choose(agent).kind!=ActionKind::Throw,"bow policy cheated on unseen target");
        game.inventory[2]={RING_SEE_INVISIBLE,1}; game.ring_slots[0]=2;
        agent.reset(); check(choose(agent).kind==ActionKind::Throw,"legitimate see-invisible bow target rejected");
    }
    arena(); agent.reset(); monster(BAT,{15,10});
    game.inventory[0]=make_equipment(LONG_BOW,0); game.weapon_slot=0; game.inventory[1]={ARROWS,20};
    check(choose(agent).kind!=ActionKind::Throw,"bow policy wasted arrows on a Bat");
    arena(); agent.reset(); monster(HOBGOBLIN,{12,10},true);
    game.inventory[0]=make_equipment(TWO_HANDED_SWORD,7); game.weapon_slot=0;
    game.inventory[1]=make_equipment(LONG_BOW,-7); game.inventory[2]={ARROWS,20};
    a=choose(agent); check(!(a.kind==ActionKind::Use && a.slot==1),"bow equipped for a close shot it would reject next turn");
    // Full-census regressions: complementary bow/ammo slots caused reverse
    // swaps for seeds 39396/41956. Neither pickup may strand its counterpart.
    for(uint8_t bow : {uint8_t(SHORT_BOW), uint8_t(LONG_BOW)}) {
        arena(); agent.reset();
        for(auto& item:game.inventory) item={POTION_HEALING,1};
        game.inventory[0]=make_equipment(TWO_HANDED_SWORD,0); game.weapon_slot=0;
        game.inventory[1]=make_equipment(bow,0);
        game.ground[0]={game.player,{ARROWS,2}};
        a=choose(agent);
        check(!(a.kind==ActionKind::Take && a.slot==1),"ammo displaced its only useful bow");
        game.inventory[1]={ARROWS,2}; game.ground[0].item=make_equipment(bow,0);
        agent.reset(); a=choose(agent);
        check(!(a.kind==ActionKind::Take && a.slot==1),"bow displaced its last ammo");
    }
}
