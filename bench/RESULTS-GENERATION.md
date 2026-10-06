# Dungeon generation optimization results — 2026-10-06

Nine individually beneficial changes were retained. Their combined result reduces the mean over the nine measured cases from **9.853 to 5.795 seconds (41.2% less time)**. Generated terrain, stairs, entities, item data and gameplay RNG are unchanged. No commits or pushes were made.

## Individual experiments

Every experiment started from the same original generator, with the other proposed optimizations removed. Each successful build ran native correctness checks, a byte-for-byte comparison of 16,384 floors, and nine complete production AVM generation profiles. Times are elapsed emulated cycles divided by 16 MHz, including loading feedback. Host runtime is not the metric. The mean weights these nine selected cases equally; it is not a population estimate.

| Change tested alone | Mean reduction | Decision |
| --- | ---: | --- |
| Skip impossible dead-end placement pass | 12.48% | Retained |
| Reject geometry before occupancy/spacing | 9.29% | Retained |
| Cache invariant loop floor masks | 6.09% | Retained |
| Limit propagation to reachable rows | 7.35% | Retained |
| Use AVM scratch word loads/stores | 2.80% | Retained |
| Swap propagation planes | 0.20% slower | Discarded |
| Stop propagation at a fixed point | 3.77% slower | Discarded |
| Flash socket rows and analytic rectangle sockets | 4.32% | Retained |
| Constant feature-weight totals | 3.55% | Retained |
| Cache stair eligibility | 1.47% | Retained |
| Validate packed transformed clearance rows | 5.07% | Retained |

The effects overlap and should not be added. Both plane swapping and the measured fixed-point implementation regressed in every selected case.

The first inline fixed-point implementation hit negative `R_AVM_DATA16` relocations in the installed SDK. A version with a separate row-step function built and was benchmarked; its 3.77% regression is the reported result. The initial socket-table compile also needed flash-qualified references to avoid copying objects across AVM address spaces. Failed attempts and their diagnostics remain in the artifact directory; neither is present in the retained generator.

## Combined generation latency

Floor numbers are zero-based. The stress case was selected for having the highest feature-attempt count among 512 native descent samples; it is not a global worst-case guarantee.

| Seed / floor / direction | Style | Before seconds | After seconds | Reduction |
| --- | --- | ---: | ---: | ---: |
| `0x0000` / 0 / descent | WARREN | 6.631 | 3.670 | 44.7% |
| `0x0003` / 3 / descent | CHAMBERS | 9.052 | 4.662 | 48.5% |
| `0x0006` / 6 / descent | FORTRESS | 13.119 | 7.854 | 40.1% |
| `0x0001` / 1 / descent | RUINS | 14.618 | 6.499 | 55.5% |
| `0xffff` / 15 / descent | RUINS | 5.985 | 3.552 | 40.6% |
| `0xffff` / 15 / ascent | CHAMBERS | 6.455 | 4.525 | 29.9% |
| `0x1234` / 7 / descent | WARREN | 3.431 | 2.760 | 19.6% |
| `0x1234` / 7 / ascent | FORTRESS | 9.283 | 6.406 | 31.0% |
| `0x00b5` / 5 / descent | FORTRESS stress | 20.103 | 12.226 | 39.2% |

The high-retry Fortress case drops from 20.103 to 12.226 seconds; the loop-heavy Ruins case drops from 14.618 to 6.499 seconds. All nine combined cases improve.

## Correctness and resources

- **14/14 native CTest checks pass** on the final build, including simulator and balance-tool checks. Intermediate experiments passed the nine game/generation/UI/render/helper checks; the first two also ran all fourteen.
- **16,384 floors match the original byte for byte** for every completed individual experiment and the combination: seeds 0..511, all sixteen depths, ascent and descent. Each record is the existing 701-byte generated-state serialization plus two gameplay-RNG bytes. The new native `--floor-corpus SEED_COUNT OUTPUT` command enables this cross-build comparison.
- **131,072 final floors pass bulk properties**: seeds 0..4095, all depths and both travel directions. Connectivity, loop topology, doors, stairs, bounds, occupants, scratch cleanup, deterministic snapshots and RNG isolation pass. The results retain 55,974 distinct sampled layout seeds and 55,974 terrain hashes.
- **Production AVM output matches the native oracle and the original generated state** for all nine timed cases. Every full generation profile is complete, with zero partial cycles and zero discontinuities.
- **Six emulator loading-cadence audits pass**, covering all four styles plus final descent/ascent. Periodic redraw intervals are 149.37–166.65 ms, within the required 100–200 ms; final redraws remain immediate.
- **36/36 complete-turn benchmarks meet 100 ms** on the final benchmark ELF. Slowest: `arrow_kill`, **94.371 ms**. Generation remains outside that turn limit.
- Production and benchmark stack bounds are both **244/256 bytes**, complete with **zero analysis gaps**.

| Resource | Original | Combined | Change |
| --- | ---: | ---: | ---: |
| Saved Game | 773 B | 773 B | 0 |
| Other permanent data | 103 B | 103 B | 0 |
| Total permanent RAM | 876 B | 876 B | 0 |
| Code `.text` | 60,264 B | 61,218 B | +954 B |
| Constants `.rodata` | 7,011 B | 8,307 B | +1,296 B |

The total flash growth is **2,250 bytes**. Floor-mask caching temporarily uses 92 bytes of the not-yet-populated monster array and clears it before population. Stair eligibility reuses explored scratch. Socket and bit-reversal tables stay in flash. No additional persistent fields, room records or stack arrays were introduced.

The old MinGW native configuration could not build existing C++17 inline variables; validation used the existing `build/bow-native` Clang configuration. Native AVR attribution was not enabled because the installed interpreter ELF/HEX mismatch was already established during profiling. Source/cycle profiles are retained.

## Reproduction and artifacts

From the project directory, with the current installed SDK and existing configurations:

```text
cmake --build build/avm-ninja --config RelWithDebInfo --target ardurogue2 ardurogue2_bench --parallel
cmake --build build/bow-native --config RelWithDebInfo --parallel
ctest --test-dir build/bow-native -C RelWithDebInfo --output-on-failure
build/bow-native/tests/game_native.exe --floor-bulk 4096
build/bow-native/tests/game_native.exe --floor-corpus 512 build/floor-corpus.bin
python tools/check_floor_avm.py --elf build/ardurogue2.elf --native build/bow-native/tests/game_native.exe --sdk-root ../../build/avm-sdk-install --seed 1 --floor 1 --profile --check-loading --output build/generation-recheck
python bench/profile_turns.py --elf build/ardurogue2-bench.elf --sdk-root ../../build/avm-sdk-install --check --output build/turn-benchmarks
```

All experiment snapshots, native binaries, ELF files, CTest/build logs, full source profiles, generated-state captures, corpus files, decisions and comparison JSON remain under `build/dungeon-experiments-20261006/`. The original full profiles are under `build/dungeon-profiling-20261006/`. The final folder is `combined/`; it also contains `loading-cadence/`, the 131,072-floor bulk output and the final turn-benchmark run. Experiment helpers restore the original source before applying each isolated trial; their source snapshots document exactly what was measured.

Original production ELF SHA-256: `6ce3337dbdb06f853332d79471e66da39427c173f5ac0ea083c0b1e9d6686a47`.

Final production ELF SHA-256: `7163c51e3ec8b719db77dc548f49817f9ca6016c70a8056397549ff1692e5c96`.

Final benchmark ELF SHA-256: `0d5fc8dd5f149b656a620ffde985fb81e14fbd9bdbece1641e0b14b0ad4dbaef`.

Original/final native corpus SHA-256: `5b660b29db60bce1f578ad0350ac02e97c90d0370e614081bee8794a29ecca25`.
