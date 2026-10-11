#include "simulator.hpp"
#include "parallel.hpp"
#include "policy_hash.hpp"
#include "build_metadata.hpp"
#include "tuning.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
namespace fs=std::filesystem;
using Clock=std::chrono::steady_clock;
uint64_t number(const std::string& s) {
    if(s.empty() || s[0]=='-' || s[0]=='+') throw std::runtime_error("invalid unsigned integer");
    size_t used=0; auto n=std::stoull(s,&used); if(used!=s.size()) throw std::runtime_error("invalid unsigned integer"); return n;
}
std::string quote(const std::string& s) {
    std::string q="\"";
    for(auto c:s) { if(c=='\\'||c=='\"') q+='\\'; if(c=='\n') q+="\\n"; else q+=c; }
    return q+'"';
}
void save(const fs::path& path,const std::string& s) {
    std::ofstream o(path,std::ios::binary); o<<s; o.close(); if(!o) throw std::runtime_error("cannot write "+path.string());
}
struct Candidate : sim::TuningFitness {
    sim::ExplorerParameters parameters;
    uint64_t id=0;
};
std::vector<std::string> csv_fields(const std::string& line) {
    std::vector<std::string> fields; std::string field; bool quoted=false;
    for(size_t n=0;n<line.size();++n) {
        char c=line[n];
        if(c=='"') {
            if(quoted && n+1<line.size() && line[n+1]=='"') { field+='"'; ++n; }
            else quoted=!quoted;
        } else if(c==',' && !quoted) { fields.push_back(field); field.clear(); }
        else if(c!='\r') field+=c;
    }
    if(quoted) throw std::runtime_error("unterminated fitness CSV field");
    fields.push_back(field); return fields;
}
// Failed runs cannot be interpreted as poor game outcomes or win the search.
bool better(const Candidate& a,const Candidate& b) {
    if(sim::fitness_better(a,b)) return true;
    if(sim::fitness_better(b,a)) return false;
    return a.id<b.id; // Stable retention, independent of worker completion order.
}
Candidate evaluate(Candidate c,uint64_t first,uint64_t count,unsigned jobs,
                   const std::string& executable,sim::Options options,const fs::path& path) {
    options.parameters=c.parameters;
    std::ofstream runs(path); sim::write_runs_header(runs);
    if(jobs>1 && count>1) {
        std::array<std::ostream*,sim::CSV_STREAM_COUNT> streams{}; streams[0]=&runs;
        sim::parallel_batch(executable,first,count,jobs,options,streams);
    } else {
        auto agent=std::make_unique<sim::ExplorerAgent>(c.parameters);
        for(uint64_t seed=first;seed<first+count;++seed) sim::write_run(runs,sim::run(uint16_t(seed),*agent,options));
    }
    runs.close(); if(!runs) throw std::runtime_error("cannot retain candidate fitness runs");
    std::ifstream input(path); std::string line; uint64_t rows=0; std::getline(input,line);
    auto header=csv_fields(line); std::map<std::string,size_t> columns;
    for(size_t n=0;n<header.size();++n) columns.emplace(header[n],n);
    while(std::getline(input,line)) {
            auto fields=csv_fields(line);
            if(fields.size()!=header.size()) throw std::runtime_error("invalid fitness CSV");
            auto field=[&](const char* name)->const std::string& { return fields.at(columns.at(name)); };
            auto result=field("result");
            c.escaped+=result=="escaped"; c.failures+=result!="escaped" && result!="death";
            c.actions+=std::stoull(field("actions")); c.score+=std::stoll(field("score"));
            unsigned depth=unsigned(std::stoul(field("deepest_floor"))),exited=unsigned(std::stoul(field("floors_exited")));
            bool yendor=field("has_yendor")=="1",lord=field("lord_killed")=="1";
            c.yendor+=yendor; c.lord+=lord; c.progression+=sim::progression_credit(depth,lord,yendor,exited);
            ++rows;
    }
    if(input.bad() || rows!=count) throw std::runtime_error("fitness seed count mismatch");
    return c;
}
sim::ExplorerParameters offspring(const Candidate& a,const Candidate& b,std::mt19937_64& rng,unsigned generation) {
    auto p=a.parameters;
    // Seeded host RNG affects only preferences. Production gameplay RNG is untouched.
    auto pick=[&](uint64_t n) { return rng()%n; };
    // Mutate one coherent group per offspring. Binary ablation switches and
    // equipment strategy modes are experimental controls, never search genes.
    auto group=[](const char* name) {
        std::string n=name;
        if(n.find("frontier")==0 || n.find("stair")==0 || n.find("loot")==0 || n.find("hunger")==0) return 0;
        if(n.find("healing")!=n.npos || n.find("emergency")==0 || n.find("retreat")==0 || n.find("risk")==0 || n.find("control")==0 || n.find("contact")==0) return 1;
        if(n.find("light")!=n.npos || n.find("shield")!=n.npos || n.find("score")!=n.npos || n.find("weight")!=n.npos || n.find("bonus")!=n.npos) return 2;
        return 3;
    };
    std::vector<const sim::ParameterDefinition*> active;
    int selected=int(generation%4);
    for(auto& d:sim::parameter_definitions) if(d.maximum-d.minimum>1 && std::string(d.name)!="offhand_mode" && group(d.name)==selected) {
        active.push_back(&d); if(pick(4)==0) p.*(d.member)=b.parameters.*(d.member);
    }
    unsigned mutations=2+unsigned(pick(9));
    for(unsigned i=0;i<mutations;++i) {
        auto& d=*active[pick(active.size())];
        int& v=p.*(d.member);
        if(pick(10)==0) v=d.minimum+int(pick(uint64_t(d.maximum-d.minimum+1)));
        else {
            int scale=std::max(2,(std::max(1,v)*(generation%3==0 ? 80 : 35))/100);
            v=std::clamp(v+int(pick(uint64_t(2*scale+1)))-scale,d.minimum,d.maximum);
        }
    }
    sim::validate_parameters(p); return p;
}
}
int main(int argc,char** argv) {
    try {
        uint64_t first=1,last=1000,population=12,generations=20,jobs=1,random_seed=1,minutes=0,initial_seeds=0,patience=3;
        bool selected=false;
        std::string output,start_params;
        auto executable=(fs::absolute(argv[0]).parent_path()/
#ifdef _WIN32
            "ardurogue2_sim.exe"
#else
            "ardurogue2_sim"
#endif
            ).string();
        sim::Options options; options.agent="explorer-v1"; options.telemetry=false;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];
            auto value=[&] { if(i+1==argc) throw std::runtime_error("missing value for "+arg); return std::string(argv[++i]); };
            if(arg=="--help") {
                std::cout<<"ardurogue2_explorer_tune --output DIRECTORY [--seeds FIRST:LAST]\n"
                    "  [--population N] [--generations N] [--jobs N] [--random-seed N]\n"
                    "  [--minutes N] [--start-params FILE] [--simulator FILE] [--max-actions N]\n"
                    "  [--initial-seeds N] [--plateau-generations N]\n"
                    "Defaults: seeds 1:1000, population 12, generations 20, jobs 1, random seed 1.\n"
                    "Fitness: valid batches; escapes, Yendor, Lord, progression, fewer actions, score.\n"
                    "With --initial-seeds, expand tenfold after a plateau and re-evaluate elites.\n"
                    "Always validate final elites on the full range. Minutes limits search between candidates.\n"
                    "Outputs: best.json, baseline.json, candidates/, history.csv, manifest.json.\n";
                return 0;
            }
            if(arg=="--seeds") {
                if(selected) throw std::runtime_error("duplicate seed selection"); selected=true;
                auto s=value(); auto colon=s.find(':'); if(colon==s.npos) throw std::runtime_error("range requires FIRST:LAST");
                first=number(s.substr(0,colon)); last=number(s.substr(colon+1));
            } else if(arg=="--output") output=value();
            else if(arg=="--population") population=number(value());
            else if(arg=="--generations") generations=number(value());
            else if(arg=="--jobs") jobs=number(value());
            else if(arg=="--random-seed") random_seed=number(value());
            else if(arg=="--minutes") minutes=number(value());
            else if(arg=="--initial-seeds") initial_seeds=number(value());
            else if(arg=="--plateau-generations") patience=number(value());
            else if(arg=="--start-params") start_params=value();
            else if(arg=="--simulator") executable=value();
            else if(arg=="--max-actions") options.max_actions=number(value());
            else throw std::runtime_error("unknown option: "+arg);
        }
        if(first>last || last>65535 || (first==0 && last>=44257)) throw std::runtime_error("invalid or duplicate effective seed range");
        if(initial_seeds>last-first+1 || !patience || patience>100000) throw std::runtime_error("invalid seed stage limits");
        if(population<2 || population>1000 || generations<1 || generations>100000 || jobs<1 || jobs>64 || minutes>1000000)
            throw std::runtime_error("invalid search limits");
        if(output.empty()) throw std::runtime_error("--output is required");
        if(jobs>1 && !fs::is_regular_file(executable)) throw std::runtime_error("simulator executable is missing");
        if(fs::exists(output) && !fs::is_empty(output)) throw std::runtime_error("output directory must be empty");
        fs::create_directories(fs::path(output)/"candidates");
        sim::ExplorerParameters initial=start_params.empty() ? sim::ExplorerParameters{} : sim::load_parameters(start_params);
        save(fs::path(output)/"baseline.json",sim::parameter_json(initial));
        std::ostringstream manifest;
        manifest<<"{\n  \"schema_version\": 2,\n  \"algorithm\": \"grouped mutation and crossover v3; staged seeds\",\n"
            "  \"objective\": [\"escapes descending\", \"Yendor descending\", \"Lord kills descending\", \"progression descending\", \"actions ascending\", \"total score descending\"],\n"
            "  \"failures\": \"invalid; excluded from elites\",\n  \"agent\": \"explorer-v1\",\n  \"agent_code_hash\": "<<quote(SIM_EXPLORER_POLICY_HASH)
            <<",\n  \"first_seed\": "<<first<<",\n  \"last_seed\": "<<last<<",\n  \"population\": "<<population
            <<",\n  \"generations\": "<<generations<<",\n  \"jobs\": "<<jobs<<",\n  \"random_seed\": "<<random_seed
            <<",\n  \"initial_seeds\": "<<initial_seeds<<",\n  \"plateau_generations\": "<<patience
            <<",\n  \"minutes\": "<<minutes<<",\n  \"max_actions\": "<<options.max_actions
            <<",\n  \"baseline_parameter_hash\": "<<quote(sim::parameter_hash(initial))<<",\n  \"simulator\": "<<quote(fs::absolute(executable).string())<<"\n}\n";
        save(fs::path(output)/"manifest.json",manifest.str());
        std::ofstream history(fs::path(output)/"history.csv");
        history<<"candidate,generation,seed_count,escapes,total_score,simulator_failures,actions,seconds,parameter_hash,agent_policy_hash,best,yendor,lord,progression\n";
        std::mt19937_64 rng(random_seed); std::set<std::string> evaluated;
        std::vector<Candidate> elites; Candidate best; bool have_best=false; uint64_t id=0; auto began=Clock::now();
        auto expired=[&] { return minutes && Clock::now()-began>=std::chrono::minutes(minutes); };
        uint64_t count=initial_seeds ? initial_seeds : last-first+1;
        unsigned plateau=0;
        auto assess=[&](Candidate c,unsigned generation) {
            c.escaped=c.failures=c.actions=c.yendor=c.lord=c.progression=0; c.score=0; c.id=id++;
            auto hash=sim::parameter_hash(c.parameters); evaluated.insert(hash);
            save(fs::path(output)/"candidates"/(std::to_string(c.id)+".json"),sim::parameter_json(c.parameters));
            auto started=Clock::now(); c=evaluate(c,first,count,unsigned(jobs),executable,options,
                fs::path(output)/"candidates"/(std::to_string(c.id)+".runs.csv"));
            bool improved=!c.failures && (!have_best || better(c,best));
            if(improved) {
                best=c; have_best=true; save(fs::path(output)/"best.json",sim::parameter_json(best.parameters));
                std::ostringstream result;
                result<<"{\n  \"candidate\": "<<best.id<<",\n  \"first_seed\": "<<first<<",\n  \"last_seed\": "<<first+count-1
                    <<",\n  \"escapes\": "<<best.escaped<<",\n  \"total_score\": "<<best.score
                    <<",\n  \"yendor\": "<<best.yendor<<",\n  \"lord\": "<<best.lord<<",\n  \"progression\": "<<best.progression
                    <<",\n  \"parameter_hash\": "<<quote(hash)<<"\n}\n";
                save(fs::path(output)/"best-result.json",result.str());
            }
            history<<c.id<<','<<generation<<','<<count<<','<<c.escaped<<','<<c.score<<','<<c.failures<<','<<c.actions<<','
                <<std::chrono::duration<double>(Clock::now()-started).count()<<','<<hash<<','
                <<sim::explorer_policy_hash(SIM_EXPLORER_POLICY_HASH,c.parameters)<<','<<improved<<','<<c.yendor<<','<<c.lord<<','<<c.progression<<'\n'; history.flush();
            if(!history) throw std::runtime_error("history write failed");
            std::cout<<"candidate="<<c.id<<" generation="<<generation<<" seeds="<<count<<" escaped="<<c.escaped<<" total_score="<<c.score
                <<" failures="<<c.failures<<(improved ? " BEST" : "")<<std::endl;
            return c;
        };
        auto expand=[&](uint64_t next,unsigned generation) {
            count=next; plateau=0; have_best=false; evaluated.clear();
            auto survivors=elites; elites.clear();
            std::cout<<"Expanding to "<<count<<" seeds; re-evaluating "<<survivors.size()<<" elites."<<std::endl;
            for(auto c:survivors) { c=assess(c,generation); if(!c.failures) elites.push_back(c); }
            std::sort(elites.begin(),elites.end(),better);
            if(elites.empty()) throw std::runtime_error("all expanded elites have simulator failures");
        };
        for(unsigned generation=0;generation<generations && (!generation || !expired());++generation) {
            auto pool=elites; auto previous_best=have_best ? sim::parameter_hash(best.parameters) : "";
            for(unsigned member=0;member<population && (!id || !expired());++member) {
                Candidate c;
                if(!id) c.parameters=initial;
                else {
                    bool unique=false;
                    for(unsigned attempt=0;attempt<1000 && !unique;++attempt) {
                        c.parameters=offspring(elites[rng()%elites.size()],elites[rng()%elites.size()],rng,generation);
                        unique=!evaluated.count(sim::parameter_hash(c.parameters));
                    }
                    if(!unique) throw std::runtime_error("cannot generate a unique candidate");
                }
                c=assess(c,generation);
                if(!c.failures) pool.push_back(c);
                std::sort(pool.begin(),pool.end(),better); if(pool.size()>4) pool.resize(4);
                elites=pool;
                if(elites.empty()) throw std::runtime_error("baseline has simulator failures; fix them before tuning");
            }
            plateau=previous_best==sim::parameter_hash(best.parameters) ? plateau+1 : 0;
            if(plateau>=patience && count<last-first+1) expand(std::min(last-first+1,count*10),generation);
        }
        // A short budget may end a coarse stage. The promoted result always
        // receives complete-range validation, never extrapolated fitness.
        if(count<last-first+1) expand(last-first+1,unsigned(generations));
        std::cout<<"best_candidate="<<best.id<<" escaped="<<best.escaped<<" total_score="<<best.score<<" evaluations="<<id<<std::endl;
        return have_best ? 0 : 1;
    } catch(const std::exception& e) { std::cerr<<"ardurogue2_explorer_tune: "<<e.what()<<'\n'; return 1; }
}
