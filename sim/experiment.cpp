#include "experiment.hpp"
#include "metrics.hpp"
#include "game_internal.hpp"
#include <sstream>
#include <stdexcept>

namespace sim {
namespace {
int integer(const std::string& s) {
    size_t end=0; int n=std::stoi(s,&end);
    if(end!=s.size() || n<0) throw std::runtime_error("invalid intervention integer: "+s);
    return n;
}
uint8_t content(const std::string& s, bool monster) {
    int last=monster ? rogue::LORD : rogue::WAND_POLYMORPH;
    for(int i=1;i<=last;++i) {
        const char* name=monster ? monster_name(uint8_t(i)) : item_name(uint8_t(i));
        // CSV labels retain schema-2 spellings; accept renamed potion enums too.
        if(s==name || (!monster && rogue::is_potion(uint8_t(i)) && s==std::string("POTION_")+name))
            return uint8_t(i);
    }
    throw std::runtime_error("unknown intervention content: "+s);
}
int group(uint8_t t) {
    using namespace rogue;
    return t==FOOD ? 1 : is_potion(t) ? 2 : is_scroll(t) ? 3 : is_weapon(t) ? 4 :
        is_armor(t) ? 5 : is_ring(t) ? 6 : is_amulet(t) ? 7 : is_wand(t) ? 8 : 9;
}
}
Rule parse_rule(const std::string& text) {
    std::istringstream in(text); std::vector<std::string> fields; std::string part;
    while(std::getline(in,part,':')) fields.push_back(part);
    if(fields.size()<2) throw std::runtime_error("intervention requires operation:FROM[:TO][:key=value...]");
    Rule r;
    if(fields[0]=="replace-item") {}
    else if(fields[0]=="remove-item") r.remove=true;
    else if(fields[0]=="replace-monster") r.monster=true;
    else if(fields[0]=="remove-monster") r.monster=r.remove=true;
    else throw std::runtime_error("unknown intervention operation: "+fields[0]);
    r.from=content(fields[1],r.monster); size_t next=2;
    if(!r.remove) {
        if(fields.size()<3) throw std::runtime_error("replacement requires TO");
        r.to=content(fields[2],r.monster); next=3;
    }
    for(;next<fields.size();++next) {
        auto equal=fields[next].find('=');
        auto key=fields[next].substr(0,equal), value=equal==std::string::npos ? "" : fields[next].substr(equal+1);
        if(key=="floor") { r.floor=integer(value); if(r.floor>=rogue::FLOORS) throw std::runtime_error("invalid intervention floor"); }
        else if(key=="direction") {
            if(value=="descent") r.direction=0;
            else if(value=="ascent") r.direction=1;
            else throw std::runtime_error("invalid intervention direction");
        } else if(key=="max") r.maximum=integer(value);
        else if(key=="info" && !r.monster && !r.remove) {
            r.info=value=="default" ? -1 : value=="preserve" ? -2 : integer(value);
            if(r.info>255) throw std::runtime_error("item info exceeds byte range");
        } else throw std::runtime_error("unknown intervention field: "+fields[next]);
    }
    if(r.info==-2 && group(r.from)!=group(r.to)) throw std::runtime_error("preserve info requires compatible item groups");
    return r;
}
void RuleExperiment::apply(rogue::Game& g, ExperimentContext context, std::vector<Intervention>& ledger) const {
    using namespace rogue;
    for(const auto& r:rules) {
        if((r.floor>=0 && r.floor!=g.floor) || (r.direction>=0 && r.direction!=int(bool(g.has_amulet)))) continue;
        unsigned count=0;
        // Stable slot order, no PRNG, and each rule's cap is per entered visit.
        for(int s=0;s<(r.monster ? MONSTERS : GROUND_ITEMS) && count<unsigned(r.maximum);++s) {
            if(r.monster) {
                auto& m=g.monsters[s]; if(m.type!=r.from) continue;
                if(r.remove) m={};
                else {
                    auto pos=m.pos;
                    m={pos,r.to,monster_health(r.to),0,{0,0},0};
                    if(r.to==MIMIC) set_mimic_appearance(m,MIMIC_SCROLL);
                }
            } else {
                auto& i=g.ground[s].item; if(i.type!=r.from) continue;
                if(r.remove) i={};
                else if(r.info==-2) i.type=r.to;
                else if(r.info>=0) i={r.to,uint8_t(r.info)};
                else i=is_equipment(r.to) ? make_equipment(r.to,0) : Item{r.to,uint8_t(is_wand(r.to) ? 5 : 1)};
            }
            ++count;
        }
        if(count) ledger.push_back({context.visit,g.floor,bool(g.has_amulet),
            std::string(r.remove ? "remove-" : "replace-")+(r.monster ? "monster" : "item"),
            r.monster ? monster_name(r.from) : item_name(r.from),
            r.remove ? "NONE" : r.monster ? monster_name(r.to) : item_name(r.to),count});
    }
}
}
