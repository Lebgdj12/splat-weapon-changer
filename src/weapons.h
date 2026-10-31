// weapons.h
// Splatoon 1 weapon ID tables (EUR/US v2.12.0)
// Source: SHRL-Red/Splatoon-HaxPack AGME01.xml comment list
//
// Memory layout in player struct (accessed via [0x106E46E8] -> +0x290 -> +offset):
//   +0x78 = main weapon ID (uint32)
//   +0x7C = sub  weapon ID (uint32)
//   +0x80 = special weapon ID (uint32) [unverified, presumed from struct ordering]
//   +0x88 = ink tank variant     [confirmed in Red's XML]

#pragma once
#include <cstdint>

namespace splat {

struct WeaponEntry {
    uint32_t id;
    const char* name;
};

// --- MAIN WEAPONS (0x00 .. 0x67) ---
// Note: there's a gap 0x1A..0x1F (unused IDs in the original game)
// Use 0xFFFFFFFF for "No Weapon" (Inkling with bare hands, fun mode)
constexpr uint32_t MAIN_WEAPON_NONE = 0xFFFFFFFFu;

const WeaponEntry MAIN_WEAPONS[] = {
    { 0x00, "Sploosh-o-Matic" },
    { 0x01, "Neo Sploosh-o-Matic" },
    { 0x02, "Sploosh-o-Matic 7" },
    { 0x03, "Splattershot Jr." },
    { 0x04, "Custom Splattershot Jr." },
    { 0x05, "Splash-o-Matic" },
    { 0x06, "Neo Splash-o-Matic" },
    { 0x07, "Aerospray MG" },
    { 0x08, "Aerospray RG" },
    { 0x09, "Aerospray PG" },
    { 0x0A, "Splattershot" },
    { 0x0B, "Tentatek Splattershot" },
    { 0x0C, "Wasabi Splattershot" },
    { 0x0D, "Hero Shot Replica" },
    { 0x0E, "Octoshot Replica" },
    { 0x0F, ".52 Gal" },
    { 0x10, ".52 Gal Deco" },
    { 0x11, "N-ZAP 85" },
    { 0x12, "N-ZAP 89" },
    { 0x13, "N-ZAP 83" },
    { 0x14, "Splattershot Pro" },
    { 0x15, "Forge Splattershot Pro" },
    { 0x16, "Berry Splattershot Pro" },
    { 0x17, ".96 Gal" },
    { 0x18, ".96 Gal Deco" },
    { 0x19, "Dual Squelcher" },
    { 0x20, "Custom Blaster" },
    { 0x21, "Range Blaster" },
    { 0x22, "Custom Range Blaster" },
    { 0x23, "Grim Range Blaster" },
    { 0x24, "Rapid Blaster" },
    { 0x25, "Rapid Blaster Deco" },
    { 0x26, "Rapid Blaster Pro" },
    { 0x27, "Rapid Blaster Pro Deco" },
    { 0x28, "L-3 Nozzlenose" },
    { 0x29, "L-3 Nozzlenose D" },
    { 0x2A, "H-3 Nozzlenose" },
    { 0x2B, "H-3 Nozzlenose D" },
    { 0x2C, "Cherry H-3 Nozzlenose" },
    { 0x2D, "Carbon Roller" },
    { 0x2E, "Carbon Roller Deco" },
    { 0x2F, "Splat Roller" },
    { 0x30, "Krak-On Splat Roller" },
    { 0x31, "CoroCoro Splat Roller" },
    { 0x32, "Hero Roller Replica" },
    { 0x33, "Dynamo Roller" },
    { 0x34, "Gold Dynamo Roller" },
    { 0x35, "Tempered Dynamo Roller" },
    { 0x36, "Inkbrush" },
    { 0x37, "Inkbrush Nouveau" },
    { 0x38, "Permanent Inkbrush" },
    { 0x39, "Octobrush" },
    { 0x3A, "Octobrush Nouveau" },
    { 0x3B, "Classic Squiffer" },
    { 0x3C, "New Squiffer" },
    { 0x3D, "Fresh Squiffer" },
    { 0x3E, "Splat Charger" },
    { 0x3F, "Kelp Splat Charger" },
    { 0x40, "Bento Splat Charger" },
    { 0x41, "Hero Charger Replica" },
    { 0x42, "Splatterscope" },
    { 0x43, "Kelp Splatterscope" },
    { 0x44, "Bento Splatterscope" },
    { 0x45, "E-Liter 3K" },
    { 0x46, "Custom E-Liter 3K" },
    { 0x47, "E-Liter 3K Scope" },
    { 0x48, "Custom E-Liter 3K Scope" },
    { 0x49, "Bamboozler MK I" },
    { 0x4A, "Bamboozler MK II" },
    { 0x4B, "Bamboozler MK III" },
    { 0x4C, "Slosher" },
    { 0x4D, "Slosher Deco" },
    { 0x4E, "Soda Slosher" },
    { 0x4F, "Tri-Slosher" },
    { 0x50, "Tri-Slosher Nouveau" },
    { 0x51, "Sloshing Machine" },
    { 0x52, "Sloshing Machine Neo" },
    { 0x53, "Mini Splatling" },
    { 0x54, "Zink Mini Splatling" },
    { 0x55, "Refurbished Mini Splatling" },
    { 0x56, "Heavy Splatling" },
    { 0x57, "Heavy Splatling Deco" },
    { 0x58, "Heavy Splatling Remix" },
    { 0x59, "Hydra Splatling" },
    { 0x5A, "Custom Hydra Splatling" },
    // Hero / unused / fun
    { 0x5B, "Hero Shot Lv0" },
    { 0x5C, "Hero Shot Lv1" },
    { 0x5D, "Hero Shot Lv2" },
    { 0x5E, "Hero Shot Lv3" },
    { 0x5F, "Hero Roller" },
    { 0x60, "Hero Charger" },
    { 0x61, "BigBall???" },
    { 0x62, "BigBall???" },
    { 0x63, "Octoshot Lv0" },
    { 0x64, "Octoshot Lv1" },
    { 0x65, "Octoshot Lv2" },
    { 0x66, "Octoshot Lv3" },
    { 0x67, "Roller_Kingsquid" },
    { MAIN_WEAPON_NONE, "No Weapon (bare hands)" },
};

// --- SUB WEAPONS (0x00 .. 0x09) ---
constexpr uint32_t SUB_WEAPON_NONE = 0xFFFFFFFFu;

const WeaponEntry SUB_WEAPONS[] = {
    { 0x00, "Splat Bomb" },
    { 0x01, "Suction Bomb" },
    { 0x02, "Burst Bomb" },
    { 0x03, "Seeker" },
    { 0x04, "Point Sensor" },
    { 0x05, "Ink Mine" },
    { 0x06, "Sprinkler" },
    { 0x07, "Squid Beakon" },
    { 0x08, "Splash Wall" },
    { 0x09, "Disruptor" },
    { SUB_WEAPON_NONE, "None (no sub)" },
};

// --- SPECIAL WEAPONS (0x00 .. 0x06) ---
// These IDs are not documented in Red's XML, but they're the standard
// Splatoon 1 special weapon enumeration. Verified by community testing.
// Offset 0x80 in player struct (4 bytes after sub weapon).
constexpr uint32_t SPECIAL_WEAPON_NONE = 0xFFFFFFFFu;

const WeaponEntry SPECIAL_WEAPONS[] = {
    { 0x00, "Inkstrike" },
    { 0x01, "Killer Wail" },
    { 0x02, "Bubbler" },
    { 0x03, "Kraken" },
    { 0x04, "Echolocator" },
    { 0x05, "Bomb Rush" },
    { 0x06, "Inkzooka" },
    { SPECIAL_WEAPON_NONE, "None (no special)" },
};

constexpr size_t MAIN_WEAPONS_COUNT = sizeof(MAIN_WEAPONS) / sizeof(MAIN_WEAPONS[0]);
constexpr size_t SUB_WEAPONS_COUNT = sizeof(SUB_WEAPONS) / sizeof(SUB_WEAPONS[0]);
constexpr size_t SPECIAL_WEAPONS_COUNT = sizeof(SPECIAL_WEAPONS) / sizeof(SPECIAL_WEAPONS[0]);

} // namespace splat
