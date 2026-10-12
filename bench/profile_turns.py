#!/usr/bin/env python3
"""Profile input-to-render turns prepared by the compiled bench.cpp scenarios.

Each case runs once. Uses avm-lldb's command interface, not its optional Python bindings. No save
files, serialization offsets, or save-version parsing are used.
"""

import argparse
import csv
from dataclasses import dataclass
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

CLOCK_HZ = 16_000_000
DEFAULT_GOAL_MS = 100.0
INVENTORY_RANGES = (("LONG_SWORD", "HAMMER_OF_RUIN", "WEAPONS"),
                    ("ARROWS", "ARROWS", "AMMO"),
                    ("CHAIN_MAIL", "TITAN_PLATE", "ARMORS"),
                    ("CANDLE", "TOWER_SHIELD", "OFFHANDS"),
                    ("RING_SEE_INVISIBLE", "RING_ILLUMINATION", "RINGS"),
                    ("AMULET_SPEED", "AMULET_BRILLIANCE", "AMULETS"),
                    ("WAND_FORCE", "WAND_LAST", "WANDS"),
                    ("POTION_HEALING", "POTION_FIRE", "POTIONS"),
                    ("SCROLL_IDENTIFY", "SCROLL_DETECT_OBJECTS", "SCROLLS"),
                    ("SPELLBOOK", "SPELLBOOK", "SPELLBOOKS"))


@dataclass(frozen=True)
class Benchmark:
    name: str
    description: str
    setup: str
    button: str
    item: str
    down: int
    up: int
    index: int


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_benchmarks(path):
    source = path.read_text(encoding="utf-8")
    entries = re.findall(r'^\s*X\((\w+),\s*"([^"\n]+)",\s*(\w+),\s*(\w+),\s*(\w+),\s*(\w+),\s*(\d+),\s*(\d+)\)', source, re.M)
    require(entries and len(entries) == len(re.findall(r'^\s*X\(', source, re.M)) and
            len({entry[0] for entry in entries}) == len(entries), "invalid compiled benchmark manifest")
    cases = tuple(Benchmark(name, description, setup, button, item, int(down), int(up), index)
                  for index, (name, description, setup, button, terrain, item, down, up) in enumerate(entries))
    require(all((case.setup in ("browse", "projectile_browse") and case.button in ("A", "DOWN", "UP") and
                 (case.button != "A" or not (case.down or case.up))) or
                (case.setup in ("play", "wait", "use", "drop", "wand", "pickup", "projectile", "feature", "feature_item", "spell", "spell_ray", "spell_target", "study") and
                 case.button in ("RIGHT", "A") and (not (case.down or case.up) or case.setup in ("use", "wand")))
                for case in cases), "unsupported benchmark input preparation")
    return cases


BENCHMARKS = read_benchmarks(Path(__file__).with_name("bench.cpp"))


def source_contract(source_dir):
    main_lines = (source_dir / "main.cpp").read_text(encoding="utf-8").splitlines()
    ui_lines = (source_dir / "ui.cpp").read_text(encoding="utf-8").splitlines()

    def idle_after(lines, marker):
        start = next(i for i, line in enumerate(lines) if marker in line)
        return next(i + 1 for i in range(start, len(lines)) if "avm_idle();" in lines[i])

    return {
        "main_idle": idle_after(main_lines, "Sleep until an interrupt"),
        "item_idle": idle_after(ui_lines, "uint8_t selection = view.first_slot();"),
        "yesno_idle": idle_after(ui_lines, "static bool yesno_modal("),
        "bench_sha256": hashlib.sha256((source_dir.parent / "bench/bench.cpp").read_bytes()).hexdigest(),
    }


def state_fields(case):
    # All reads use DWARF names; there is no dependency on Game byte layout.
    fields = {
        "valid": "game.valid", "hp": "game.hp", "turns": "game.turns",
        "x": "game.player.x", "y": "game.player.y", "score": "game.score", "rng": "game.random_state",
        "monster_type": "game.monsters[0].type", "monster_hp": "game.monsters[0].hp",
        "door_open": "(game.doors[0].pos.y & 0x80) != 0",
        "item_type": "game.inventory[0].type", "item_info": "game.inventory[0].info",
        "weapon": "game.weapon_slot", "armor": "game.armor_slot",
        "offhand": "game.offhand_slot", "blindness": "game.blindness", "sight": "game.see_invisible",
        "amulet": "game.amulet_slot", "ring0": "game.ring_slots[0]", "ring1": "game.ring_slots[1]",
        "ground_type": "game.ground[0].item.type",
        "digging_wall": "(game.walls[(16 * MAP_W + 35) >> 3] & (1u << (35 & 7))) != 0",
    }
    fields = {key: re.sub(r'\b(game|MAP_W)\b', r'rogue::\1', expr) for key, expr in fields.items()}
    if case.name.startswith(("trap_", "feature_", "monster_trap_", "visibility_trap_")):
        fields["feature_id"] = "rogue::game.features[0].id"
    if case.item != "NO_ITEM":
        fields["expected_item"] = "rogue::game.inventory[0].type == rogue::ItemType::" + case.item
    if case.index >= 87 or case.name.startswith("monster_trap_") or case.name in ("drink_sight", "throw_sight", "sight_expire_dense", "sight_cursed_trap"):
        fields["monster_x"] = "rogue::game.monsters[0].pos.x"
        fields["monster_y"] = "rogue::game.monsters[0].pos.y"
        for i in range(1, 4):
            fields[f"other_type_{i}"] = f"rogue::game.monsters[{i}].type"
            fields[f"other_hp_{i}"] = f"rogue::game.monsters[{i}].hp"
    if case.index >= 87:
        for name in ("haste", "levitation", "fire_resistance", "vamp_drain", "weakened", "confused", "slowed"):
            fields[name] = "rogue::game." + name
        fields["monster_stun"] = "rogue::game.monsters[0].stun"
        fields["object_map"] = "rogue::ui.mode == rogue::Mode::OBJECT_MAP"
    if case.name.startswith(("magic_", "study_")):
        fields["mana"]="rogue::game.mana"
        fields["study_progress"]="rogue::session.study_progress"
        fields["study_slot"]="rogue::session.study_slot"
        fields["monster_x"]="rogue::game.monsters[0].pos.x"
        fields["feature_id"]="rogue::game.features[0].id"
    if case.name == "scroll_traps":
        fields["feature_id"] = "rogue::game.features[0].id"
    if case.name.startswith("visibility_trap_"):
        fields["monster_x"] = "rogue::game.monsters[0].pos.x"
        fields["monster_y"] = "rogue::game.monsters[0].pos.y"
    if case.name == "storm_splash":
        for i in (1, 2, 3): fields[f"splash_hp_{i}"] = f"rogue::game.monsters[{i}].hp"
    if case.name == "glass_break":
        fields["weapon_empty"] = "rogue::game.weapon_slot == rogue::NONE"
    if case.name == "hammer_heavy":
        fields["monster_x"] = "rogue::game.monsters[0].pos.x"
    if case.name == "pickup_food":
        fields["food"] = "rogue::game.inventory[0].type == rogue::ItemType::FOOD"
    if case.name == "equip_cursed_amulet":
        index = "(POTION_FIRE - POTION_HEALING + 1 + SCROLL_DETECT_OBJECTS - SCROLL_IDENTIFY + 1 + RING_ILLUMINATION - RING_SEE_INVISIBLE + 1)"
        index = re.sub(r'\b[A-Z][A-Z_0-9]+\b', lambda m: "rogue::ItemType::" + m[0], index)
        fields["amulet_known"] = f"(rogue::game.identified_items[{index} >> 3] & (1u << ({index} & 7))) != 0"
        fields["cursed"] = "(rogue::game.inventory[0].info & rogue::ITEM_CURSED) != 0"
        fields["identified"] = "(rogue::game.inventory[0].info & rogue::ITEM_IDENTIFIED) != 0"
        fields["magnitude"] = "rogue::game.inventory[0].info & rogue::ITEM_VALUE_MASK"
        fields["amulet_empty"] = "rogue::game.amulet_slot == rogue::NONE"
    if case.name == "wand_digging":
        fields["charges"] = "rogue::game.inventory[0].info & rogue::WAND_CHARGE_MASK"
    if case.setup in ("browse", "projectile_browse"):
        fields["pack_size"] = "rogue::INVENTORY"
        fields["visible_rows"] = "rogue::INVENTORY_VISIBLE_ROWS"
        fields["group_count"] = "rogue::InventoryGroup::INVENTORY_GROUPS"
        # Read enum bounds separately: AVM LLDB cannot interpret nested selects.
        for index, (first, last, group) in enumerate(INVENTORY_RANGES):
            fields[f"group_first_{index}"] = "rogue::ItemType::" + first
            fields[f"group_last_{index}"] = "rogue::ItemType::" + last
            fields[f"group_id_{index}"] = "rogue::InventoryGroup::" + group
        fields["food_type"] = "rogue::ItemType::FOOD"
        fields["food_group"] = "rogue::InventoryGroup::FOODS"
        fields["quest_group"] = "rogue::InventoryGroup::QUEST_ITEMS"
        for slot in range(int(re.search(r"constexpr uint8_t INVENTORY = (\d+);", Path(__file__).parents[1].joinpath("src/model.hpp").read_text())[1])):
            item = f"rogue::game.inventory[{slot}]"
            fields[f"pack_type_{slot}"] = item + ".type"
            fields[f"pack_info_{slot}"] = item + ".info"
    return fields


def quote(path):
    # LLDB command parsing, never a shell command.
    return '"' + str(path).replace("\\", "/").replace('"', '\\"') + '"'


def make_commands(case, contract, folder, native=False, deadline_ms=10000):
    run = f"avm run-for {deadline_ms}ms"

    def press(button):
        return ["avm button set " + button, run, "avm button set", run]

    def snapshot():
        return [f"expr -- (unsigned int)({expr})" for expr in state_fields(case).values()]

    commands = [
        "breakpoint set --name bench_select", "run",
        f"expr -- (unsigned int)(bench_case = {case.index})", "breakpoint disable 1",
        f"breakpoint set --name bench_{case.name}", run,
        "expr -- (unsigned int)(bench_case)", "breakpoint disable 2",
        f"breakpoint set --file main.cpp --line {contract['main_idle']}", run,
        "avm time",  # Compiled setup and initial render have finished.
    ]
    if case.setup in ("spell", "spell_ray", "spell_target"):
        commands += press("B")
        for _ in range(7): commands += press("DOWN")
        commands += press("A")
        if case.setup in ("spell_ray", "spell_target"): commands += press("A")
    if case.setup == "wait":
        commands += press("B")
    if case.setup in ("use", "drop", "wand", "browse", "projectile", "projectile_browse"):
        commands += press("B")
        selection = 3 if case.setup in ("projectile", "projectile_browse") else 2 if case.setup == "drop" else 1
        for _ in range(selection):
            commands += press("DOWN")
    if case.setup in ("browse", "projectile_browse"):
        commands += ["breakpoint disable 3",
                     f"breakpoint set --file ui.cpp --line {contract['item_idle']}", "breakpoint list 4"]
        if case.button != "A":
            commands += press("A")
            for button, count in (("DOWN", case.down), ("UP", case.up)):
                for _ in range(count):
                    commands += press(button)
    if case.setup in ("use", "drop", "pickup", "wand", "projectile"):
        line = contract["yesno_idle"] if case.setup == "pickup" else contract["item_idle"]
        commands += [
            "breakpoint disable 3", f"breakpoint set --file ui.cpp --line {line}",
            "avm button set " + ("RIGHT" if case.setup == "pickup" else "A"), run,
            "avm button set", run,  # Poll release, ready for the final A edge.
        ]
        for button, count in (("DOWN", case.down), ("UP", case.up)):
            for _ in range(count):
                commands += press(button)
        commands += ["breakpoint disable 4", "breakpoint enable 3"]
        if case.setup in ("wand", "projectile"):
            commands += press("A")  # Confirm slot; stop after direction prompt is ready.
    if case.setup in ("feature", "feature_item"):
        commands += ["breakpoint disable 3", f"breakpoint set --file ui.cpp --line {contract['yesno_idle']}",
                     "avm button set RIGHT", run, "avm button set", run, "breakpoint disable 4"]
        if case.setup == "feature_item":
            commands += [f"breakpoint set --file ui.cpp --line {contract['item_idle']}",
                         "avm button set A", run, "avm button set", run, "breakpoint disable 5"]
        commands += ["breakpoint enable 3"]
    commands += [
        *snapshot(),
        *([f"avm display save {quote(folder / 'before.pgm')} --mode logical"] if case.setup in ("browse", "projectile_browse") else []),
        "avm time",
        "avm profile start" + (" --native" if native else ""),
        "avm button set " + case.button, run, "avm time", "avm profile stop",
        f"avm profile save {quote(folder / 'turn.avmp')}", "avm profile report --top 12",
        *snapshot(),
        # Read this named array after profiling to check mapping, regardless of its size.
        f"memory read --binary --outfile {quote(folder / 'explored.bin')} --size 1 "
        "--count `sizeof(rogue::game.explored)` `(unsigned int)&rogue::game.explored + 0x01000000`",
        f"memory read --binary --outfile {quote(folder / 'game.bin')} --size 1 "
        "--count `sizeof(rogue::game)` `(unsigned int)&rogue::game + 0x01000000`",
        # AVM LLDB can crash evaluating optimized inline arguments during a
        # backtrace. The saved profile already contains call-stack diagnostics;
        # ready/page PCs and typed snapshots validate the measurement endpoint.
        f"avm display save {quote(folder / 'frame.pgm')} --mode " + ("logical" if case.setup in ("browse", "projectile_browse") else "controller"),
    ]
    return "\n".join(commands) + "\n"


def records_from(output):
    return [json.loads(line) for line in output.splitlines()
            if line.startswith("{") and line.endswith("}")]


def validate_outcome(case, before, after, explored):
    expected_turns = before["turns"] if case.setup in ("browse", "projectile_browse") else (before["turns"] + 1) % 256
    require(after["turns"] == expected_turns, "expected no game turn while browsing" if case.setup in ("browse", "projectile_browse")
            else "expected one completed game turn")
    require(after["valid"] and after["hp"], "turn unexpectedly ended the run")
    if case.name.startswith("magic_"):
        require(after["mana"]<before["mana"],"spell did not spend mana")
    if case.name=="magic_phase": require(after["x"]==before["x"]+3 and after["hp"]<before["hp"],"Phase Step did not land on trap")
    if case.name=="magic_blood": require(after["hp"]==before["hp"]-3 and after["monster_hp"]<before["monster_hp"],"Blood Bolt sacrifice/effect absent")
    if case.name=="magic_fireburst": require(after["hp"]<before["hp"] and after["other_hp_1"]<before["other_hp_1"],"Fireburst cluster/self damage absent")
    if case.name=="magic_chain" or case.name.startswith("wand_lightning"):
        require(after["monster_hp"]<before["monster_hp"] and after["other_hp_1"]<before["other_hp_1"],"lightning chain absent")
    if case.name=="wand_lightning_powerful": require(after["other_hp_2"]<before["other_hp_2"],"powerful third target absent")
    if case.name=="magic_shockwave": require(after["monster_x"]>=35,"Shockwave failed multi-tile displacement")
    if case.name=="study_turn": require(after["study_progress"]==before["study_progress"]+1 and after["study_slot"]!=255,"study did not advance once")
    if case.name=="study_interrupt": require(after["study_slot"]==255 and after["study_progress"]==0,"study failed interruption")
    if case.name.startswith("visibility_trap_"):
        require((after['monster_x'],after['monster_y']) == (33,16) and after['monster_hp'] < before['monster_hp'],
                'monster did not enter and suffer the production trap')
        require(after['feature_id'] == (before['feature_id'] if case.name.endswith('_blind') else before['feature_id'] & ~1),
                'trap visibility/discovery changed')
    if case.setup in ("browse", "projectile_browse"):
        require(before == after, "inventory browsing changed game state")
        return
    position_before = (before["x"], before["y"])
    position_after = (after["x"], after["y"])
    if case.item != "NO_ITEM":
        require(before["expected_item"], "compiled scenario prepared the wrong item")
    if case.name.startswith("trap_"):
        require(after["feature_id"] == (before["feature_id"] & ~1), "trap was not revealed/retained")
    if case.name in ("feature_spring", "feature_forge"):
        require(after["feature_id"] == 0, "interactive feature was not consumed")
    if case.name == "feature_spring": require(after["hp"] > before["hp"], "spring did not heal")
    if case.name == "feature_holy": require(after["monster_hp"] < before["monster_hp"], "holy pulse did not damage monster")
    if case.name == "trap_phoenix": require(after["hp"] == 100 and after["amulet"] == 255, "trap resurrection failed")
    if case.name.startswith("monster_trap_"):
        require(after["feature_id"] == (before["feature_id"] & ~1), "monster landing trap not revealed/retained")
        if case.name in ("monster_trap_multi", "monster_trap_force", "monster_trap_hammer", "monster_trap_spread"):
            require(after["monster_hp"] < before["monster_hp"], "monster trap damage missing")
        if case.name in ("monster_trap_multi", "monster_trap_spread"):
            require(all(after[f"other_hp_{i}"] < before[f"other_hp_{i}"] for i in range(1,4)), "multiple monster trap damage missing")
        if case.name == "monster_trap_busy":
            require(after["other_hp_2"] < before["other_hp_2"], "busy monster trap damage missing")
        if case.name == "monster_trap_kills":
            require(after["monster_type"] == 0 and all(after[f"other_type_{i}"] == 0 for i in range(1,4)) and after["score"] == before["score"], "environmental kills/credit missing")
        if case.name == "monster_trap_teleport":
            require((after["monster_x"], after["monster_y"]) != (33,16) and after["monster_hp"] == before["monster_hp"], "monster teleport missing")
        if case.name == "monster_trap_summon":
            require(after["other_type_1"] != 0 and after["monster_hp"] == before["monster_hp"], "monster summons missing")
    if case.name.startswith("wand_lightning"):
        require(after["item_type"] == before["item_type"] and (after["item_info"] & 15) == (before["item_info"] & 15)-1,"lightning charge not consumed")
    if case.name.startswith("move_"):
        require(position_after == (before["x"] + 1, before["y"]), "player did not move right")
    if case.name == "attack_hit":
        require(0 < after["monster_hp"] < before["monster_hp"], "attack did not hit a surviving enemy")
    if case.name == "attack_miss":
        require(after["monster_hp"] == before["monster_hp"], "attack was not a miss")
    if case.name == "attack_kill":
        require(after["monster_type"] == 0 and after["score"] != before["score"], "enemy was not killed/scored")
    if case.name == "open_door":
        require(after["door_open"] and position_after == position_before, "door was not opened in place")
    if case.name in ("eat_food", "drink_healing", "scroll_mapping", "scroll_teleport", "drop_food"):
        require(after["item_type"] == 0, "consumable/drop was not used")
    if case.name in ("storm_splash", "glass_break", "hammer_heavy", "reprisal"):
        require(after["monster_hp"] < before["monster_hp"], "artifact scenario did not hit")
    if case.name == "storm_splash":
        require(after["hp"] < before["hp"] and all(after[f"splash_hp_{i}"] < before[f"splash_hp_{i}"] for i in (1, 2, 3)),
                "Stormbringer splash or life cost missing")
    if case.name == "glass_break":
        require(after["weapon_empty"] and not after["item_type"] and not after["item_info"], "Glass Sword did not shatter")
    if case.name == "reprisal": require(after["hp"] < before["hp"], "Reprisal attacker did not hit")
    if case.name == "hammer_heavy":
        require(after["monster_x"] > before["monster_x"], "Hammer target was not knocked back")
    if case.name == "phoenix":
        require(after["hp"] == 100 and not after["item_type"] and after["amulet"] == 255, "Phoenix Heart failed")
    if case.name in ("equip_light", "equip_shield"):
        require(after["offhand"] == 0 and after["expected_item"], "offhand was not equipped")
    if case.name == "drink_sight": require(19 <= after["sight"] <= 29, "temporary sight duration missing")
    if case.name == "sight_expire_dense": require(after["sight"] == 0, "temporary sight did not expire")
    if case.name == "throw_sight":
        require(after["monster_hp"] == before["monster_hp"] and after["sight"] == 0, "thrown sight had an effect")
    if case.name == "sight_cursed_trap": require(after["sight"] == 39 and after["ring0"] == 1, "cursed ring sight dose changed")
    if case.name == "fuel_out":
        require(after["offhand"] == 255 and after["item_type"] == 0, "exhausted torch remained equipped/carried")
    if case.name == "drink_blindness": require(7 <= after["blindness"] <= 14, "blindness duration missing")
    if case.name == "blind_recovery": require(after["blindness"] == 0, "blindness failed to expire")
    if case.name == "drink_healing":
        require(after["hp"] > before["hp"], "potion did not heal")
    if case.index >= 87 and case.item != "NO_ITEM" and not case.item.startswith("WAND_"):
        require(after["item_type"] == 0, "new consumable was not consumed")
    if case.name == "drink_extra_healing":
        require(after["hp"] >= 85 and not after["confused"] and not after["slowed"], "extra healing failed")
    if case.name == "drink_vitality":
        require(not after["vamp_drain"] and not after["weakened"] and after["hp"] == before["hp"], "vitality restoration failed")
    for name, minimum in (("haste", 14), ("levitation", 14), ("fire_resistance", 19)):
        if case.name == "drink_" + name:
            require(minimum <= after[name] <= minimum + 10, "temporary effect duration missing")
    if case.name == "effects_expire":
        require(all(not after[name] for name in ("haste", "levitation", "fire_resistance", "sight")), "temporary effects did not expire")
    if case.name == "scroll_hold": require(after["monster_stun"] > 0, "hold did not stun")
    if case.name == "scroll_traps": require(after["feature_id"] == 2, "trap discovery failed")
    if case.name == "scroll_summon": require(after["monster_type"] != 0, "adjacent summoning failed")
    if case.name == "scroll_mass_teleport": require((after["monster_x"],after["monster_y"]) != (before["monster_x"],before["monster_y"]), "mass teleport failed")
    if case.name in ("scroll_fire", "drink_fire"): require(after["hp"] < before["hp"], "self fire damage missing")
    if case.name == "throw_fire": require(after["monster_hp"] < before["monster_hp"], "fire splash damage missing")
    if case.name == "throw_fire_wall": require(after["hp"] == 100 and after["amulet"] == 255, "fire splash resurrection failed")
    if case.name == "scroll_objects": require(after["object_map"], "object map not displayed")
    if case.name == "natural_recovery":
        require(before["turns"] == 255 and after["hp"] == before["hp"] + 1,
                "natural recovery did not heal at clock wrap")
    for name, slot in (("equip_weapon", "weapon"), ("equip_bow", "weapon"), ("equip_armor", "armor")):
        if case.name == name:
            require(after[slot] == 0 and after["expected_item"], "item was not equipped")
    if case.name == "equip_ring":
        require(0 in (after["ring0"], after["ring1"]) and after["expected_item"], "ring was not equipped")
    if case.name == "equip_cursed_amulet":
        require(before["amulet_empty"] and before["cursed"] and not before["identified"] and
                before["magnitude"] == 1 and not before["amulet_known"],
                "cursed amulet must start unequipped with its type and curse unknown")
        require(after["amulet"] == 0 and after["expected_item"] and
                after["cursed"] and after["identified"] and after["magnitude"] == 1 and after["amulet_known"],
                "amulet was not equipped with its type and curse discovered")
    if case.name == "scroll_mapping":
        require(explored and all(byte == 0xff for byte in explored), "mapping did not reveal the map")
    if case.name == "scroll_teleport":
        require(position_after != position_before, "teleport did not move the player")
    if case.name == "drop_food":
        require(after["ground_type"] == before["item_type"], "dropped food is missing")
    if case.name == "wand_digging":
        require(after["charges"] == before["charges"] - 1 and before["digging_wall"] and not after["digging_wall"],
                "digging did not spend a charge and carve the wall")
    if case.setup == "projectile" and case.item == "ARROWS":
        require(before["item_info"] == 255 and after["item_info"] == 254, "arrow consumption/quantity wrong")
        if case.name == "arrow_hit": require(after["monster_hp"] < before["monster_hp"], "arrow failed to hit")
        if case.name == "arrow_miss": require(after["monster_hp"] == before["monster_hp"], "arrow failed to miss")
        if case.name == "arrow_kill": require(after["monster_type"] == 0 and after["score"] > before["score"], "arrow failed to kill")
    if case.name == "pickup_food":
        require(after["ground_type"] == 0 and after["food"], "food was not picked up")


def read_frame(path):
    # avm display save emits a binary 128x64 grayscale PGM without comments.
    magic, size, maximum, pixels = path.read_bytes().split(b"\n", 3)
    require((magic, size, maximum) == (b"P5", b"128 64", b"255") and len(pixels) == 128 * 64,
            "invalid display frame capture")
    return pixels


def inventory_row(frame, row, selected):
    pixels = frame[(12 + row * 7) * 128:(19 + row * 7) * 128]
    # Remove the selection's inverse colors before comparing scrolled content.
    return bytes(255 - value if selected and index % 128 >= 7 else value
                 for index, value in enumerate(pixels))


def validate_browsing(case, before, before_frame, after_frame):
    # Reconstruct grouped rows from the named inventory fields captured by LLDB.
    def group_for(slot):
        item_type = before[f"pack_type_{slot}"]
        return next((before[f"group_id_{index}"] for index in range(len(INVENTORY_RANGES))
                     if before[f"group_first_{index}"] <= item_type <= before[f"group_last_{index}"]),
                    before["food_group"] if item_type == before["food_type"] else before["quest_group"])

    rows = []
    for group in range(before["group_count"]):
        slots = [slot for slot in range(before["pack_size"]) if before[f"pack_type_{slot}"] and
                 group_for(slot) == group]
        if slots:
            rows += [before["pack_size"] + group, *slots]
    items = [slot for slot in rows if slot < before["pack_size"]]
    require(items, "inventory browsing fixture is empty")
    selected, top = 0, 0

    def advance(step):
        nonlocal selected, top
        selected = max(0, min(len(items) - 1, selected + step))
        row = rows.index(items[selected])
        top = min(max(0, len(rows) - before["visible_rows"]),
                  min(top, row) if row < top + before["visible_rows"]
                  else row - before["visible_rows"] + 1)

    for step, count in ((1, case.down), (-1, case.up)):
        for _ in range(count):
            advance(step)
    before_top = top
    before_row = rows.index(items[selected]) - top
    if case.button != "A":
        advance(1 if case.button == "DOWN" else -1)
        require(top != before_top, "inventory benchmark did not scroll the viewport")
    after_row = rows.index(items[selected]) - top

    def check_highlight(frame, expected):
        highlighted = [row for row in range(before["visible_rows"])
                       if all(frame[y * 128 + 7] == 255 for y in range(12 + row * 7, 19 + row * 7))]
        require(highlighted == [expected], "inventory selection highlight is on the wrong row")

    check_highlight(after_frame, after_row)
    require(before_frame != after_frame, "inventory input did not redraw a changed view")
    if case.button != "A":
        check_highlight(before_frame, before_row)
        # Visible rows shared by both viewports must move by the expected amount.
        for row in range(before["visible_rows"]):
            old_row = row + top - before_top
            if 0 <= old_row < before["visible_rows"]:
                require(inventory_row(after_frame, row, row == after_row) ==
                        inventory_row(before_frame, old_row, old_row == before_row),
                        "inventory viewport did not scroll to the intended grouped rows")


def validate_sample(case, contract, folder, output, more_pcs=(), completed_after=None):
    values = [int(value) for value in re.findall(r"\(unsigned int\) \$\d+ = (\d+)", output)]
    fields = list(state_fields(case))
    require(len(values) == 2 + 2 * len(fields) and values[:2] == [case.index, case.index],
            "missing compiled case selection or typed state capture")
    before = dict(zip(fields, values[2:2 + len(fields)]))
    after = dict(zip(fields, values[2 + len(fields):]))
    if completed_after is not None:
        require(set(completed_after) == set(fields), "missing completed-turn state fields")
        (folder / "boundary.json").write_text(json.dumps(after, indent=2) + "\n", encoding="utf-8")
        after = completed_after
    records = records_from(output)
    require(not any(record.get("ok") is False for record in records), "debugger command failed")
    stops = [record for record in records if "requested_cycles" in record]
    require(stops and all(stop["reason"] == "breakpoint" for stop in stops),
            "setup or turn did not reach its expected input boundary before the deadline")
    times = [record for record in records if "seconds_since_reset" in record]
    require(len(times) == 3, "missing ready/start/end clock records")
    ready, start, end = times
    if case.setup in ("browse", "projectile_browse"):
        modal_pcs = re.findall(r'^Breakpoint 4:.*address = (0x[0-9a-fA-F]+)', output, re.M)
        modal_pcs += re.findall(r'^\s*4\.\d+:.*address = (?:.*?\[)?(0x[0-9a-fA-F]+)', output, re.M)
        require(modal_pcs and end["pc"] in {int(pc, 16) for pc in modal_pcs},
                "stopped before inventory display returned to the modal input loop")
    else:
        require(end["pc"] == ready["pc"] or end["pc"] in more_pcs, "stopped before final render or displayed [more] input boundary")
    profile = json.loads((folder / "turn.avmp").read_text(encoding="utf-8"))
    window = profile["window"]
    cycles = int(window["end_cycle"]) - int(window["start_cycle"])
    require(window["complete"] and window["stop_reason"] == "breakpoint" and
            int(window["partial_cycles"]) == 0 and int(window["discontinuities"]) == 0,
            "profile window is incomplete or discontinuous")
    require(int(window["start_cycle"]) == start["cycles"] and
            int(window["end_cycle"]) == end["cycles"] and cycles > 0 and
            sum(int(row["cycles"]) for row in profile["pcs"]) == cycles,
            "profile and input-to-ready clock interval do not reconcile")
    renderer = "_ZN5rogue16render_inventoryE" if case.setup in ("browse", "projectile_browse") else "_ZN5rogue6renderEv"
    require(end["pc"] in more_pcs or any(row["linkage"].startswith(renderer) for row in profile["pcs"]),
            "the measured turn did not execute the final render")
    require(not any(("rogue::animate_" in row["function"] and case.setup not in ("projectile", "wand", "spell_ray") and case.name != "scroll_fire") or
                    row["linkage"] == "_ZN5rogue10make_floorEv" or
                    row["function"].lstrip(":").startswith("bench_") or
                    Path(row.get("file", "").replace("\\", "/")).name == "bench.cpp"
                    for row in profile["pcs"]),
            "setup, generation or animation occurred inside the measured turn")
    buttons = [record for record in records if "pressed_mask" in record and
               start["cycles"] <= record.get("cycle", -1) <= end["cycles"]]
    require(len(buttons) == 1 and buttons[0]["cycle"] == start["cycles"] and buttons[0]["pressed_mask"] != 0,
            "a measured turn must contain exactly one submitted input")
    validate_outcome(case, before, after, (folder / "explored.bin").read_bytes())
    if case.setup in ("browse", "projectile_browse"):
        validate_browsing(case, before, read_frame(folder / "before.pgm"), read_frame(folder / "frame.pgm"))
    for name, state in (("before", before), ("after", after)):
        (folder / (name + ".json")).write_text(json.dumps(state, indent=2) + "\n", encoding="utf-8")
    return {"cycles": cycles, "ms": cycles * 1000 / CLOCK_HZ,
            "start_cycle": start["cycles"], "end_cycle": end["cycles"],
            "start_pc": start["pc"], "end_pc": end["pc"],
            "profile": str(folder / "turn.avmp"), "identity": profile["identity"],
            "start_state_sha256": hashlib.sha256(json.dumps(before, sort_keys=True).encode()).hexdigest(),
            "display_hash": profile["end_display_hash"],
            "endpoint": "more" if end["pc"] in more_pcs else "ready"}


def run_paginated(case, contract, folder, lldb, elf, native, deadline_ms, timeout):
    """Measure every input-to-page/ready interval; acknowledge all status pages.

    Waits for acknowledgment lie outside profile windows. The final snapshot
    still validates the complete turn, including effects occurring after pages.
    """
    script = make_commands(case, contract, folder, native, deadline_ms)
    prefix, tail = script.split("avm profile start", 1)
    snapshot_start = tail.index("expr --")
    final_commands = tail[snapshot_start:]
    lines = (Path(contract["source_dir"]) / "status.cpp").read_text().splitlines()
    more_line = next(i + 1 for i, line in enumerate(lines) if "while(avm_buttons() & AVM_BUTTON_A)" in line)
    boundary_command = f"breakpoint set --file status.cpp --line {more_line}\nbreakpoint list"
    replay = ""
    segments, first, logs, ready_pc, more_pcs = [], None, [], None, set()
    for page in range(32):
        name = "turn" if page == 0 else f"page-{page}"
        button = case.button if page == 0 else "A"
        commands = prefix + boundary_command + "\n" + replay + "\n".join([
            "avm time" if page else "", "avm profile start" + (" --native" if native else ""),
            "avm button set " + button, f"avm run-for {deadline_ms}ms", "avm time", "avm profile stop",
            f"avm profile save {quote(folder / (name + '.avmp'))}", "avm profile report --top 12"]) + "\n" + (final_commands if page == 0 else final_commands.replace("frame.pgm", f"page-{page}-frame.pgm"))
        command_file = folder / (name + '.lldb')
        command_file.write_text(commands, encoding="utf-8")
        run = subprocess.run([str(lldb), "--batch", "--source", str(command_file), str(elf)],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=timeout)
        output = run.stdout
        logs.append(output)
        (folder / (name + '.log')).write_text(output, encoding="utf-8")
        require(run.returncode in (0, 1) and "error:" not in output and
                not any(r.get("ok") is False for r in records_from(output)), "debugger failed; see page log")
        clocks = [r for r in records_from(output) if "seconds_since_reset" in r]
        if page == 0:
            ready_pc = clocks[0]["pc"]
            more_id = int(re.search(r"\(lldb\) " + re.escape(boundary_command.splitlines()[0]) + r"\nBreakpoint (\d+):", output)[1])
            # The list includes all inline instances of this source breakpoint.
            more_pcs = {int(address,16) for address in re.findall(
                rf"^\s*{more_id}(?:\.\d+)?:.*address = (?:.*?\[)?(0x[0-9a-fA-F]+)", output, re.M)}
            more_pcs.update(int(address,16) for address in re.findall(
                rf"^Breakpoint {more_id}:.*address = (0x[0-9a-fA-F]+)", output, re.M))
            first = output
        start, end = clocks[-2:]
        profile = json.loads((folder / (name + '.avmp')).read_text())
        window = profile["window"]
        require(window["complete"] and window["stop_reason"] == "breakpoint" and
                int(window["partial_cycles"]) == 0 and int(window["discontinuities"]) == 0,
                "page profile window is incomplete")
        cycles = end["cycles"] - start["cycles"]
        require(cycles > 0 and int(window["start_cycle"]) == start["cycles"] and
                int(window["end_cycle"]) == end["cycles"] and
                sum(int(row["cycles"]) for row in profile["pcs"]) == cycles,
                "page clock and profile do not reconcile")
        require(end["pc"] == ready_pc or end["pc"] in more_pcs, "unexpected page input boundary")
        segments.append({"page": page, "cycles": cycles, "ms": cycles * 1000 / CLOCK_HZ,
                         "start_cycle": start["cycles"], "end_cycle": end["cycles"],
                         "start_pc": start["pc"], "end_pc": end["pc"],
                         "display_hash": profile["end_display_hash"],
                         "endpoint": "ready" if end["pc"] == ready_pc else "more",
                         "profile": str(folder / (name + '.avmp'))})
        require(run.returncode == 0 or end["pc"] in more_pcs, "debugger exited abnormally at final readiness")
        if end["pc"] == ready_pc: break
        replay += (f"avm button set {button}\navm run-for {deadline_ms}ms\n"
                   f"breakpoint disable {more_id}\navm button set\navm run-for 1ms\nbreakpoint enable {more_id}\n")
    else:
        raise ValueError("turn exceeded 32 status pages")
    # Validate the first measured interval and the independently captured final
    # state after all pages; retain the partial boundary state separately.
    values = [int(value) for value in re.findall(r"\(unsigned int\) \$\d+ = (\d+)", output)]
    fields = list(state_fields(case))
    require(len(values) == 2 + 2 * len(fields), "missing final page state capture")
    completed = dict(zip(fields, values[-len(fields):]))
    measurement = validate_sample(case, contract, folder, first, more_pcs, completed)
    worst = max(segments, key=lambda segment: segment["cycles"])
    measurement.update({key: value for key, value in worst.items() if key != "page"})
    measurement.update(computation_ms=sum(s["ms"] for s in segments), segments=segments)
    (folder / "lldb.log").write_text("\n".join(logs), encoding="utf-8")
    return measurement


def write_summary(folder, cases, goal_ms, elf, contract):
    summary = {"schema": 3, "metric": "input-to-ready or displayed [more] elapsed emulated cycles",
               "clock_hz": CLOCK_HZ, "goal_ms": goal_ms, "elf": str(elf),
               "source_bench_sha256": contract["bench_sha256"], "benchmarks": cases}
    (folder / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    with (folder / "summary.csv").open("w", newline="", encoding="utf-8") as output:
        writer = csv.writer(output)
        writer.writerow(("benchmark", "cycles", "ms", "goal_ms", "meets_goal"))
        for case in cases:
            writer.writerow((case["name"], case["cycles"], case["ms"], goal_ms, case["meets_goal"]))
    lines = [f"# Turn benchmarks: {goal_ms:g} ms limit", "",
             "Elapsed emulated time at 16 MHz, from submitted button to readiness or a displayed [more] prompt. Paginated turns report the largest segment; every page is acknowledged and the completed turn is validated.", "",
             "| Benchmark | Cycles | ms | Goal |", "| --- | ---: | ---: | --- |"]
    for case in cases:
        lines.append(f"| {case['name']} | {case['cycles']} | {case['ms']:.3f} | {'PASS' if case['meets_goal'] else 'OVER'} |")
    (folder / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, help="compiled ardurogue2-bench.elf")
    parser.add_argument("--lldb", type=Path, help="installed avm-lldb executable")
    parser.add_argument("--sdk-root", type=Path, help="SDK containing bin/avm-lldb")
    parser.add_argument("--source-dir", type=Path, default=Path(__file__).resolve().parents[1] / "src")
    parser.add_argument("--output", type=Path, default=Path("build/turn-benchmarks"), help="parent of a new, unique run directory")
    parser.add_argument("--benchmark", action="append", choices=[case.name for case in BENCHMARKS], help="select cases; may be specified multiple times")
    parser.add_argument("--goal-ms", type=float, default=DEFAULT_GOAL_MS)
    parser.add_argument("--deadline-ms", type=int, default=10000, help="emulated safety deadline, not the performance goal")
    parser.add_argument("--timeout", type=float, default=120, help="host seconds allowed for each debugger session")
    parser.add_argument("--native", action="store_true", help="also collect AVR interpreter hotspots")
    parser.add_argument("--html", action="store_true", help="render each profile with the SDK's avm-prof")
    parser.add_argument("--emit-only", action="store_true", help="write LLDB scripts without running")
    parser.add_argument("--check", action="store_true", help="exit 2 if any benchmark exceeds the goal")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args(argv)
    if args.list:
        for case in BENCHMARKS:
            print(f"{case.index:2} {case.name:22} {case.description}")
        return 0
    require(args.elf and args.elf.is_file(), "--elf must name an existing benchmark ELF")
    require(args.deadline_ms > 0 and args.timeout > 0 and
            math.isfinite(args.goal_ms) and args.goal_ms > 0, "budgets must be positive")
    suffix = ".exe" if sys.platform == "win32" else ""
    lldb = args.lldb or (args.sdk_root / "bin" / ("avm-lldb" + suffix) if args.sdk_root else shutil.which("avm-lldb"))
    require(args.emit_only or (lldb and Path(lldb).is_file()), "provide --lldb or --sdk-root, or put avm-lldb on PATH")
    contract = source_contract(args.source_dir.resolve())
    contract["source_dir"] = str(args.source_dir.resolve())
    cases = [case for case in BENCHMARKS if not args.benchmark or case.name in args.benchmark]
    args.output.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ-")
    folder = Path(tempfile.mkdtemp(prefix=stamp, dir=args.output.resolve()))
    print(f"Artifacts: {folder}", flush=True)
    results = []
    for case in cases:
        case_dir = folder / case.name
        case_dir.mkdir()
        commands = case_dir / "turn.lldb"
        commands.write_text(make_commands(case, contract, case_dir, args.native, args.deadline_ms), encoding="utf-8")
        if args.emit_only:
            continue
        try:
            if case.index >= 87 or case.name.startswith("monster_trap_") or case.name in ("drink_sight", "throw_sight", "sight_expire_dense", "sight_cursed_trap"):
                measurement = run_paginated(case, contract, case_dir, Path(lldb).resolve(), args.elf.resolve(),
                                            args.native, args.deadline_ms, args.timeout)
            else:
                run = subprocess.run([str(Path(lldb).resolve()), "--batch", "--source", str(commands), str(args.elf.resolve())],
                                     stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=args.timeout)
                (case_dir / "lldb.log").write_text(run.stdout, encoding="utf-8")
                require(run.returncode == 0, f"avm-lldb exited {run.returncode}; see {case_dir / 'lldb.log'}")
                measurement = validate_sample(case, contract, case_dir, run.stdout)
            meets_goal = measurement["ms"] <= args.goal_ms
            print(f"{case.name:22} {measurement['ms']:9.3f} ms  {'PASS' if meets_goal else 'OVER'}", flush=True)
            if args.html:
                profiler = Path(lldb).resolve().with_name("avm-prof" + suffix)
                subprocess.run([str(profiler), "report", measurement["profile"], "--html", str(case_dir / "profile.html")],
                               check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=args.timeout)
        except (ValueError, subprocess.SubprocessError) as error:
            (case_dir / "error.txt").write_text(str(error) + "\n", encoding="utf-8")
            raise ValueError(f"{case.name}: {error}; artifacts in {case_dir}") from error
        results.append({"name": case.name, "description": case.description,
                        **measurement, "meets_goal": meets_goal})
        write_summary(folder, results, args.goal_ms, args.elf.resolve(), contract)
    if args.emit_only:
        print("Run any generated turn.lldb with avm-lldb --batch --source <script> <elf>.")
        return 0
    failed = sum(not case["meets_goal"] for case in results)
    print(f"{len(results) - failed}/{len(results)} benchmarks meet {args.goal_ms:g} ms; {failed} over. Summary: {folder / 'summary.md'}")
    return 2 if args.check and failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, OSError, StopIteration) as error:
        print(f"turn benchmark error: {error}", file=sys.stderr)
        sys.exit(1)
