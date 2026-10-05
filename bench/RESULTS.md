# Turn optimization results (2026-10-04)

Both approved optimizations are implemented: defer the dungeon render after
inventory/direction selection until an intermediate page, prompt, or animation
needs it, and render terrain using packed row masks and batched framebuffer
writes. The main loop still renders the completed turn. Status pagination
restores the dungeon only for its first page when needed; later pages clear
just the status rectangle with the existing filled-rectangle operation.

**All 21 cases passed the historical 150 ms goal**, compared with 9 passes and 12 failures
before these changes. The slowest case in this round was dense waiting at 118.145 ms,
leaving 31.855 ms to that limit. Each case runs once because emulated timing
is deterministic. Workloads, measurement boundaries, scenario source, and
interpreter firmware are unchanged. Generation, animation, and pagination
acknowledgement time remain outside these controlled benchmark scenarios.

The current [documented limit](README.md) is 100 ms. The later
[visibility and exploration changes](RESULTS-100MS.md) bring all 21 cases below it.

| Benchmark | Before ms | After ms | After cycles | Reduction |
| --- | ---: | ---: | ---: | ---: |
| move_room | 101.726 | 66.047 | 1,056,758 | 35.1% |
| move_corridor | 136.385 | 74.422 | 1,190,753 | 45.4% |
| move_map_edge | 36.682 | 32.946 | 527,130 | 10.2% |
| move_dense | 167.939 | 113.408 | 1,814,522 | 32.5% |
| wait | 91.977 | 66.221 | 1,059,541 | 28.0% |
| wait_dense | 172.359 | 118.145 | 1,890,325 | 31.5% |
| attack_hit | 97.859 | 71.859 | 1,149,736 | 26.6% |
| attack_miss | 103.332 | 77.332 | 1,237,306 | 25.2% |
| attack_kill | 97.010 | 71.009 | 1,136,149 | 26.8% |
| open_door | 141.685 | 82.155 | 1,314,480 | 42.0% |
| eat_food | 181.058 | 69.181 | 1,106,893 | 61.8% |
| drink_healing | 190.105 | 78.265 | 1,252,247 | 58.8% |
| equip_weapon | 183.205 | 71.327 | 1,141,238 | 61.1% |
| equip_armor | 182.266 | 70.389 | 1,126,225 | 61.4% |
| equip_ring | 185.956 | 74.079 | 1,185,264 | 60.2% |
| equip_cursed_amulet | 196.705 | 84.828 | 1,357,248 | 56.9% |
| scroll_mapping | 250.120 | 89.183 | 1,426,929 | 64.3% |
| scroll_teleport | 216.457 | 88.058 | 1,408,933 | 59.3% |
| drop_food | 185.706 | 72.598 | 1,161,566 | 60.9% |
| wand_digging | 279.621 | 91.758 | 1,468,131 | 67.2% |
| pickup_food | 110.214 | 74.779 | 1,196,463 | 32.2% |

Validation:

- All six native CTest entries pass, including the benchmark helper rejection
  tests. Renderer tests preserve eleven existing golden frame/exploration
  hashes and compare 280 terrain/clipping cases with an independent per-tile
  oracle. Pagination tests check first-page restoration, retention of the
  dungeon on later pages, status-rectangle clearing, and yes/no restoration.
- All 21 benchmark initial state hashes, named before/after state fields,
  complete exploration arrays, and captured controller frames exactly match
  the original run. The full final suite was run with `--check --html` and
  returned exit status 0.
- RelWithDebInfo production and benchmark builds pass. The production maximum
  stack bound is 247/256 bytes with zero analysis gaps; the benchmark bound is
  245/256 bytes. Production `.saved` is still 821 bytes, and `.data` is 102
  bytes (923/1,024 bytes total). Rendering scratch is transient and unsaved.
- The root `ardurogue2.arduboy` package is rebuilt.

Build identities:

- Before benchmark ELF SHA-256: `619e6bf520d1ccc520cd1a1786a2ab253c4e577db76b1effdfcdcd0df88bcbb0`.
- After benchmark ELF SHA-256: `45f3e639b6c9bbafa5fb714b126dfc8cbd754ec65910fd3b2f268ea0f9eb1f7e`.
- Scenario source SHA-256: `c9e2062e36cc915cb49d289986974b6b52eb049b7507ba0ffdaa1e4030fe0ec3`.
- Interpreter SHA-256: `659cd6ebf0d8c7b315bb0ec5bf58f2dc533b25410f7d4035cca6144368abc754`.

Raw measurements and per-case profiles are retained in the ignored build tree:
[before](../build/turn-benchmarks/20261005T005321Z-2am3kb_k/summary.md) and
[after](../build/turn-benchmarks/20261005T011922Z-gq475bsa/summary.md).
See [README.md](README.md) for reproduction commands and measurement scope.
