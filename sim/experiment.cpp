#include "experiment.hpp"
#include "metrics.hpp"
#include "game_internal.hpp"
#include <sstream>
#include <stdexcept>

namespace sim {
namespace {
const char* feature_name(uint8_t t) {
    static const char* names[]={"NONE","SPIKE_TRAP","POISON_DART_TRAP","FLAME_TRAP","FROST_TRAP","SNARE_TRAP","TELEPORT_TRAP","CONFUSION_TRAP","MAGIC_DRAIN_TRAP","SUMMONING_TRAP","RUST_TRAP","FOUNTAIN","HEALING_SPRING","ALTAR","SACRIFICIAL_ALTAR","ENCHANTED_FORGE","STATUE","SARCOPHAGUS","RUNESTONE","ALCHEMICAL_POOL","HOLY_SYMBOL"};
    return t<=rogue::FEATURE_HOLY_SYMBOL ? names[t/2] : "NONE";
}
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
        is_armor(t) ? 5 : is_light(t) ? 11 : is_shield(t) ? 12 : is_ring(t) ? 6 : is_amulet(t) ? 7 : is_wand(t) ? 8 : is_ammo(t) ? 10 : 9;
}
}
Rule parse_rule(const std::string& text) {
    if(text.rfind("balance:",0)==0) {
        auto equal=text.find('=');
        if(equal==std::string::npos) throw std::runtime_error("balance requires name=value");
        Rule r; r.balance_parameter=text.substr(8,equal-8); int value=integer(text.substr(equal+1));
        int min=0,max=100;
        auto& n=r.balance_parameter;
        if(n=="ground") { min=16; max=24; }
        else if(n=="inventory") { min=16; max=20; }
        else if(n=="monsters" || n=="phantom-count") { min=1; max=12; }
        else if(n=="sight-duration") { min=1; max=254; }
        else if(n=="phantom-strength" || n=="phantom-dexterity" || n=="phantom-speed" || n=="phantom-health") { min=1; max=255; }
        else if(n=="phantom-armor") max=20;
        else if(n!="sight-percent" && n!="sight-early-percent" && n!="healing-percent" && n!="early-equipment-percent" && n!="late-strength-percent" && n!="late-dexterity-percent" && n!="late-health-percent" && n!="ascent-strength-percent")
            throw std::runtime_error("unknown balance parameter: "+n);
        if(value<min || value>max) throw std::runtime_error("balance value out of range: "+n);
        r.balance_setting=uint8_t(value); return r;
    }
    static const char* original_rules[]={"original-teleport-placement","original-early-traps","original-holy-generation","original-early-density","original-early-damage","original-early-roster"};
    for(int i=0;i<6;++i) if(text==original_rules[i]) { Rule r; r.original=i; return r; }
    for(int i=0;i<4;++i) if(text==(i==0 ? "no-teleport-traps" : i==1 ? "no-early-damaging-traps" : i==2 ? "interactive-features-only" : "traps-only")) {
        Rule r; r.feature_mode=i; return r;
    }
    if(text=="full-light") { Rule r; r.full_light=true; return r; }
    if(text=="light-awareness") { Rule r; r.light_awareness=true; return r; }
    if(text=="legacy-awareness") { Rule r; r.legacy_awareness=true; return r; }
    if(text=="no-monster-traps") { Rule r; r.no_monster_traps=true; return r; }
    if(text=="no-map-features") { Rule r; r.no_features=true; return r; }
    if(text=="ordinary-items") {
        Rule r; r.ordinary_items=true; return r;
    }
    std::istringstream in(text); std::vector<std::string> fields; std::string part;
    while(std::getline(in,part,':')) fields.push_back(part);
    if(fields.size()<2) throw std::runtime_error("intervention requires operation:FROM[:TO][:key=value...]");
    Rule r;
    if(fields[0]=="replace-item") {}
    else if(fields[0]=="remove-item") r.remove=true;
    else if(fields[0]=="replace-monster") r.monster=true;
    else if(fields[0]=="remove-monster") r.monster=r.remove=true;
    else if(fields[0]=="remove-feature") r.feature=r.remove=true;
    else throw std::runtime_error("unknown intervention operation: "+fields[0]);
    if(r.feature) {
        for(uint8_t t=2;t<=rogue::FEATURE_HOLY_SYMBOL;t+=2)
            if(fields[1]==feature_name(t)) r.from=t;
        if(!r.from) throw std::runtime_error("unknown feature: "+fields[1]);
    } else r.from=content(fields[1],r.monster);
    size_t next=2;
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
        if(!r.balance_parameter.empty() || r.full_light || r.light_awareness || r.legacy_awareness || r.ordinary_items || r.no_features || r.no_monster_traps || r.original>=0) continue;
        if(r.feature || r.feature_mode>=0) {
            unsigned removed=0;
            for(auto& f:g.features) {
                uint8_t t=feature_type(f.id);
                bool damaging=t==FEATURE_SPIKE_TRAP || t==FEATURE_POISON_DART_TRAP || t==FEATURE_FLAME_TRAP || t==FEATURE_FROST_TRAP || t==FEATURE_MAGIC_DRAIN_TRAP || t==FEATURE_RUST_TRAP;
                bool match=r.feature ? t==r.from : r.feature_mode==0 ? t==FEATURE_TELEPORT_TRAP :
                    r.feature_mode==1 ? g.floor<4 && damaging : r.feature_mode==2 ? feature_is_trap(t) || t==FEATURE_HOLY_SYMBOL : !feature_is_trap(t);
                if(!f.id || !match || removed>=unsigned(r.maximum) || (r.floor>=0 && r.floor!=g.floor) || (r.direction>=0 && r.direction!=int(bool(g.has_amulet)))) continue;
                ledger.push_back({context.visit,g.floor,bool(g.has_amulet),"remove-feature",feature_name(t),"NONE",1});
                f={}; ++removed;
            }
            continue;
        }
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
uint8_t RuleExperiment::balance_value(const char* name, uint8_t fallback) const {
    for(const auto& r:rules) if(r.balance_parameter==name) return r.balance_setting;
    return fallback;
}
bool RuleExperiment::original_features(FeatureVariant variant) const {
    for(const auto& r:rules) if(r.original==int(variant)) return true;
    return false;
}
bool RuleExperiment::monster_traps_enabled() const {
    for(const auto& r:rules) if(r.no_monster_traps) return false;
    return true;
}
bool RuleExperiment::features_enabled() const {
    for(const auto& r:rules) if(r.no_features) return false;
    return true;
}
bool RuleExperiment::full_light() const {
    for(const auto& r : rules) if(r.full_light) return true;
    return false;
}
bool RuleExperiment::light_awareness() const {
    bool candidate=false,legacy=false;
    for(const auto& r : rules) { candidate|=r.light_awareness; legacy|=r.legacy_awareness; }
    if(candidate && legacy) throw std::runtime_error("conflicting awareness interventions");
    return !legacy;
}
bool RuleExperiment::artifacts_enabled() const {
    for(const auto& r:rules) if(r.ordinary_items) return false;
    return true;
}
}
