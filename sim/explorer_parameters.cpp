#include "explorer_parameters.hpp"
#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
namespace sim {
void validate_parameters(const ExplorerParameters& p) {
    for(auto& d:parameter_definitions) if(p.*(d.member)<d.minimum || p.*(d.member)>d.maximum)
        throw std::runtime_error(std::string("explorer parameter out of range: ")+d.name);
}
ExplorerParameters parse_parameters(const std::string& json) {
    ExplorerParameters p; size_t at=0; std::set<std::string> seen;
    auto space=[&] { while(at<json.size() && std::isspace(static_cast<unsigned char>(json[at]))) ++at; };
    auto expect=[&](char c) { space(); if(at==json.size() || json[at++]!=c) throw std::runtime_error("invalid explorer parameter JSON"); };
    expect('{'); space();
    if(at<json.size() && json[at]=='}') ++at;
    else while(true) {
        expect('"'); size_t start=at;
        while(at<json.size() && json[at]!='"' && json[at]!='\\') ++at;
        if(at==json.size() || json[at]!='"') throw std::runtime_error("invalid parameter name");
        std::string name=json.substr(start,at-start); ++at; expect(':'); space(); start=at;
        if(at<json.size() && json[at]=='-') ++at;
        size_t digits=at;
        while(at<json.size() && std::isdigit(static_cast<unsigned char>(json[at]))) ++at;
        if(digits==at) throw std::runtime_error("parameter value must be an integer");
        std::string number=json.substr(start,at-start);
        if(at-digits>1 && json[digits]=='0') throw std::runtime_error("invalid parameter integer");
        int value=std::stoi(number); bool found=false;
        if(!seen.insert(name).second) throw std::runtime_error("duplicate explorer parameter: "+name);
        for(auto& d:parameter_definitions) if(name==d.name) { p.*(d.member)=value; found=true; break; }
        if(!found) throw std::runtime_error("unknown explorer parameter: "+name);
        space(); if(at<json.size() && json[at]=='}') { ++at; break; }
        expect(',');
    }
    space(); if(at!=json.size()) throw std::runtime_error("trailing parameter JSON content");
    validate_parameters(p); return p;
}
ExplorerParameters load_parameters(const std::string& path) {
    std::ifstream input(path); if(!input) throw std::runtime_error("cannot open explorer parameters: "+path);
    std::ostringstream json; json<<input.rdbuf(); if(input.bad()) throw std::runtime_error("cannot read explorer parameters");
    return parse_parameters(json.str());
}
std::string parameter_json(const ExplorerParameters& p) {
    validate_parameters(p); std::ostringstream json; json<<"{\n";
    for(size_t i=0;i<EXPLORER_PARAMETER_COUNT;++i) {
        auto& d=parameter_definitions[i]; json<<"  \""<<d.name<<"\": "<<p.*(d.member)<<(i+1==EXPLORER_PARAMETER_COUNT ? "\n" : ",\n");
    }
    json<<"}\n"; return json.str();
}
}
