#pragma once
#include "explorer_parameters.hpp"
#include <string>
namespace sim {
std::string sha256(const std::string&);
std::string parameter_hash(const ExplorerParameters&);
std::string explorer_policy_hash(const std::string& code_hash,const ExplorerParameters&);
}
