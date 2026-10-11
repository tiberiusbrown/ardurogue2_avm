#include "explorer_events.hpp"
#include <cstring>
#include <algorithm>
namespace sim {
namespace { ExplorerMessages* sink=nullptr; }
void ExplorerMessages::prepare(const ExplorerAction& a,SeenPosition player,bool confused) {
    *this={};
    origin=player;
    melee_attempt=a.kind==ExplorerActionKind::Move && !confused &&
        ((a.dx==0 && (a.dy==1 || a.dy==-1)) || (a.dy==0 && (a.dx==1 || a.dx==-1)));
    if(melee_attempt) attempted={uint8_t(int(player.x)+a.dx),uint8_t(int(player.y)+a.dy)};
}
void ExplorerMessages::text(const char* s) {
    if(!std::strcmp(s,"You miss")) pending=uint8_t(MeleeFeedback::Miss);
    else if(!std::strcmp(s,"You hit")) pending=uint8_t(MeleeFeedback::Hit);
    else if(!std::strcmp(s,"You defeat")) { pending=uint8_t(MeleeFeedback::Defeat); encounter_changed=true; }
    else if(!std::strcmp(s,"hits you!") || !std::strcmp(s,"misses you.") || !std::strcmp(s,"breathes fire!")) {
        attacked=true; attacker_type=last_named; pending=0;
        incoming_attacks=uint8_t(std::min(255,int(incoming_attacks)+1));
        if(!last_named) anonymous_attacks=uint8_t(std::min(255,int(anonymous_attacks)+1));
    } else pending=0;
    if(!std::strcmp(s,"is blasted back!") || !std::strcmp(s,"disappears!") ||
       !std::strcmp(s,"changes form!") || !std::strcmp(s,"triggers a teleport trap!") ||
       !std::strcmp(s,"dies in a trap!")) encounter_changed=true;
    if(!std::strcmp(s,"A trap calls monsters!") || !std::strcmp(s,"Monsters answer a sinister call!")) summoning_reported=true;
}
void ExplorerMessages::monster(uint8_t type) {
    last_named=type;
    // Only the first player melee result belongs to the attempted square.
    // Subsequent splash/reprisal deaths only add encounter uncertainty.
    if(pending && pending!=uint8_t(MeleeFeedback::Defeat) && melee_attempt && !contact_reported) {
        contact_reported=true; contact_type=type; feedback=MeleeFeedback(pending);
    }
    // A defeat may also be splash, reprisal or holy damage. It cannot identify
    // the attempted square by itself, even during a Move action.
    if(pending==uint8_t(MeleeFeedback::Defeat) && feedback==MeleeFeedback::None) feedback=MeleeFeedback::Defeat;
    pending=0;
}
ExplorerMessageScope::ExplorerMessageScope(ExplorerMessages* s):previous(sink) { sink=s; }
ExplorerMessageScope::~ExplorerMessageScope() { sink=previous; }
void explorer_status_text(const char* s) { if(sink) sink->text(s); }
void explorer_status_monster(uint8_t type) { if(sink) sink->monster(type); }
void explorer_message_observation(ExplorerObservation& o) {
    if(!sink) return;
    o.attacked=o.attacked||sink->attacked; o.reported_attacker=sink->attacker_type;
    o.incoming_attack=sink->attacked;
    o.contact_reported=sink->contact_reported; o.contact_type=sink->contact_type;
    o.melee_feedback=sink->feedback; o.incoming_attacks=sink->incoming_attacks;
    o.anonymous_attacks=sink->anonymous_attacks; o.encounter_changed=sink->encounter_changed;
    o.summoning_reported=sink->summoning_reported;
    if(o.contact_reported) { o.contact=sink->attempted; o.contact_origin=sink->origin; }
}
}
