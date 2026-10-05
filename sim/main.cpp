#include "simulator.hpp"
#include "parallel.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
uint64_t number(const std::string& value) {
    if(value.empty() || value[0] == '-' || value[0] == '+') throw std::runtime_error("invalid unsigned number: "+value);
    size_t used=0; auto n=std::stoull(value,&used,0);
    if(used != value.size()) throw std::runtime_error("invalid number: "+value);
    return n;
}
}
int main(int argc, char** argv) {
    try {
        uint64_t start=1, count=1, jobs=1; bool trace=false, selected=false, count_selected=false;
        std::string output;
        sim::Options options;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];
            auto value=[&]() { if(i+1>=argc) throw std::runtime_error("missing value for "+arg); return std::string(argv[++i]); };
            if(arg=="--help") {
                std::cout << "ardurogue2_sim [--seed N | --seeds FIRST:LAST | --count N --start-seed N]\n"
                    "  [--output DIRECTORY] [--jobs N] [--trace] [--max-actions N] [--no-telemetry]\n"
                    "Seeds are unsigned 16-bit; ranges are inclusive. Trace goes to stderr.\n"
                    "Without --output, runs.csv goes to stdout. With --output all four CSVs are written.\n"
                    "--jobs uses 1..64 isolated processes (default 1); CSV remains in seed order.\n";
                return 0;
            } else if(arg=="--seed" || arg=="--seeds") {
                if(selected || count_selected) throw std::runtime_error("conflicting seed selection"); selected=true;
                std::string v=value();
                if(arg=="--seed") start=number(v);
                else {
                    auto colon=v.find(':'); if(colon==std::string::npos) throw std::runtime_error("range requires FIRST:LAST");
                    start=number(v.substr(0,colon)); uint64_t last=number(v.substr(colon+1));
                    if(last<start || last>65535) throw std::runtime_error("invalid seed range"); count=last-start+1;
                }
            } else if(arg=="--start-seed") { if(selected) throw std::runtime_error("conflicting seed selection"); count_selected=true; start=number(value()); }
            else if(arg=="--count") { if(selected) throw std::runtime_error("conflicting seed selection"); count_selected=true; count=number(value()); }
            else if(arg=="--output") output=value();
            else if(arg=="--max-actions") options.max_actions=number(value());
            else if(arg=="--jobs") jobs=number(value());
            else if(arg=="--trace") trace=true;
            else if(arg=="--no-telemetry") options.telemetry=false;
            else throw std::runtime_error("unknown option: "+arg);
        }
        if(start>65535 || count==0 || count>65536 || start+count>65536) throw std::runtime_error("seed selection exceeds 0..65535");
        if(jobs==0 || jobs>64) throw std::runtime_error("jobs must be in 1..64");
        if(trace && count != 1) throw std::runtime_error("trace requires one seed");
        if(trace) options.trace=&std::cerr;
        std::ofstream runs,floors,items,monsters;
        std::ostream* summary=&std::cout;
        if(!output.empty()) {
            std::filesystem::create_directories(output);
            auto open=[&](std::ofstream& f,const char* name) { f.open(std::filesystem::path(output)/name); if(!f) throw std::runtime_error(std::string("cannot open ")+name); };
            open(runs,"runs.csv"); open(floors,"floors.csv"); open(items,"items.csv"); open(monsters,"monsters.csv");
            summary=&runs; sim::write_floors_header(floors); sim::write_items_header(items); sim::write_monsters_header(monsters);
        }
        sim::write_runs_header(*summary);
        uint64_t escaped=0,deaths=0,stuck=0;
        if(jobs>1 && count>1) {
            auto totals=sim::parallel_batch(argv[0],start,count,static_cast<unsigned>(jobs),options,*summary,
                output.empty() ? nullptr : &floors,output.empty() ? nullptr : &items,output.empty() ? nullptr : &monsters);
            escaped=totals.escaped; deaths=totals.deaths; stuck=totals.stuck;
        } else {
            sim::OmniscientAgent agent;
            for(uint64_t i=0;i<count;++i) {
                auto r=sim::run(static_cast<uint16_t>(start+i),agent,options);
                sim::write_run(*summary,r);
                if(!output.empty()) { sim::write_floors(floors,r); sim::write_items(items,r); sim::write_monsters(monsters,r); }
                escaped+=r.result=="escaped"; deaths+=r.result=="death"; stuck+=r.stuck;
            }
        }
        if(!*summary || (!output.empty() && (!floors || !items || !monsters))) throw std::runtime_error("output write failed");
        std::cerr << "runs=" << count << " escaped=" << escaped << " death=" << deaths << " simulator_failures=" << stuck << '\n';
        return 0;
    } catch(const std::exception& e) { std::cerr << "ardurogue2_sim: " << e.what() << '\n'; return 1; }
}
