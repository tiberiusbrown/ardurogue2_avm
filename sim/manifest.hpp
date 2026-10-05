#pragma once
#include "simulator.hpp"
#include <filesystem>
namespace sim {
void write_manifest(const std::filesystem::path&, uint64_t start, uint64_t count,
                    unsigned jobs, bool all_seeds, const Options&, int argc, char** argv);
}
