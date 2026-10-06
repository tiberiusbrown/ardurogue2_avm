#include "manifest.hpp"
#include "build_metadata.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace sim {
namespace {
std::string json(const std::string& s) {
    std::ostringstream o; o<<'"';
    for(unsigned char c:s) {
        if(c=='"' || c=='\\') o<<'\\'<<c;
        else if(c<32) o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;
        else o<<c;
    }
    o<<'"'; return o.str();
}
}
void write_manifest(const std::filesystem::path& path,uint64_t start,uint64_t count,
                    unsigned jobs,bool all_seeds,const Options& options,int argc,char** argv) {
    std::ofstream o(path);
    o<<"{\n  \"schema_version\": 1,\n  \"telemetry_schema_version\": "<<TELEMETRY_SCHEMA_VERSION
     <<",\n  \"git_sha\": "<<json(SIM_GIT_SHA)<<",\n  \"git_dirty\": "<<SIM_GIT_DIRTY
     <<",\n  \"agent_policy_hash\": "<<json(SIM_AGENT_POLICY_HASH)
     <<",\n  \"agent\": \"omniscient-v2\",\n  \"seed_selection\": {\"first\": "<<start
     <<", \"last\": "<<start+count-1<<", \"all_seeds\": "<<(all_seeds ? "true" : "false")
     <<"},\n  \"effective_seed_count\": "<<count
     <<",\n  \"experiment\": "<<json(options.experiment_id)<<",\n  \"variant\": "<<json(options.variant)
     <<",\n  \"build_type\": "<<json(SIM_BUILD_TYPE)<<",\n  \"compiler\": "<<json(SIM_COMPILER)
     <<",\n  \"options\": {\"max_actions\": "<<options.max_actions<<", \"telemetry\": "<<(options.telemetry ? "true" : "false")
     <<", \"jobs\": "<<jobs<<"},\n  \"interventions\": [";
    for(size_t i=0;i<options.intervention_rules.size();++i) { if(i) o<<", "; o<<json(options.intervention_rules[i]); }
    o<<"],\n  \"entry_state_schema_version\": "<<(options.entry_state ? ENTRY_STATE_SCHEMA_VERSION : 0)
     <<",\n  \"command\": [";
    for(int i=0;i<argc;++i) { if(i) o<<", "; o<<json(argv[i]); }
    o<<"]\n}\n";
    if(!o) throw std::runtime_error("manifest write failed");
}
}
