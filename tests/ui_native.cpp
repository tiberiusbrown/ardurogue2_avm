#include "status.hpp"
#include "companions.hpp"
#include "spells.hpp"
#include "app_state.hpp"
#include "game.hpp"
#include "inventory_view.hpp"
#include "render.hpp"
#include "ui.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

uint8_t avm_test_buttons[16] = {};
uint8_t avm_test_button_count = 0;
uint8_t avm_test_button_index = 0;

static unsigned play_renders = 0;
static unsigned full_renders = 0;
static unsigned inventory_renders = 0;
static unsigned deferred_renders = 0;
static bool play_render_pending = false;
static uint8_t last_inventory_total = 0;
static void require(bool condition, const char* message);

namespace rogue {
Game game = {};
bool status_has_message() { return false; }
const char* status_word(const char*) { return nullptr; }
void status_words(const char*) { study_status_output();}
void status_suffix(char) {}
void status_capitalize() {}
void status(const char*) { study_status_output();}
void status(const char*, char) { study_status_output();}
void status(Item) { study_status_output();}
void status(Item, char) { study_status_output();}
void status_visible_monster(const Monster& m) { status(m); }
void status(const Monster&) { study_status_output();}
void status(const Monster&, char) { study_status_output();}
void status_number(uint8_t) { study_status_output();}
void status_number(uint8_t, char) { study_status_output();}
void status_clear() {}
void defer_play_render() { ++deferred_renders; play_render_pending = true; }
void render_play() { ++play_renders; play_render_pending = false; }
void restore_play_render() { if(play_render_pending) render_play(); }
void render_inventory(const char*, const InventoryView& view, uint8_t selection,
                      uint8_t top, uint8_t total)
{
    require(total == view.count(), "modal reused a stale inventory count");
    if(total) {
        uint8_t row = view.position(selection);
        require(row != NONE && row >= top && row < top + INVENTORY_VISIBLE_ROWS,
                "modal selection row left its viewport");
    } else require(selection == NONE && top == 0, "empty modal retained a selection");
    last_inventory_total = total;
    ++inventory_renders;
}
void render_spellbook(uint8_t, const SpellbookView&, uint8_t, uint8_t) {}
void render_yesno_prompt(const char*, const Item*) { restore_play_render(); }
void render() { ++full_renders; play_render_pending = false; }
void animate_arrow(Position, int8_t, int8_t, uint8_t) {}
void animate_lightning(Position, Position) {}
void animate_ray(Position, int8_t, int8_t, uint8_t) {}
void animate_fire_burst(Position) {}
void animate_spreading_rays(Position, const uint8_t[4]) {}
void animate_fire_bursts(const Position*, uint8_t, bool) {}
void invalidate_saved_game() {}
void resume_saved_game() {}
void save_resumable_game() {}
}

static void require(bool condition, const char* message)
{
    if(!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

static rogue::InputAction dispatch_input(uint8_t buttons)
{
    using namespace rogue;
    InputAction input = resolve_move_input(handle_input(buttons));
    if(input == INPUT_DROP || input == INPUT_DISCARD) drop_inventory(ui.selection, input == INPUT_DISCARD);
    if(input == INPUT_WAIT_SEARCH) wait_search();
    if(input == INPUT_WAIT) end_turn();
    if(input == INPUT_DISMISS_PET) dismiss_pet(ui.use_target);
    if(input == INPUT_USE_INVENTORY) use_inventory(ui.selection, ui.use_target);
    if(session.object_detection) { session.object_detection=false; ui.mode=OBJECT_MAP; }
    if(input >= INPUT_PROJECTILE_UP && input <= INPUT_PROJECTILE_LEFT) {
        int8_t dx=input==INPUT_PROJECTILE_RIGHT ? 1 : input==INPUT_PROJECTILE_LEFT ? -1 : 0;
        int8_t dy=input==INPUT_PROJECTILE_UP ? -1 : input==INPUT_PROJECTILE_DOWN ? 1 : 0;
        if(!throw_or_shoot(ui.selection,dx,dy)) ui.mode=PROJECTILE_DIRECTION;
    }
    if(input >= INPUT_WAND_UP && input <= INPUT_WAND_IMMEDIATE) {
        int8_t dx = input == INPUT_WAND_RIGHT ? 1 :
                    input == INPUT_WAND_LEFT ? -1 : 0;
        int8_t dy = input == INPUT_WAND_UP ? -1 :
                    input == INPUT_WAND_DOWN ? 1 : 0;
        if(!use_wand(ui.selection, dx, dy) && input != INPUT_WAND_IMMEDIATE)
            ui.mode = WAND_DIRECTION;
    }
    return input;
}

int main()
{
    using namespace rogue;
    // The book modal is inspection-only until its selection frame has unwound.
    for(bool cancel : {false,true}) {
        start_new(71); std::memset(game.monsters,0,sizeof game.monsters); std::memset(game.inventory,0,sizeof game.inventory);
        game.inventory[0]={SPELLBOOK,BOOK_CONTROL}; game.known_spells=1|spell_bit(FROSTBIND)|spell_bit(CONFOUND);
        ui={}; ui.mode=MENU; ui.selection=1;
        const uint8_t confirm[]={0,AVM_BUTTON_A,0,AVM_BUTTON_D,0,AVM_BUTTON_D,0,AVM_BUTTON_U,0,AVM_BUTTON_A};
        const uint8_t dismiss[]={0,AVM_BUTTON_A,0,AVM_BUTTON_B};
        std::memcpy(avm_test_buttons,cancel ? dismiss : confirm,cancel ? sizeof dismiss : sizeof confirm);
        avm_test_button_count=cancel ? sizeof dismiss : sizeof confirm; avm_test_button_index=0;
        Game snapshot=game; InputAction result=handle_input(AVM_BUTTON_A);
        require(!std::memcmp(&snapshot,&game,sizeof game) && !studying(),"book selection changed turn/MP/hunger/RNG before unwinding");
        if(cancel) require(result==INPUT_NONE && ui.mode==PLAY,"book B cancellation emitted action");
        else {
            require(result==INPUT_USE_INVENTORY && ui.selection==0 && ui.use_target==BANISH,"book navigation/confirmation selected wrong unknown spell");
            require(use_inventory(ui.selection,ui.use_target) && studying() && !game.turns && session.study_spell==BANISH,"deferred book study failed");
            cancel_study(false);
        }
    }
    start_new(71); game.inventory[0]={SPELLBOOK,BOOK_CONTROL}; game.known_spells=ALL_SPELLS; ui={};
    require(choose_book_spell(0)==NONE && !studying(),"completed book opened empty learning menu");
    // Inventory modals return a command; consumption and effects run afterwards.
    for(uint8_t type : {uint8_t(SCROLL_DETECT_OBJECTS), uint8_t(POTION_HASTE)}) {
        start_new(0x4312);
        std::memset(game.monsters, 0, sizeof game.monsters);
        std::memset(game.inventory, 0, sizeof game.inventory);
        game.inventory[0] = {type, 2};
        ui = {}; ui.mode = MENU; ui.selection = 1;
        avm_test_buttons[0] = 0; avm_test_buttons[1] = AVM_BUTTON_A;
        avm_test_button_count = 2; avm_test_button_index = 0;
        uint8_t turns = game.turns;
        require(handle_input(AVM_BUTTON_A) == INPUT_USE_INVENTORY &&
                game.turns == turns && item_value(game.inventory[0]) == 2 &&
                !session.object_detection && !game.haste,
                "inventory modal resolved effect before its frame unwound");
        require(use_inventory(ui.selection, ui.use_target) &&
                game.turns == uint8_t(turns + 1) && item_value(game.inventory[0]) == 1,
                "deferred consumable failed turn or stack consumption");
        if(type == SCROLL_DETECT_OBJECTS) {
            require(session.object_detection, "object scroll did not request display");
            session.object_detection = false; ui.mode = OBJECT_MAP;
            Game snapshot = game;
            ui.previous_buttons = 0;
            require(handle_input(AVM_BUTTON_U) == INPUT_NONE && ui.mode == OBJECT_MAP &&
                    !std::memcmp(&game, &snapshot, sizeof game),
                    "object map inspection spent a turn or changed state");
            for(uint8_t dismiss : {uint8_t(AVM_BUTTON_A), uint8_t(AVM_BUTTON_B)}) {
                ui.mode = OBJECT_MAP; ui.previous_buttons = 0;
                require(handle_input(dismiss) == INPUT_NONE && ui.mode == PLAY &&
                        !std::memcmp(&game, &snapshot, sizeof game),
                        "object map dismissal spent a turn");
            }
        }
    }
    for(uint8_t type : {uint8_t(SCROLL_IDENTIFY), uint8_t(SCROLL_ENCHANT), uint8_t(SCROLL_REMOVE_CURSE)}) {
        start_new(0x4312); std::memset(game.inventory, 0, sizeof game.inventory);
        game.inventory[0] = {type, 1}; ui = {}; ui.mode = MENU; ui.selection = 1;
        const uint8_t cancel_target[] = {0, AVM_BUTTON_A, 0, AVM_BUTTON_B};
        std::memcpy(avm_test_buttons, cancel_target, sizeof cancel_target);
        avm_test_button_count = sizeof cancel_target; avm_test_button_index = 0;
        Game snapshot = game;
        require(handle_input(AVM_BUTTON_A) == INPUT_NONE && ui.mode == PLAY &&
                !std::memcmp(&game, &snapshot, sizeof game),
                "canceling scroll target spent an item or turn");
    }
    start_new(0x4312);
    std::memset(game.walls,0,sizeof game.walls); std::memset(game.monsters,0,sizeof game.monsters);
    game.door_count=0; game.player={10,10}; game.inventory[0]={ARROWS,255};
    ui.mode=PROJECTILE_DIRECTION; ui.selection=0;
    uint8_t before=game.turns;
    require(handle_input(AVM_BUTTON_R)==INPUT_PROJECTILE_RIGHT && game.inventory[0].info==255 && game.turns==before,
            "UI resolved animated projectile before its frame unwound");
    require(throw_or_shoot(ui.selection,1,0) && game.inventory[0].info==254,"deferred arrow dispatch failed");
    ui.mode=PROJECTILE_DIRECTION; ui.previous_buttons=0;
    require(handle_input(AVM_BUTTON_B)==INPUT_NONE && ui.mode==PLAY && game.inventory[0].info==254,"projectile cancel spent ammo");
    start_new(0x4312);
    game.inventory[0] = {WAND_FIRE, 3};
    ui.mode = WAND_DIRECTION;
    ui.selection = 0;
    ui.previous_buttons = 0;
    uint8_t turns = game.turns;
    require(dispatch_input(AVM_BUTTON_B) == INPUT_NONE && ui.mode == PLAY &&
            item_value(game.inventory[0]) == 3 && game.turns == turns &&
            !item_type_identified(WAND_FIRE),
            "B did not cancel wand direction without spending resources");

    for(Monster& monster : game.monsters) monster.type = NO_MONSTER;
    game.player = {10, 10};
    for(uint8_t& wall : game.walls) wall = 0;
    game.door_count = 0;
    ui.mode = WAND_DIRECTION;
    ui.previous_buttons = 0;
    require(dispatch_input(AVM_BUTTON_R) == INPUT_WAND_RIGHT && ui.mode == PLAY &&
            item_value(game.inventory[0]) == 2 &&
            game.turns == static_cast<uint8_t>(turns + 1) &&
            item_type_identified(WAND_FIRE),
            "wand direction did not fire and spend one charge and turn");

    for(uint8_t modifier = WAND_NORMAL; modifier <= WAND_OVERPOWERED;
        ++modifier) {
        start_new(0x4312);
        std::memset(game.walls, 0, sizeof game.walls);
        std::memset(game.monsters, 0, sizeof game.monsters);
        std::memset(game.inventory, 0, sizeof game.inventory); game.offhand_slot = NONE;
        game.door_count = 0;
        game.player = {10, 10};
        game.hp = game.max_hp = 240;
        game.inventory[0] = {WAND_DIGGING, 2};
        set_wand_modifier(game.inventory[0], static_cast<WandModifier>(modifier));
        ui.mode = MENU;
        ui.selection = 1;
        ui.previous_buttons = 0;
        avm_test_buttons[0] = 0;
        avm_test_buttons[1] = AVM_BUTTON_A;
        avm_test_button_count = 2;
        avm_test_button_index = 0;
        play_renders = full_renders = deferred_renders = 0;
        turns = game.turns;
        dispatch_input(AVM_BUTTON_A);
        bool needs_direction = modifier == WAND_NORMAL ||
                               modifier == WAND_POWERFUL;
        require(ui.mode == (needs_direction ? WAND_DIRECTION : PLAY) &&
                item_value(game.inventory[0]) == (needs_direction ? 2 : 1) &&
                game.turns == static_cast<uint8_t>(turns +
                                                  (needs_direction ? 0 : 1)),
                "wand selection UI targeted a modifier incorrectly");
        require(play_renders == 0 && full_renders == 0 && deferred_renders == 1,
                "wand selection eagerly restored the dungeon");
        if(needs_direction) {
            dispatch_input(AVM_BUTTON_B);
            require(ui.mode == PLAY && item_value(game.inventory[0]) == 2 &&
                    game.turns == turns,
                    "canceling a normal or powerful wand spent resources");
        }
    }
    // Item actions defer their background until a page/prompt actually needs
    // it; the main loop can otherwise render only the completed turn.
    const uint8_t types[] = {DAGGER, SPEAR, LONG_SWORD, MACE, TWO_HANDED_SWORD,
                             LEATHER_ARMOR, RING_MAIL, SCALE_MAIL, CHAIN_MAIL, SPLINT_MAIL, PLATE_MAIL, RING_STRENGTH, AMULET_SPEED,
                             SCROLL_IDENTIFY, SCROLL_ENCHANT, SCROLL_REMOVE_CURSE};
    for(uint8_t type : types) {
        for(unsigned equipped = 0; equipped < 2; ++equipped) {
            start_new(0x4312);
            std::memset(game.monsters, 0, sizeof game.monsters);
            std::memset(game.inventory, 0, sizeof game.inventory); game.offhand_slot = NONE;
            game.inventory[0] = {type, 1};
            if(equipped) {
                if(is_weapon(type)) game.weapon_slot = 0;
                if(is_armor(type)) game.armor_slot = 0;
                if(is_ring(type)) game.ring_slots[0] = 0;
                if(is_amulet(type)) game.amulet_slot = 0;
            }
            ui.mode = MENU;
            ui.selection = 1;
            ui.previous_buttons = 0;
            bool target_scroll = type == SCROLL_IDENTIFY || type == SCROLL_ENCHANT ||
                                 type == SCROLL_REMOVE_CURSE;
            avm_test_buttons[0] = 0;
            avm_test_buttons[1] = AVM_BUTTON_A;
            avm_test_buttons[2] = 0;
            avm_test_buttons[3] = AVM_BUTTON_A;
            avm_test_button_count = target_scroll ? 4 : 2;
            avm_test_button_index = 0;
            play_renders = full_renders = inventory_renders = deferred_renders = 0;
            turns = game.turns;
            dispatch_input(AVM_BUTTON_A);
            require(play_renders == 0 && full_renders == 0 && deferred_renders == 1 &&
                    inventory_renders == (target_scroll ? 2u : 1u),
                    "item selection eagerly restored or displayed the dungeon");
            require(ui.mode == PLAY && game.turns == static_cast<uint8_t>(turns + 1),
                    "item selection changed the turn or left the inventory open");
            if(is_amulet(type)) require(game.amulet_slot == (equipped ? NONE : 0),
                                        "amulet selection failed to toggle equipment");
            if(is_ring(type)) require(game.ring_slots[0] == (equipped ? NONE : 0),
                                      "ring selection failed to toggle equipment");
        }
    }
    // Each new modal must rebuild its metadata after inventory/filter changes.
    std::memset(game.inventory, 0, sizeof game.inventory); game.offhand_slot = NONE;
    game.inventory[0] = {POTION_HEALING, 1};
    game.inventory[7] = {WAND_FORCE, 2};
    game.inventory[15] = {LONG_SWORD, 1};
    const uint8_t browsing[] = {0, AVM_BUTTON_D, 0, AVM_BUTTON_D,
                               0, AVM_BUTTON_U, 0, AVM_BUTTON_A};
    std::memcpy(avm_test_buttons, browsing, sizeof browsing);
    avm_test_button_count = sizeof browsing;
    avm_test_button_index = 0;
    ui.previous_buttons = ui.held_direction = 0;
    turns = game.turns;
    require(choose_item(F("Choose"), nullptr) == 7 && last_inventory_total == 6 &&
            game.turns == turns,
            "browsing did not skip headers or preserve the game turn");

    game.inventory[15].type = NO_ITEM;
    const uint8_t next_item[] = {0, AVM_BUTTON_D, 0, AVM_BUTTON_A};
    std::memcpy(avm_test_buttons, next_item, sizeof next_item);
    avm_test_button_count = sizeof next_item;
    avm_test_button_index = 0;
    ui.previous_buttons = ui.held_direction = 0;
    require(choose_item(F("Choose"), nullptr) == 0 && last_inventory_total == 4,
            "reopened modal retained removed inventory rows");

    avm_test_buttons[0] = 0;
    avm_test_buttons[1] = AVM_BUTTON_A;
    avm_test_button_count = 2;
    avm_test_button_index = 0;
    ui.previous_buttons = ui.held_direction = 0;
    require(choose_item(F("Choose"), is_potion) == 0 && last_inventory_total == 2,
            "filtered modal reused unfiltered row metadata");

    avm_test_button_index = 0;
    ui.previous_buttons = ui.held_direction = 0;
    require(choose_item(F("Choose"), is_equipment) == NONE && last_inventory_total == 0,
            "empty filtered modal retained row metadata");
    // Run the same deferred feature input/resolver lifecycle as main.cpp.
    for(uint8_t type : {uint8_t(FEATURE_HEALING_SPRING), uint8_t(FEATURE_ENCHANTED_FORGE), uint8_t(FEATURE_SACRIFICIAL_ALTAR), uint8_t(FEATURE_ALCHEMICAL_POOL)}) {
        for(unsigned response = 0; response < 3; ++response) {
            game={}; game.offhand_slot=NONE; session={NONE,DEATH,false}; ui={}; ui.mode=PLAY;
            game.hp=20; game.max_hp=100; game.hunger=220; game.speed=4; game.random_state=123;
            game.player={10,10}; game.up={2,2}; game.down={40,20};
            game.weapon_slot=game.armor_slot=game.amulet_slot=game.ring_slots[0]=game.ring_slots[1]=NONE;
            game.inventory[0]=make_equipment(LONG_SWORD,0);
            game.features[0]={{11,10},type};
            require(resolve_move_input(handle_input(AVM_BUTTON_R))==INPUT_MOVED && session.pending_move && !game.turns,
                "feature input resolved before modal frame unwound");
            const uint8_t buttons[]={0,uint8_t(response==0 ? AVM_BUTTON_B : AVM_BUTTON_A),0,uint8_t(response==1 ? AVM_BUTTON_B : AVM_BUTTON_A)};
            std::memcpy(avm_test_buttons,buttons,sizeof buttons); avm_test_button_count=sizeof buttons; avm_test_button_index=0;
            FeatureChoice choice=prompt_feature();
            require(!game.turns && game.features[0].id==type,"feature modal changed production state/turn");
            complete_player_move(choice);
            bool consumed=response!=0 && (type==FEATURE_HEALING_SPRING || response==2);
            require(game.turns==1 && bool(game.features[0].id)==!consumed,"feature yes/no/target cancel turn or consumption");
        }
    }
    // Overlapping item/feature entry follows main's pickup-before-feature phase.
    for(bool accept : {false,true}) {
        game={}; game.offhand_slot=NONE; session={NONE,DEATH,false}; ui={}; ui.mode=PLAY;
        game.hp=game.max_hp=100; game.hunger=220; game.speed=4; game.random_state=123;
        game.player={10,10}; game.up={2,2}; game.down={40,20};
        game.weapon_slot=game.armor_slot=game.amulet_slot=game.ring_slots[0]=game.ring_slots[1]=NONE;
        game.features[0]={{11,10},FEATURE_TELEPORT_TRAP|MAP_FEATURE_HIDDEN};
        game.ground[0]={{11,10},{YENDOR_AMULET,1}};
        require(resolve_move_input(handle_input(AVM_BUTTON_R))==INPUT_MOVED && session.pending_pickups && !game.turns,
            "UI item/feature overlap activated before pickup");
        const uint8_t buttons[]={0,uint8_t(accept ? AVM_BUTTON_A : AVM_BUTTON_B)};
        std::memcpy(avm_test_buttons,buttons,sizeof buttons); avm_test_button_count=sizeof buttons; avm_test_button_index=0;
        prompt_ground_items();
        require(bool(game.has_amulet)==accept && game.player==Position{11,10} && !game.turns,
            "UI pickup offer ran trap or enemy turn");
        complete_ground_pickups(); complete_player_move(prompt_feature());
        require(game.features[0].id==FEATURE_TELEPORT_TRAP && game.turns==(accept ? 2 : 1) &&
            game.player!=Position{11,10},"UI pickup/decline then teleport completion");
    }
    bool offered_gift=false;
    for(uint16_t seed=1;seed<=100 && !offered_gift;++seed) {
        game={}; game.offhand_slot=NONE; session={NONE,DEATH,false}; ui={}; ui.mode=PLAY;
        game.hp=game.max_hp=100; game.hunger=220; game.speed=4; game.random_state=seed;
        game.player={10,10}; game.up={2,2}; game.down={40,20};
        game.weapon_slot=game.armor_slot=game.amulet_slot=game.ring_slots[0]=game.ring_slots[1]=NONE;
        game.features[0]={{11,10},FEATURE_STATUE}; game.ground[0]={{11,10},{FOOD,1}};
        require(resolve_move_input(handle_input(AVM_BUTTON_R))==INPUT_MOVED,"gift overlap entry");
        Position pickup_position=game.player;
        const uint8_t buttons[]={0,AVM_BUTTON_A};
        std::memcpy(avm_test_buttons,buttons,sizeof buttons); avm_test_button_count=sizeof buttons; avm_test_button_index=0;
        prompt_ground_items();
        if(item_at(game.player)==NONE) pickup_position={NONE,NONE};
        complete_ground_pickups(); complete_player_move({true});
        if(item_at(game.player)==NONE) continue;
        require(game.player!=pickup_position,"new feature gift suppressed by earlier pickup offer");
        avm_test_button_index=0; prompt_ground_items();
        require(item_at(game.player)==NONE && game.turns==3,"feature gift pickup flow or turn cost");
        offered_gift=true;
    }
    require(offered_gift,"statue gift pickup fixture never exercised");
    game={}; session={NONE,DEATH,false}; ui={}; ui.mode=SPELL_MENU;
    game.mana=6; game.known_spells=spell_bit(MAGIC_DART)|spell_bit(BLINK); ui.selection=MAGIC_DART;
    Game unchanged=game;
    require(handle_input(AVM_BUTTON_D)==INPUT_NONE && ui.selection==BLINK,"spell menu included unlearned spells");
    handle_input(0); require(handle_input(AVM_BUTTON_B)==INPUT_NONE && ui.mode==PLAY && !std::memcmp(&unchanged,&game,sizeof game),"spell menu cancel spent resources");
    ui={}; ui.mode=SPELL_MENU; ui.selection=MAGIC_DART;
    require(handle_input(AVM_BUTTON_A)==INPUT_NONE && ui.mode==SPELL_DIRECTION,"spell direction prompt absent");
    handle_input(0); handle_input(AVM_BUTTON_B);
    require(ui.mode==PLAY && !std::memcmp(&unchanged,&game,sizeof game),"direction cancel spent resources");
    ui={}; ui.mode=SPELL_DIRECTION; ui.selection=MAGIC_DART;
    require(handle_input(AVM_BUTTON_R)==INPUT_CAST_RIGHT && ui.mode==PLAY && !std::memcmp(&unchanged,&game,sizeof game),"spell resolved inside UI frame");
    ui={}; ui.mode=SPELL_MENU; ui.selection=BLINK;
    require(handle_input(AVM_BUTTON_A)==INPUT_CAST_IMMEDIATE && !std::memcmp(&unchanged,&game,sizeof game),"immediate spell resolved inside UI frame");
    game={}; session={NONE,DEATH,false}; game.player={20,15}; game.hp=game.max_hp=100;
    game.inventory[0]={GLOWING_ORB,0}; game.offhand_slot=0;
    game.mana=6; game.known_spells=spell_bit(EXCHANGE);
    game.monsters[2]={{22,15},GOBLIN,10,15,{0,0},0}; game.monsters[5]={{20,13},GOBLIN,10,15,{0,0},0};
    ui={}; ui.mode=SPELL_MENU; ui.selection=EXCHANGE; unchanged=game;
    require(handle_input(AVM_BUTTON_A)==INPUT_NONE && ui.mode==SPELL_TARGET && ui.use_target==2,"Exchange target prompt missing");
    handle_input(0); handle_input(AVM_BUTTON_D); require(ui.use_target==5,"Exchange target navigation ignored visible alternative");
    handle_input(0); handle_input(AVM_BUTTON_B); require(ui.mode==PLAY && !std::memcmp(&unchanged,&game,sizeof game),"target cancel spent mana/turn/RNG");
    ui={}; ui.mode=SPELL_MENU; ui.selection=EXCHANGE; game.mana=spell_cost(EXCHANGE)-1; unchanged=game;
    require(handle_input(AVM_BUTTON_A)==INPUT_NONE && ui.mode==PLAY && !std::memcmp(&unchanged,&game,sizeof game),"insufficient menu mana spent resources");
    session.study_slot=2; session.study_spell=FROSTBIND;
    ui={}; ui.mode=PLAY;
    require(handle_input(AVM_BUTTON_R)==INPUT_STUDY_TURN,"held movement delayed automatic study");
    ui.next_repeat_ms=0;
    require(handle_input(AVM_BUTTON_R)==INPUT_STUDY_TURN,"held movement repeatedly paused study");
    handle_input(0); handle_input(AVM_BUTTON_B); require(!studying(),"B failed to cancel between study turns");
    game.known_spells=ALL_SPELLS; game.mana=16; session.study_slot=NONE;
    ui={}; ui.mode=SPELL_MENU; ui.selection=MAGIC_DART;
    for(uint8_t s=1;s<SPELL_COUNT;++s) { handle_input(AVM_BUTTON_D); handle_input(0); require(ui.selection==s,"16-spell menu skipped/wrapped a spell"); }
    require(ui.selection==SUMMON_GOLEM,"spell menu cannot reach bit 19");
    ui.selection=BLOOD_BOLT; unchanged=game; handle_input(AVM_BUTTON_A); handle_input(0); handle_input(AVM_BUTTON_B);
    require(ui.mode==PLAY && !std::memcmp(&unchanged,&game,sizeof game),"Blood Bolt cancellation paid HP/MP");
    start_new(71); std::memset(game.monsters,0,sizeof game.monsters);
    ui={}; ui.mode=MENU; ui.selection=8; Game pet_ui_before=game;
    require(handle_input(AVM_BUTTON_A)==INPUT_NONE && ui.mode==COMPANIONS && ui.selection==NONE,"empty companions UI");
    handle_input(0); handle_input(AVM_BUTTON_B);
    require(ui.mode==PLAY && !std::memcmp(&pet_ui_before,&game,sizeof game),"empty companions cancellation costs turn");
    for(uint8_t i=12;i<16;++i) game.monsters[i]={{uint8_t(10+i),15},uint8_t(HOUND+i-12),14,255,{0,0},0};
    ui={}; ui.mode=MENU; ui.selection=8; handle_input(AVM_BUTTON_A);
    require(ui.mode==COMPANIONS && ui.selection==12,"companions first entry");
    handle_input(0); handle_input(AVM_BUTTON_D); require(ui.selection==13,"companions selection");
    handle_input(0); handle_input(AVM_BUTTON_A); pet_ui_before=game;
    require(ui.mode==DISMISS_CONFIRM,"dismissal missing confirmation");
    handle_input(0); handle_input(AVM_BUTTON_B);
    require(ui.mode==COMPANIONS && !std::memcmp(&pet_ui_before,&game,sizeof game),"dismissal B removes pet");
    handle_input(0); handle_input(AVM_BUTTON_A); handle_input(0);
    require(handle_input(AVM_BUTTON_A)==INPUT_DISMISS_PET && !std::memcmp(&pet_ui_before,&game,sizeof game),"dismissal resolves inside UI frame");
    require(dismiss_pet(ui.use_target) && !game.monsters[13].type && game.turns==uint8_t(pet_ui_before.turns+1),"confirmed dismissal turn/free slot");
    std::puts("equipment, scroll, wand, inventory, spell and feature UI flow passed");
    return 0;
}
