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
    game = {}; game.offhand_slot = NONE; session = {NONE,DEATH,false}; game.run_seed = game.random_state = 123;
    game.hp = static_cast<uint8_t>(hp); game.max_hp = 100; game.level = 1; game.strength = 5;
    game.dexterity = 4; game.speed = 4; game.hunger = 220;
    game.inventory[15] = {GLOWING_ORB,0}; game.offhand_slot = 15;
    game.player = {10,10}; game.up = {2,2}; game.down = {20,20};
    game.weapon_slot = game.armor_slot = game.amulet_slot = game.ring_slots[0] = game.ring_slots[1] = NONE;
    game.inventory[14] = {AMULET_BRILLIANCE,1}; game.amulet_slot = 14;
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
    // The pickup offer uses item observations alone, before hidden entry effects.
    arena(); game.ground[0]={game.player,{YENDOR_AMULET,1}};
    auto pickup_context=DecisionContext(game,0,0);
    auto pickup_first=agent.choose_pickup(pickup_context);
    for(uint8_t type=FEATURE_SPIKE_TRAP;type<=FEATURE_RUST_TRAP;type+=2) {
        game.features[0]={{11,10},uint8_t(type|MAP_FEATURE_HIDDEN)};
        auto next=agent.choose_pickup({game,0,0});
        check(next.kind==pickup_first.kind && next.target==pickup_first.target && next.slot==pickup_first.slot,
            "pickup offer leaked hidden feature type or position");
    }
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
    {
        OmniscientAgent agent; arena(); agent.reset();
        for(auto& item:game.inventory) item={POTION_HEALING,1};
        game.inventory[0]=make_equipment(SHORT_BOW,0);
        game.ground[0]={game.player,{WAND_FIRE,10}};
        auto choice=agent.choose_pickup({game,0,0});
        check(choice.kind==ActionKind::Wait,"unarmed bow/wand reverse swap regression");
        // A substantially better wand may replace the bow, but the bow must
        // then fail the reverse acquisition comparison.
        game.ground[0].item={WAND_STRIKING,15};
        choice=agent.choose_pickup({game,0,0});
        check(choice.kind==ActionKind::Take && choice.slot==0 && dispatch(choice),"valuable wand/bow replacement failed");
        check(agent.choose_pickup({game,1,1}).kind==ActionKind::Wait,"bow reverse acquisition loop");
    }

    {
        OmniscientAgent agent;
        arena(); game.floor=4; game.has_amulet=1; game.player={10,10}; game.up={14,10}; game.down={50,30};
        std::memset(game.walls,0xff,sizeof game.walls);
        for(uint8_t x=10;x<=50;++x) { carve(x,10); carve(x,30); }
        for(uint8_t y=10;y<=30;++y) { carve(10,y); carve(50,y); }
        game.features[0]={{11,10},FEATURE_TELEPORT_TRAP | MAP_FEATURE_HIDDEN};
        auto initial=choose(agent);
        check(initial.dx==1 && initial.dy==0,"hidden teleport changed initial route");
        check(dispatch(initial) && game.features[0].id==FEATURE_TELEPORT_TRAP && game.player!=Position{11,10},"teleport activation/displacement regression");
        unsigned steps=0;
        for(;steps<200 && game.player!=game.up;++steps) {
            auto action=choose(agent);
            check(action.diagnostic==Diagnostic::None && action.kind==ActionKind::Move,"teleport detour became stuck");
            Position next{uint8_t(game.player.x+action.dx),uint8_t(game.player.y+action.dy)};
            check(next!=Position{11,10},"known teleport re-entered despite detour");
            check(dispatch(action),"teleport detour rejected move");
        }
        check(game.player==game.up && steps<200,"teleport detour failed eventual progress");
        game.player={10,10}; agent.reset();
        auto detour=choose(agent);
        check(detour.dy==1 && detour.dx==0 && Paths(true).to(game.up)>60,"finite teleport penalty won over long detour");
        // Leaving the tile remains legal, including a monster endpoint.
        game.player={11,10}; game.monsters[0]={{12,10},GOBLIN,10,0,{0,0},0};
        check(Paths(true).to({12,10})==1,"cannot leave known teleport or approach blocker");
        // Same nonempty history, differing hidden types/positions and future RNG.
        game.monsters[0]={}; game.player={10,10}; game.features[1]={{10,11},FEATURE_SPIKE_TRAP | MAP_FEATURE_HIDDEN};
        auto warm=agent; auto first=choose(warm);
        game.features[1]={{50,20},FEATURE_SUMMONING_TRAP | MAP_FEATURE_HIDDEN};
        auto other=agent; auto second=choose(other);
        check(first.kind==second.kind && first.dx==second.dx && first.dy==second.dy && first.goal==second.goal && first.destination==second.destination,"navigation history leaked hidden information");
        // A truly disconnected mandatory goal has one bounded fallback.
        arena(); game.floor=4; game.down={14,10}; game.player={10,10}; agent.reset();
        std::memset(game.walls,0xff,sizeof game.walls);
        for(uint8_t x=10;x<=14;++x) carve(x,10);
        game.features[0]={{11,10},FEATURE_TELEPORT_TRAP};
        check(Paths(true).to(game.down)<0 && Paths(true,true).to(game.down)==4,"teleport fallback connectivity");
        check(choose(agent).dx==1,"mandatory teleport fallback rejected");
    }
    {
        OmniscientAgent a;
        arena(); game.player={10,10}; game.down={14,10}; game.hp=game.max_hp=50;
        Game empty = game;
        auto normal=choose(a);
        check(normal.kind==ActionKind::Move && normal.dx==1 && normal.dy==0, "empty shortest path fixture");
        for(uint8_t type=FEATURE_SPIKE_TRAP;type<=FEATURE_RUST_TRAP;type+=2) {
            game=empty; a.reset(); game.features[0]={{11,10},uint8_t(type|MAP_FEATURE_HIDDEN)};
            auto view=observed_features(game);
            DecisionContext context(game,0,0);
            check(view[0].id==0 && view[0].pos==Position{0,0} && context.world.features[0].id==0 &&
                context.world.features[0].pos==Position{0,0}, "hidden type/position leaked through observation");
            auto hidden=choose(a);
            check(hidden.kind==normal.kind && hidden.dx==normal.dx && hidden.dy==normal.dy &&
                hidden.goal==normal.goal && hidden.destination==normal.destination, "agent routed around hidden trap");
            game.features[0].pos={12,11}; a.reset(); hidden=choose(a);
            check(hidden.dx==normal.dx && hidden.dy==normal.dy, "agent distinguished relocated hidden trap");
        }
        game=empty; a.reset(); game.features[0]={{11,10},FEATURE_SPIKE_TRAP};
        auto discovered=choose(a);
        check(discovered.kind==ActionKind::Move && discovered.dx!=1, "agent failed discovered trap detour");
        game=empty; a.reset(); game.hp=20; game.features[0]={{10,11},FEATURE_HEALING_SPRING};
        auto spring=choose(a);
        check(spring.kind==ActionKind::Move && spring.dy==1 && spring.dx==0, "visible beneficial feature ignored");
        check(dispatch(spring) && !game.features[0].id && game.hp==45 && game.turns==1, "simulator feature production resolver/turn");
        game=empty; a.reset(); game.weapon_slot=0; game.inventory[0]=make_equipment(LONG_SWORD,0);
        game.inventory[0].info|=ITEM_CURSED; game.inventory[1]=make_equipment(LONG_BOW,0);
        game.inventory[2]={ARROWS,20}; monster(HOBGOBLIN,{14,10});
        check(!(choose(a).kind==ActionKind::Use && choose(a).slot==1), "cursed weapon caused rejected bow switch");
        game.inventory[0]=make_equipment(SHORT_BOW,0); game.inventory[0].info|=ITEM_CURSED; a.reset();
        check(!(choose(a).kind==ActionKind::Use && choose(a).slot==1), "cursed bow caused rejected replacement");
    }

    fire_checks(); tactical_wands(); retreat_and_inventory();
    using namespace rogue; using namespace sim;
    OmniscientAgent agent;
    for(uint8_t type : {uint8_t(STORMBRINGER), uint8_t(GLASS_SWORD), uint8_t(HAMMER_OF_RUIN),
            uint8_t(DRAGONHIDE), uint8_t(TITAN_PLATE), uint8_t(RING_INVISIBILITY), uint8_t(RING_REPRISAL), uint8_t(RING_HUNT),
            uint8_t(AMULET_PHOENIX_HEART), uint8_t(AMULET_HEART_OF_GIANT)}) {
        arena(); agent.reset();
        game.inventory[0] = is_equipment(type) ? make_equipment(type, 0) : Item{type, 1};
        auto artifact_action = choose(agent);
        check(artifact_action.kind == ActionKind::Use && artifact_action.slot == 0,
              "agent did not value artifact equipment");
        game.inventory[0].info |= ITEM_CURSED;
        check(choose(agent).kind != ActionKind::Use, "agent equipped cursed artifact");
    }
    // An artifact must remain an upgrade when ordinary top-tier equipment is
    // already worn, including a Hunt without arrows (its passive benefits apply).
    for(uint8_t type : {uint8_t(STORMBRINGER), uint8_t(GLASS_SWORD), uint8_t(HAMMER_OF_RUIN),
            uint8_t(DRAGONHIDE), uint8_t(TITAN_PLATE), uint8_t(RING_INVISIBILITY), uint8_t(RING_REPRISAL), uint8_t(RING_HUNT),
            uint8_t(AMULET_PHOENIX_HEART), uint8_t(AMULET_HEART_OF_GIANT)}) {
        arena(); agent.reset();
        game.inventory[0] = is_equipment(type) ? make_equipment(type, 0) : Item{type, 1};
        if(is_weapon(type)) { game.inventory[1] = make_equipment(TWO_HANDED_SWORD, 0); game.weapon_slot = 1; }
        else if(is_armor(type)) { game.inventory[1] = make_equipment(PLATE_MAIL, 0); game.armor_slot = 1; }
        else if(is_amulet(type)) { game.inventory[1] = {AMULET_SPEED, 1}; game.amulet_slot = 1; }
        else {
            game.inventory[1] = game.inventory[2] = {RING_PROTECTION, 1};
            game.ring_slots[0] = 1; game.ring_slots[1] = 2;
        }
        auto upgrade_action = choose(agent);
        if(is_ring(type)) {
            check(upgrade_action.kind == ActionKind::Use &&
                  (upgrade_action.slot == 1 || upgrade_action.slot == 2),
                  "artifact policy did not remove the weaker ordinary ring");
            check(dispatch(upgrade_action), "ordinary ring removal failed");
            upgrade_action = choose(agent);
        }
        check(upgrade_action.kind == ActionKind::Use && upgrade_action.slot == 0,
              "artifact policy failed to upgrade ordinary top-tier equipment");
    }
    arena(); agent.reset(); wand(WAND_FIRE); monster(ORC, {11,10});
    game.inventory[1] = make_equipment(DRAGONHIDE, 0); game.armor_slot = 1;
    check(fire(choose(agent)), "agent ignored Dragonhide immunity for point-blank fire");
    arena(); agent.reset();
    for(auto& item : game.inventory) item = {SCROLL_IDENTIFY, 1};
    game.inventory[1] = {ARROWS, 6}; game.ground[0] = {game.player, {RING_HUNT, 1}};
    auto hunt_action = choose(agent);
    check(!(hunt_action.kind == ActionKind::Take && hunt_action.slot == 1), "Hunt displaced its only arrows");
    check(dispatch(hunt_action), "Hunt acquisition failed");
    auto next_action = choose(agent);
    check(next_action.kind != ActionKind::Take, "Hunt acquisition reversed immediately");
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
    // Forge/pool curses can affect a weapon already equipped. A ranged
    // opportunity must not repeatedly request an impossible replacement.
    for(uint8_t current : {uint8_t(DAGGER),uint8_t(SHORT_BOW),uint8_t(LONG_BOW)}) {
        arena(); agent.reset(); monster(HOBGOBLIN,{15,10});
        game.inventory[0]=make_equipment(current,0); game.inventory[0].info|=ITEM_CURSED;
        game.weapon_slot=0; game.inventory[1]=make_equipment(LONG_BOW,7);
        game.inventory[2]={ARROWS,20}; game.inventory[3]=make_equipment(TWO_HANDED_SWORD,7);
        a=choose(agent);
        check(!(a.kind==ActionKind::Use && (a.slot==1 || a.slot==3)),"feature-cursed weapon replacement loop");
        for(auto& m:game.monsters) m={};
        game.inventory[4]={SCROLL_REMOVE_CURSE,1}; agent.reset(); a=choose(agent);
        check(a.kind==ActionKind::Use && a.slot==4 && a.target==0,"feature-cursed weapon failed to use available cure");
    }
    // Lights and shields are mutually exclusive; fixed jewelry permits armor.
    arena(); agent.reset(); game.inventory[0] = {LANTERN,150};
    game.inventory[14] = {}; game.amulet_slot = NONE;
    a=choose(agent); check(a.kind==ActionKind::Use && a.slot==0, "lantern upgrade ignored");
    arena(); agent.reset(); game.inventory[0] = make_equipment(TOWER_SHIELD,0);
    a=choose(agent); check(a.kind==ActionKind::Use && a.slot==0, "shield with permanent sight ignored");
    // A discovered replenishing trap must not pin a healthy armored oracle forever.
    arena(); agent.reset(); monster(PHANTOM,{11,10});
    game.monsters[0].state |= MON_TRAP_SUMMONED;
    game.features[0] = {{11,10},FEATURE_SUMMONING_TRAP};
    bool left=false;
    for(unsigned step=0;step<70;++step) {
        a=choose(agent);
        if(a.goal=="leave replenishing summoning fight") {
            check(a.kind==ActionKind::Move && (int(game.player.x+a.dx)-11)*(int(game.player.x+a.dx)-11)+a.dy*a.dy>1,"summoning retreat not away from trap"); left=true; break;
        }
    }
    check(left,"summoning fight remained unbounded");
    // Acquisition and retention must not reverse the bow-owned ammo/wand swap.
    arena(); agent.reset();
    for(auto& item:game.inventory) item={POTION_HEALING,1};
    game.inventory[14]={AMULET_BRILLIANCE,1}; game.amulet_slot=14;
    game.inventory[15]=make_equipment(BUCKLER,0); game.offhand_slot=15;
    game.inventory[0]={CANDLE,1}; game.ground[0]={game.player,{CANDLE,1}};
    auto light_pick=agent.choose_pickup({game,0,0});
    check(!(light_pick.kind==ActionKind::Take && light_pick.slot==0),"depleted candle replacement cycles");
    game.ground[0].item.info=2;
    light_pick=agent.choose_pickup({game,0,0});
    check(light_pick.kind==ActionKind::Take && light_pick.slot==0,"strictly improved reserve fuel rejected");
    check(dispatch(light_pick),"reserve fuel replacement failed");
    light_pick=agent.choose_pickup({game,0,0});
    check(!(light_pick.kind==ActionKind::Take && light_pick.slot==0),"reserve fuel replacement reversed");
    // Acquisition and retention must not reverse the bow-owned ammo/wand swap.
    arena(); agent.reset(); for(auto& i:game.inventory) i={POTION_HEALING,1};
    game.inventory[0]=make_equipment(LONG_BOW,0); game.inventory[1]={ARROWS,2};
    game.inventory[2]={WAND_STRIKING,12}; game.ground[0]={game.player,{ARROWS,2}};
    auto pick=agent.choose_pickup({game,0,0});
    check(!(pick.kind==ActionKind::Take && pick.slot==2),"ammo acquisition reversed useful wand retention");
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
