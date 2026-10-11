#include "explorer_events.hpp"
#include <cstring>
namespace sim {
namespace { ExplorerMessages* sink=nullptr; }
void ExplorerMessages::prepare(const ExplorerAction& a,SeenPosition player,bool confused) {
    *this={};
    melee_attempt=a.kind==ExplorerActionKind::Move && !confused;
    if(melee_attempt) attempted={uint8_t(int(player.x)+a.dx),uint8_t(int(player.y)+a.dy)};
}
void ExplorerMessages::text(const char* s) {
    if(!std::strcmp(s,"You miss") || !std::strcmp(s,"You hit")) pending=1;
    else if(!std::strcmp(s,"hits you!") || !std::strcmp(s,"misses you.") || !std::strcmp(s,"breathes fire!")) {
        attacked=true; attacker_type=last_named; pending=0;
    } else pending=0;
}
void ExplorerMessages::monster(uint8_t type) {
    last_named=type;
    if(pending && melee_attempt) { contact_reported=true; contact_type=type; }
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
    if(o.contact_reported) o.contact=sink->attempted;
}
}
