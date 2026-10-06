#include "simulator.hpp"
#include "parallel.hpp"
#include "manifest.hpp"
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
        uint64_t start=1, count=1, jobs=1; bool trace=false, selected=false, count_selected=false, all_seeds=false;
        std::string output;
        sim::Options options;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];
            auto value=[&]() { if(i+1>=argc) throw std::runtime_error("missing value for "+arg); return std::string(argv[++i]); };
            if(arg=="--help") {
                std::cout << "ardurogue2_sim [--seed N | --seeds FIRST:LAST | --all-seeds | --count N --start-seed N]\n"
                    "  [--output DIRECTORY] [--jobs N] [--trace] [--max-actions N] [--no-telemetry] [--entry-state]\n"
                    "Seeds are unsigned 16-bit; ranges are inclusive. Trace goes to stderr.\n"
                    "Without --output, runs.csv goes to stdout. With --output eight CSVs and manifest.json are written.\n"
                    "--entry-state adds entry_state.csv for balance reports; requires --output and telemetry.\n"
                    "  [--experiment ID] [--variant ID] [--intervention RULE] (repeatable)\n"
                    "RULE: replace-item:FROM:TO[:floor=N][:direction=descent|ascent][:max=N][:info=default|preserve|BYTE]\n"
                    "Also replace-monster, remove-item:FROM and remove-monster:FROM.\n"
                    "--jobs uses 1..64 isolated processes (default 1); CSV remains in seed order.\n";
                return 0;
            } else if(arg=="--all-seeds") {
                if(selected || count_selected) throw std::runtime_error("conflicting seed selection");
                selected=all_seeds=true; start=1; count=65535;
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
            else if(arg=="--entry-state") options.entry_state=true;
            else if(arg=="--experiment") options.experiment_id=value();
            else if(arg=="--variant") options.variant=value();
            else if(arg=="--intervention") options.intervention_rules.push_back(value());
            else throw std::runtime_error("unknown option: "+arg);
        }
        if(start>65535 || count==0 || count>65536 || start+count>65536) throw std::runtime_error("seed selection exceeds 0..65535");
        if(start==0 && start+count>0xace1) throw std::runtime_error("duplicate effective seed: requested 0 and 44257");
        if(jobs==0 || jobs>64) throw std::runtime_error("jobs must be in 1..64");
        if(trace && count != 1) throw std::runtime_error("trace requires one seed");
        if(trace) options.trace=&std::cerr;
        if(!options.intervention_rules.empty()) {
            auto experiment=std::make_shared<sim::RuleExperiment>();
            for(const auto& rule:options.intervention_rules) experiment->rules.push_back(sim::parse_rule(rule));
            options.experiment=experiment;
        }
        if(options.entry_state && (output.empty() || !options.telemetry))
            throw std::runtime_error("--entry-state requires --output and telemetry");
        std::ofstream entry_state;
        if(options.entry_state) {
            std::filesystem::create_directories(output);
            if(std::filesystem::exists(std::filesystem::path(output)/"runs.csv"))
                throw std::runtime_error("output already contains runs.csv; choose a fresh directory");
            entry_state.open(std::filesystem::path(output)/"entry_state.csv");
            if(!entry_state) throw std::runtime_error("cannot open entry_state.csv");
            sim::write_entry_state_header(entry_state);
        }
        std::array<std::ofstream,sim::CSV_STREAM_COUNT> files;
        std::array<std::ostream*,sim::CSV_STREAM_COUNT> outputs{}; outputs[0]=&std::cout;
        if(!output.empty()) {
            std::filesystem::create_directories(output);
            if(std::filesystem::exists(std::filesystem::path(output)/"runs.csv")) throw std::runtime_error("output already contains runs.csv; choose a fresh directory");
            for(size_t s=0;s<files.size();++s) {
                files[s].open(std::filesystem::path(output)/sim::csv_streams[s].name);
                if(!files[s]) throw std::runtime_error("cannot open output CSV"); outputs[s]=&files[s];
            }
        }
        for(size_t s=0;s<outputs.size();++s) if(outputs[s]) sim::csv_streams[s].header(*outputs[s]);
        uint64_t escaped=0,deaths=0,stuck=0;
        if(jobs>1 && count>1) {
            auto totals=sim::parallel_batch(argv[0],start,count,static_cast<unsigned>(jobs),options,outputs,
                options.entry_state ? &entry_state : nullptr);
            escaped=totals.escaped; deaths=totals.deaths; stuck=totals.stuck;
        } else {
            sim::OmniscientAgent agent;
            for(uint64_t i=0;i<count;++i) {
                auto r=sim::run(static_cast<uint16_t>(start+i),agent,options);
                for(size_t s=0;s<outputs.size();++s) if(outputs[s]) sim::csv_streams[s].rows(*outputs[s],r);
                if(options.entry_state) sim::write_entry_state(entry_state,r);
                escaped+=r.result=="escaped"; deaths+=r.result=="death"; stuck+=r.stuck;
            }
        }
        for(auto* stream:outputs) if(stream) {
            stream->flush();
            if(!*stream) throw std::runtime_error("output write failed");
        }
        if(options.entry_state) {
            entry_state.flush();
            if(!entry_state) throw std::runtime_error("entry_state.csv write failed");
        }
        if(!output.empty()) sim::write_manifest(std::filesystem::path(output)/"manifest.json",start,count,unsigned(jobs),all_seeds,options,argc,argv);
        std::cerr << "runs=" << count << " escaped=" << escaped << " death=" << deaths << " simulator_failures=" << stuck << '\n';
        return 0;
    } catch(const std::exception& e) { std::cerr << "ardurogue2_sim: " << e.what() << '\n'; return 1; }
}
