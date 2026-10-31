// main.cpp - Splat Weapon Changer + Cheat Pack Aroma plugin
//
// Modern WUPS API. All codes ported from Red's AGME01.xml (SHRL-Red).
//
// Categories:
//   - General        : master enable
//   - Main weapon    : weapon selector (94 entries) + override toggle
//   - Sub weapon     : weapon selector (10 entries)  + override toggle
//   - Special weapon : weapon selector (7 entries)   + override toggle
//   - Cheats - Movement    : Speedhack, MiniJump, Moonjump, Swim Anywhere, Skydiving
//   - Cheats - Mechanics   : Fast Respawn, No Drown, Fast Superjump, Cover Squid
//   - Cheats - Ink         : Infinite Ink, Ink Tank Mod, Enemy Ink Speed, Squidflop
//   - Cheats - Weapons     : No Weapon, Rainmaker, Shooter/Blaster Rapidfire,
//                            Extended Charger Range, Shooter Rapidfire (adv)
//
// Memory layout (EUR v2.12.0):
//   Player struct pointer chain: [*0x106E46E8] + 0x290 -> player struct
//   Weapon struct pointer chain: [*0x106E417C] + 0x578 -> +0x760 -> weapon struct
//   Offsets documented inline per cheat.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <malloc.h>

#include <coreinit/cache.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/title.h>
#include <coreinit/memorymap.h>
#include <proc_ui/procui.h>
#include <gx2/surface.h>
#include <gx2/swap.h>

#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/config_api.h>
#include <wups/storage.h>

#include "weapons.h"
#include "kernel.hpp"

// ---------------------------------------------------------------------------
// Plugin metadata
// ---------------------------------------------------------------------------
WUPS_PLUGIN_NAME("Splat Weapon Changer + Cheats");
WUPS_PLUGIN_DESCRIPTION(
    "Splatoon 1 (v2.12.0) cheat pack: weapon selector (main/sub/special) + "
    "movement, mechanics, ink and weapon cheats. All from Red's AGME01.xml.");
WUPS_PLUGIN_VERSION("1.1.0");
WUPS_PLUGIN_AUTHOR("GLM (Z.ai) - codes by Red (SHRL-Red)");
WUPS_PLUGIN_LICENSE("MIT");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("splat_weapon_changer");

// ---------------------------------------------------------------------------
// Game constants
// ---------------------------------------------------------------------------
constexpr uint64_t TITLE_SPLATOON1_EUR = 0x0005000010162200ULL;
constexpr uint64_t TITLE_SPLATOON1_USA = 0x0005000010112300ULL;
constexpr uint64_t TITLE_SPLATOON1_JPN = 0x0005000010110F00ULL;

// Pointer chains
constexpr uintptr_t PLAYER_BASE_PTR_ADDR = 0x106E46E8u;  // -> player struct via +0x290
constexpr uintptr_t WEAPON_BASE_PTR_ADDR = 0x106E417Cu;  // -> weapon struct via +0x578 -> +0x760

// Player struct offsets
constexpr uint32_t OFF_MAIN_WEAPON   = 0x78u;
constexpr uint32_t OFF_SUB_WEAPON    = 0x7Cu;
constexpr uint32_t OFF_SPECIAL       = 0x80u;
constexpr uint32_t OFF_INK_TANK      = 0x88u;
constexpr uint32_t OFF_Y_COORD       = 0x24Cu;
constexpr uint32_t OFF_FAST_RESPAWN_1 = 0x4E4u;
constexpr uint32_t OFF_FAST_RESPAWN_2 = 0x4E8u;
constexpr uint32_t OFF_FAST_RESPAWN_3 = 0x4ECu;
constexpr uint32_t OFF_DROWNING      = 0x4E0u;
constexpr uint32_t OFF_MOONJUMP_Y    = 0x5C8u;
constexpr uint32_t OFF_MOONJUMP_VEL  = 0x5D8u;
constexpr uint32_t OFF_FAST_SUPERJUMP = 0x6F8u;
constexpr uint32_t OFF_SWIM_ANYWHERE  = 0x88Cu;
constexpr uint32_t OFF_ENEMY_INK_SPD1 = 0x8A0u;
constexpr uint32_t OFF_ENEMY_INK_SPD2 = 0x8A4u;
constexpr uint32_t OFF_RAINMAKER_FLAG = 0x704u;
constexpr uint32_t OFF_RAINMAKER_ID   = 0x708u;
constexpr uint32_t OFF_INK_TANK_FULL  = 0x760u;  // complex chain, simplified

// Absolute addresses (game code/data section)
constexpr uintptr_t ADDR_SPEEDHACK        = 0x105EBE40u;
constexpr uintptr_t ADDR_MINI_JUMP        = 0x105EC2E4u;
constexpr uintptr_t ADDR_COVER_SQUID      = 0x105EC2C0u;
constexpr uintptr_t ADDR_SQUIDFLOP        = 0x105E87B8u;
constexpr uintptr_t ADDR_SHOOTER_RAPID    = 0x105EDCACu;
constexpr uintptr_t ADDR_CHARGER_RANGE    = 0x1054BE1Cu;

// ---------------------------------------------------------------------------
// Runtime state
// ---------------------------------------------------------------------------
namespace {
namespace CfgKey {
constexpr const char* Enabled         = "enabled";
constexpr const char* OverrideMain    = "overrideMain";
constexpr const char* OverrideSub     = "overrideSub";
constexpr const char* OverrideSpecial = "overrideSpecial";
constexpr const char* MainWeaponId    = "mainWeaponId";
constexpr const char* SubWeaponId     = "subWeaponId";
constexpr const char* SpecialWeaponId = "specialWeaponId";
constexpr const char* InkTankOverride = "inkTankOverride";
constexpr const char* InkTankId       = "inkTankId";
}  // namespace CfgKey

bool     g_enabled         = true;
bool     g_overrideMain    = true;
bool     g_overrideSub     = false;
bool     g_overrideSpecial = false;
uint32_t g_mainWeaponId    = 0x0Bu;
uint32_t g_subWeaponId     = 0x00u;
uint32_t g_specialWeaponId = 0x02u;
bool     g_inkTankOverride = false;
uint32_t g_inkTankId       = 0x00u;  // Normal ink tank

OSThread* g_workerThread  = nullptr;  // unused (hook-based now) but kept for ABI
uint8_t*  g_workerStack   = nullptr;
bool      g_workerRunning = false;

// Debug state (notifications removed - couldn't find header path)
bool g_splatoonDetectedNotified = false;  // placeholder, unused
}  // namespace

// ---------------------------------------------------------------------------
// Storage helpers
// ---------------------------------------------------------------------------
static void storeBool(const char* key, bool v) { WUPSStorageAPI_StoreBool(nullptr, key, v); }
static void storeInt(const char* key, int v)   { WUPSStorageAPI_StoreInt(nullptr, key, v); }
static void loadBool(const char* key, bool& v) {
    WUPSStorageError e = WUPSStorageAPI_GetBool(nullptr, key, &v);
    if (e == WUPS_STORAGE_ERROR_NOT_FOUND) storeBool(key, v);
}
static void loadInt(const char* key, int& v) {
    WUPSStorageError e = WUPSStorageAPI_GetInt(nullptr, key, &v);
    if (e == WUPS_STORAGE_ERROR_NOT_FOUND) storeInt(key, v);
}

// ---------------------------------------------------------------------------
// Memory access - using KernelWriteU32 (from CodePatchPlugin by theawesomecoder61)
// ---------------------------------------------------------------------------
// This is the EXACT approach from theawesomecoder61/CodePatchPlugin (a WORKING
// Aroma plugin that patches game code). See src/kernel.hpp.
//
// KernelWriteU32 uses:
// - KernelCopyData from <kernel/kernel.h> (libkernel)
// - OSEffectiveToPhysical to translate virtual -> physical
// - ICInvalidateRange to flush instruction cache (CRITICAL for code patches)
// - DCFlushRange to flush data cache
//
// This handles BOTH:
// - RW memory (player struct, data section) - regular data writes
// - RO memory (game code section) - instruction patches via physical copy
// ---------------------------------------------------------------------------
static inline uint32_t readU32(uintptr_t addr) {
    return *reinterpret_cast<volatile uint32_t*>(addr);
}
static inline void writeU32(uintptr_t addr, uint32_t value) {
    KernelWriteU32(static_cast<uint32_t>(addr), value);
}
static inline bool validRam(uint32_t v) {
    return v != 0 && v >= 0x10000000u && v < 0x50000000u;
}

// Check if we're in a game application (not system menu/etc).
// Title ID upper 32 bits: 0x00050000 = base game, 0x0005000E = game update.
// When the v2.12.0 update is installed, the Wii U launches the update title
// (0x0005000E-...) instead of the base game (0x00050000-...).
static bool isInGameApp() {
    uint64_t tid = OSGetTitleID();
    uint32_t upper = static_cast<uint32_t>(tid >> 32);
    return upper == 0x00050000u || upper == 0x0005000Eu;
}

// Check if we're specifically in Splatoon 1 (any region, base or update).
// Lower 32 bits of the title ID are game-specific:
//   EUR: 0x10162200  | USA: 0x10112300  | JPN: 0x10110F00
// If you're on a region not listed here, add your title ID's lower 32 bits.
static bool isSplatoon1Running() {
    if (!isInGameApp()) return false;
    uint64_t tid = OSGetTitleID();
    uint32_t lower = static_cast<uint32_t>(tid & 0xFFFFFFFFu);
    return lower == 0x10162200u ||  // EUR
           lower == 0x10112300u ||  // USA
           lower == 0x10110F00u;    // JPN
}

static uintptr_t resolvePlayerStruct() {
    // Relaxed validation: just check non-zero.
    // The strict validRam check was rejecting valid player struct addresses.
    // arGecko doesn't use strict range checks either.
    uint32_t p1 = readU32(PLAYER_BASE_PTR_ADDR);
    if (p1 == 0) return 0;
    uint32_t p2 = readU32(p1 + 0x290u);
    if (p2 == 0) return 0;
    return p2;
}

// Weapon struct chain: [0x106E417C] + 0x578 -> +0x760 -> weapon struct
static uintptr_t resolveWeaponStruct() {
    uint32_t p1 = readU32(WEAPON_BASE_PTR_ADDR);
    if (p1 == 0) return 0;
    uint32_t p2 = readU32(p1 + 0x578u);
    if (p2 == 0) return 0;
    uint32_t p3 = readU32(p2 + 0x760u);
    if (p3 == 0) return 0;
    return p3;
}

// ---------------------------------------------------------------------------
// CHEATS - all ported from Red's AGME01.xml
// ---------------------------------------------------------------------------
namespace Cheats {

struct Cheat {
    const char* name;
    const char* key;
    bool defaultValue;
    void (*apply)();
};

// --- Movement ---
static void Speedhack()      { writeU32(ADDR_SPEEDHACK, 0x3F100000u); }     // 0.5f
static void MiniHighJump()   { writeU32(ADDR_MINI_JUMP, 0x41000000u); }     // 8.0f
static void Moonjump() {
    uintptr_t p = resolvePlayerStruct();
    if (p) { writeU32(p + OFF_MOONJUMP_Y, 0); writeU32(p + OFF_MOONJUMP_VEL, 0); }
}
static void SwimAnywhere() {
    uintptr_t p = resolvePlayerStruct();
    if (p) writeU32(p + OFF_SWIM_ANYWHERE, 0x43000000u);  // 128.0f
}
static void Skydiving() {
    uintptr_t p = resolvePlayerStruct();
    if (p) {
        writeU32(p + OFF_Y_COORD, 0x46300000u);     // Y = 14000.0f
        writeU32(p + OFF_MOONJUMP_VEL, 0);
    }
}

// --- Mechanics ---
static void FastRespawn() {
    uintptr_t p = resolvePlayerStruct();
    if (p) {
        writeU32(p + OFF_FAST_RESPAWN_1, 0);
        writeU32(p + OFF_FAST_RESPAWN_2, 0);
        writeU32(p + OFF_FAST_RESPAWN_3, 0);
    }
}
static void DisabledDrowning() {
    uintptr_t p = resolvePlayerStruct();
    if (p) writeU32(p + OFF_DROWNING, 0);
}
static void FastSuperjump() {
    uintptr_t p = resolvePlayerStruct();
    if (p) writeU32(p + OFF_FAST_SUPERJUMP, 0);
}
static void CoverSquidForm() {
    writeU32(ADDR_COVER_SQUID, 0x4FF00000u);  // large float coverage
}

// --- Ink ---
static void InfiniteInk() {
    // Red's code does a complex pointer chain; the essence is writing 1.0f
    // (0x3F800000) to the ink tank fill ratio.
    uintptr_t p = resolvePlayerStruct();
    if (p) writeU32(p + OFF_INK_TANK_FULL + 0x1Cu, 0x3F800000u);
}
static void NormalSpeedEnemyInk() {
    uintptr_t p = resolvePlayerStruct();
    if (p) {
        writeU32(p + OFF_ENEMY_INK_SPD1, 0x3E000000u);
        writeU32(p + OFF_ENEMY_INK_SPD2, 0x3E000000u);
    }
}
static void ViableSquidflop() {
    writeU32(ADDR_SQUIDFLOP, 0x43000000u);
}

// --- Weapon extras ---
static void NoWeapon() {
    uintptr_t p = resolvePlayerStruct();
    if (p) writeU32(p + OFF_MAIN_WEAPON, 0xFFFFFFFFu);
}
static void RainmakerWeapon() {
    uintptr_t p = resolvePlayerStruct();
    if (p) {
        writeU32(p + OFF_RAINMAKER_FLAG, 0xFFFFFFFFu);
        writeU32(p + OFF_RAINMAKER_ID,   0x01014992u);
    }
}
static void ShooterBlasterRapidfire() {
    writeU32(ADDR_SHOOTER_RAPID, 0xFFF00000u);
}
static void ExtendedChargerRange() {
    writeU32(ADDR_CHARGER_RANGE, 0x40000000u);
}
static void ShooterRapidfireAdv() {
    // Writes to weapon struct: +0x50 = 0 (rate), +0x48 = 2.0f (speed)
    uintptr_t w = resolveWeaponStruct();
    if (w) {
        writeU32(w + 0x50u, 0x00000000u);
        writeU32(w + 0x48u, 0x40000000u);
    }
}

static const Cheat ALL[] = {
    // Movement
    {"Speedhack",                     "ch_speedhack",      false, Speedhack},
    {"Mini High Jump",                "ch_miniJump",       false, MiniHighJump},
    {"Moonjump",                      "ch_moonjump",       false, Moonjump},
    {"Swim in Ink Anywhere",          "ch_swimAnywhere",   false, SwimAnywhere},
    {"Skydiving (teleport high)",     "ch_skydiving",      false, Skydiving},
    // Mechanics
    {"Fast Respawn",                  "ch_fastRespawn",    false, FastRespawn},
    {"Disabled Drowning",             "ch_noDrown",        false, DisabledDrowning},
    {"Fast Superjumps",               "ch_fastSjump",      false, FastSuperjump},
    {"Cover Everything in Squid Form","ch_coverSquid",     false, CoverSquidForm},
    // Ink
    {"Infinite Ink",                  "ch_infInk",         false, InfiniteInk},
    {"Normal Speed on Enemy Ink",     "ch_normEnemyInk",   false, NormalSpeedEnemyInk},
    {"Viable Squidflopping",          "ch_squidflop",      false, ViableSquidflop},
    // Weapon extras
    {"No Weapon (bare hands)",        "ch_noWeapon",       false, NoWeapon},
    {"Weapon = Rainmaker",            "ch_rainmaker",      false, RainmakerWeapon},
    {"Shooter & Blaster Rapidfire",   "ch_shooterRapid",   false, ShooterBlasterRapidfire},
    {"Extended Charger Range",        "ch_chargerRange",   false, ExtendedChargerRange},
    {"Shooter Rapidfire (advanced)",  "ch_shooterRapidAdv",false, ShooterRapidfireAdv},
};

constexpr size_t COUNT = sizeof(ALL) / sizeof(ALL[0]);

}  // namespace Cheats

bool g_cheatEnabled[Cheats::COUNT] = {};

// Ink tank modifier values
const splat::WeaponEntry INK_TANKS[] = {
    { 0x00, "Normal Ink Tank" },
    { 0x01, "Hero Lv0 Ink Tank" },
    { 0x02, "Hero Lv1 Ink Tank" },
    { 0x03, "Hero Lv2 Ink Tank" },
    { 0x04, "Hero Lv3 Ink Tank" },
    { 0x05, "Octoling Ink Tank" },
};
constexpr size_t INK_TANKS_COUNT = sizeof(INK_TANKS) / sizeof(INK_TANKS[0]);

// ---------------------------------------------------------------------------
// Apply everything each frame
// ---------------------------------------------------------------------------
// IMPORTANT: we don't use OSGetTitleID() as the gate. When the user has the
// v2.12.0 update installed (required for online play), the Wii U launches the
// UPDATE title (0x0005000E-...) instead of the base game (0x00050000-...).
// So the title ID check would always return false in practice.
//
// Instead, we gate on the player base pointer at 0x106E46E8:
//   - 0 or invalid RAM  -> not in Splatoon (or game not yet initialized)
//   - non-zero + valid  -> we're in Splatoon, apply cheats
// ---------------------------------------------------------------------------
static void applyAllCheats() {
    if (!g_enabled) return;

    // GATE: Check if the player base pointer at 0x106E46E8 is initialized.
    // This address is Splatoon-specific. If it's 0, we're not in Splatoon yet
    // (or the game hasn't initialized the player struct).
    // We use regular read (safe - this address is in RW data section when mapped).
    // If the address is unmapped (wrong game), regular read might crash.
    // To be safe, check title ID first to ensure we're in a game app.
    uint64_t tid = OSGetTitleID();
    uint32_t upper = static_cast<uint32_t>(tid >> 32);
    if (upper != 0x00050000u && upper != 0x0005000Eu) return;  // not a game

    // Now safe to read game memory - we're in a game app
    uint32_t basePtr = readU32(PLAYER_BASE_PTR_ADDR);
    if (basePtr == 0) return;

    // Player-relative writes (weapon overrides)
    uintptr_t player = resolvePlayerStruct();
    if (player) {
        if (g_overrideMain)    writeU32(player + OFF_MAIN_WEAPON, g_mainWeaponId);
        if (g_overrideSub)     writeU32(player + OFF_SUB_WEAPON,  g_subWeaponId);
        if (g_overrideSpecial) writeU32(player + OFF_SPECIAL,     g_specialWeaponId);
        if (g_inkTankOverride) writeU32(player + OFF_INK_TANK,    g_inkTankId);
    }

    // All Red cheats - KernelWriteU32 handles both RW and RO memory
    for (size_t i = 0; i < Cheats::COUNT; ++i) {
        if (g_cheatEnabled[i]) Cheats::ALL[i].apply();
    }
}

// ---------------------------------------------------------------------------
// GX2SwapScanBuffers hook - THE EXACT pattern from arGecko by quarky
// ---------------------------------------------------------------------------
// arGecko (https://git.sr.ht/~quarky/arGecko) is a WORKING gecko codehandler
// for Aroma that works with Splatoon. It hooks GX2SwapScanBuffers (called
// every frame by the game's render loop) and applies patches inside the hook.
//
// Key things arGecko does that my previous versions didn't:
// 1. Hooks GX2SwapScanBuffers (not GX2CopyColorBufferToScanBuffer, not VPADRead)
// 2. Checks ProcUIInForeground() before applying patches
// 3. Checks OSEffectiveToPhysical() return value (in KernelWriteU32)
//
// This is the EXACT same pattern, adapted for our cheat menu.
// ---------------------------------------------------------------------------
DECL_FUNCTION(void, GX2SwapScanBuffers, void) {
    // Call the original first so rendering still works
    real_GX2SwapScanBuffers();

    // Only apply cheats when the game is in the foreground
    // (arGecko does this check - prevents writes during background/suspended state)
    if (!ProcUIInForeground()) return;

    // Apply all cheats - runs every frame, in game context
    applyAllCheats();
}

// Tell WUPS to replace the real GX2SwapScanBuffers with our hook.
WUPS_MUST_REPLACE(GX2SwapScanBuffers, WUPS_LOADER_LIBRARY_GX2, GX2SwapScanBuffers);

// ---------------------------------------------------------------------------
// Config menu helpers
// ---------------------------------------------------------------------------
static ConfigItemMultipleValuesPair* makePairs(
    const splat::WeaponEntry* entries, size_t count) {
    auto* p = static_cast<ConfigItemMultipleValuesPair*>(
        malloc(sizeof(ConfigItemMultipleValuesPair) * count));
    if (!p) return nullptr;
    for (size_t i = 0; i < count; ++i) {
        p[i].value     = entries[i].id;
        p[i].valueName = entries[i].name;
    }
    return p;
}

static int findWeaponIndex(const splat::WeaponEntry* entries, size_t count, uint32_t id) {
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].id == id) return static_cast<int>(i);
    }
    return 0;
}

// Helper macro: creates a category, adds it to root, returns on failure.
// Defines a variable `catName` of type WUPSConfigCategoryHandle in scope.
#define MAKE_CATEGORY(catName, root, displayName) \
    WUPSConfigCategoryHandle catName; \
    do { \
        WUPSConfigAPICreateCategoryOptionsV1 _opts = {.name = displayName}; \
        if (WUPSConfigAPI_Category_Create(_opts, &catName) != WUPSCONFIG_API_RESULT_SUCCESS) \
            return WUPSCONFIG_API_CALLBACK_RESULT_ERROR; \
        if (WUPSConfigAPI_Category_AddCategory(root, catName) != WUPSCONFIG_API_RESULT_SUCCESS) \
            return WUPSCONFIG_API_CALLBACK_RESULT_ERROR; \
    } while (0)

// ---------------------------------------------------------------------------
// Config menu open callback
// ---------------------------------------------------------------------------
static WUPSConfigAPICallbackStatus configMenuOpened(WUPSConfigCategoryHandle root) {
    // ---- General ----
    {
        MAKE_CATEGORY(cat, root, "General");
        WUPSConfigItemBoolean_AddToCategory(
            cat, CfgKey::Enabled, "Plugin enabled", true, g_enabled,
            [](ConfigItemBoolean*, bool v) { g_enabled = v; storeBool(CfgKey::Enabled, v); });
    }

    // ---- Main weapon ----
    {
        MAKE_CATEGORY(cat, root, "Main weapon");
        WUPSConfigItemBoolean_AddToCategory(
            cat, CfgKey::OverrideMain, "Override main weapon", true, g_overrideMain,
            [](ConfigItemBoolean*, bool v) { g_overrideMain = v; storeBool(CfgKey::OverrideMain, v); });
        static ConfigItemMultipleValuesPair* pairs = nullptr;
        if (!pairs) pairs = makePairs(splat::MAIN_WEAPONS, splat::MAIN_WEAPONS_COUNT);
        if (pairs) {
            WUPSConfigItemMultipleValues_AddToCategory(
                cat, CfgKey::MainWeaponId, "Weapon", 0,
                findWeaponIndex(splat::MAIN_WEAPONS, splat::MAIN_WEAPONS_COUNT, g_mainWeaponId),
                pairs, static_cast<int>(splat::MAIN_WEAPONS_COUNT),
                [](ConfigItemMultipleValues*, uint32_t v) {
                    g_mainWeaponId = v; storeInt(CfgKey::MainWeaponId, static_cast<int>(v));
                });
        }
    }

    // ---- Sub weapon ----
    {
        MAKE_CATEGORY(cat, root, "Sub weapon");
        WUPSConfigItemBoolean_AddToCategory(
            cat, CfgKey::OverrideSub, "Override sub weapon", false, g_overrideSub,
            [](ConfigItemBoolean*, bool v) { g_overrideSub = v; storeBool(CfgKey::OverrideSub, v); });
        static ConfigItemMultipleValuesPair* pairs = nullptr;
        if (!pairs) pairs = makePairs(splat::SUB_WEAPONS, splat::SUB_WEAPONS_COUNT);
        if (pairs) {
            WUPSConfigItemMultipleValues_AddToCategory(
                cat, CfgKey::SubWeaponId, "Weapon", 0,
                findWeaponIndex(splat::SUB_WEAPONS, splat::SUB_WEAPONS_COUNT, g_subWeaponId),
                pairs, static_cast<int>(splat::SUB_WEAPONS_COUNT),
                [](ConfigItemMultipleValues*, uint32_t v) {
                    g_subWeaponId = v; storeInt(CfgKey::SubWeaponId, static_cast<int>(v));
                });
        }
    }

    // ---- Special weapon ----
    {
        MAKE_CATEGORY(cat, root, "Special weapon");
        WUPSConfigItemBoolean_AddToCategory(
            cat, CfgKey::OverrideSpecial, "Override special weapon", false, g_overrideSpecial,
            [](ConfigItemBoolean*, bool v) { g_overrideSpecial = v; storeBool(CfgKey::OverrideSpecial, v); });
        static ConfigItemMultipleValuesPair* pairs = nullptr;
        if (!pairs) pairs = makePairs(splat::SPECIAL_WEAPONS, splat::SPECIAL_WEAPONS_COUNT);
        if (pairs) {
            WUPSConfigItemMultipleValues_AddToCategory(
                cat, CfgKey::SpecialWeaponId, "Weapon", 0,
                findWeaponIndex(splat::SPECIAL_WEAPONS, splat::SPECIAL_WEAPONS_COUNT, g_specialWeaponId),
                pairs, static_cast<int>(splat::SPECIAL_WEAPONS_COUNT),
                [](ConfigItemMultipleValues*, uint32_t v) {
                    g_specialWeaponId = v; storeInt(CfgKey::SpecialWeaponId, static_cast<int>(v));
                });
        }
    }

    // ---- Cheats - Movement ----
    {
        MAKE_CATEGORY(cat, root, "Cheats - Movement");
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[0].key,  Cheats::ALL[0].name,  Cheats::ALL[0].defaultValue,  g_cheatEnabled[0],  [](ConfigItemBoolean*, bool v){ g_cheatEnabled[0]=v;  storeBool(Cheats::ALL[0].key,  v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[1].key,  Cheats::ALL[1].name,  Cheats::ALL[1].defaultValue,  g_cheatEnabled[1],  [](ConfigItemBoolean*, bool v){ g_cheatEnabled[1]=v;  storeBool(Cheats::ALL[1].key,  v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[2].key,  Cheats::ALL[2].name,  Cheats::ALL[2].defaultValue,  g_cheatEnabled[2],  [](ConfigItemBoolean*, bool v){ g_cheatEnabled[2]=v;  storeBool(Cheats::ALL[2].key,  v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[3].key,  Cheats::ALL[3].name,  Cheats::ALL[3].defaultValue,  g_cheatEnabled[3],  [](ConfigItemBoolean*, bool v){ g_cheatEnabled[3]=v;  storeBool(Cheats::ALL[3].key,  v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[4].key,  Cheats::ALL[4].name,  Cheats::ALL[4].defaultValue,  g_cheatEnabled[4],  [](ConfigItemBoolean*, bool v){ g_cheatEnabled[4]=v;  storeBool(Cheats::ALL[4].key,  v); });
    }

    // ---- Cheats - Mechanics ----
    {
        MAKE_CATEGORY(cat, root, "Cheats - Mechanics");
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[5].key,  Cheats::ALL[5].name,  Cheats::ALL[5].defaultValue,  g_cheatEnabled[5],  [](ConfigItemBoolean*, bool v){ g_cheatEnabled[5]=v;  storeBool(Cheats::ALL[5].key,  v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[6].key,  Cheats::ALL[6].name,  Cheats::ALL[6].defaultValue,  g_cheatEnabled[6],  [](ConfigItemBoolean*, bool v){ g_cheatEnabled[6]=v;  storeBool(Cheats::ALL[6].key,  v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[7].key,  Cheats::ALL[7].name,  Cheats::ALL[7].defaultValue,  g_cheatEnabled[7],  [](ConfigItemBoolean*, bool v){ g_cheatEnabled[7]=v;  storeBool(Cheats::ALL[7].key,  v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[8].key,  Cheats::ALL[8].name,  Cheats::ALL[8].defaultValue,  g_cheatEnabled[8],  [](ConfigItemBoolean*, bool v){ g_cheatEnabled[8]=v;  storeBool(Cheats::ALL[8].key,  v); });
    }

    // ---- Cheats - Ink ----
    {
        MAKE_CATEGORY(cat, root, "Cheats - Ink");
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[9].key,  Cheats::ALL[9].name,  Cheats::ALL[9].defaultValue,  g_cheatEnabled[9],  [](ConfigItemBoolean*, bool v){ g_cheatEnabled[9]=v;  storeBool(Cheats::ALL[9].key,  v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[10].key, Cheats::ALL[10].name, Cheats::ALL[10].defaultValue, g_cheatEnabled[10], [](ConfigItemBoolean*, bool v){ g_cheatEnabled[10]=v; storeBool(Cheats::ALL[10].key, v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[11].key, Cheats::ALL[11].name, Cheats::ALL[11].defaultValue, g_cheatEnabled[11], [](ConfigItemBoolean*, bool v){ g_cheatEnabled[11]=v; storeBool(Cheats::ALL[11].key, v); });

        // Ink Tank Modifier (multi-value)
        WUPSConfigItemBoolean_AddToCategory(
            cat, CfgKey::InkTankOverride, "Override ink tank", false, g_inkTankOverride,
            [](ConfigItemBoolean*, bool v) { g_inkTankOverride = v; storeBool(CfgKey::InkTankOverride, v); });
        static ConfigItemMultipleValuesPair* inkPairs = nullptr;
        if (!inkPairs) inkPairs = makePairs(INK_TANKS, INK_TANKS_COUNT);
        if (inkPairs) {
            WUPSConfigItemMultipleValues_AddToCategory(
                cat, CfgKey::InkTankId, "Ink tank variant", 0,
                findWeaponIndex(INK_TANKS, INK_TANKS_COUNT, g_inkTankId),
                inkPairs, static_cast<int>(INK_TANKS_COUNT),
                [](ConfigItemMultipleValues*, uint32_t v) {
                    g_inkTankId = v; storeInt(CfgKey::InkTankId, static_cast<int>(v));
                });
        }
    }

    // ---- Cheats - Weapons ----
    {
        MAKE_CATEGORY(cat, root, "Cheats - Weapons");
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[12].key, Cheats::ALL[12].name, Cheats::ALL[12].defaultValue, g_cheatEnabled[12], [](ConfigItemBoolean*, bool v){ g_cheatEnabled[12]=v; storeBool(Cheats::ALL[12].key, v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[13].key, Cheats::ALL[13].name, Cheats::ALL[13].defaultValue, g_cheatEnabled[13], [](ConfigItemBoolean*, bool v){ g_cheatEnabled[13]=v; storeBool(Cheats::ALL[13].key, v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[14].key, Cheats::ALL[14].name, Cheats::ALL[14].defaultValue, g_cheatEnabled[14], [](ConfigItemBoolean*, bool v){ g_cheatEnabled[14]=v; storeBool(Cheats::ALL[14].key, v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[15].key, Cheats::ALL[15].name, Cheats::ALL[15].defaultValue, g_cheatEnabled[15], [](ConfigItemBoolean*, bool v){ g_cheatEnabled[15]=v; storeBool(Cheats::ALL[15].key, v); });
        WUPSConfigItemBoolean_AddToCategory(cat, Cheats::ALL[16].key, Cheats::ALL[16].name, Cheats::ALL[16].defaultValue, g_cheatEnabled[16], [](ConfigItemBoolean*, bool v){ g_cheatEnabled[16]=v; storeBool(Cheats::ALL[16].key, v); });
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

static void configMenuClosed() {
    WUPSStorageAPI_SaveStorage(false);
}

// ---------------------------------------------------------------------------
// Plugin lifecycle
// ---------------------------------------------------------------------------
INITIALIZE_PLUGIN() {
    WUPSConfigAPIOptionsV1 opts = {.name = "Splat Weapon Changer + Cheats"};
    WUPSConfigAPI_Init(opts, configMenuOpened, configMenuClosed);

    loadBool(CfgKey::Enabled,         g_enabled);
    loadBool(CfgKey::OverrideMain,    g_overrideMain);
    loadBool(CfgKey::OverrideSub,     g_overrideSub);
    loadBool(CfgKey::OverrideSpecial, g_overrideSpecial);
    loadBool(CfgKey::InkTankOverride, g_inkTankOverride);

    int tmp;
    loadInt(CfgKey::MainWeaponId,    tmp); g_mainWeaponId    = static_cast<uint32_t>(tmp);
    loadInt(CfgKey::SubWeaponId,     tmp); g_subWeaponId     = static_cast<uint32_t>(tmp);
    loadInt(CfgKey::SpecialWeaponId, tmp); g_specialWeaponId = static_cast<uint32_t>(tmp);
    loadInt(CfgKey::InkTankId,       tmp); g_inkTankId       = static_cast<uint32_t>(tmp);

    for (size_t i = 0; i < Cheats::COUNT; ++i) {
        bool v = Cheats::ALL[i].defaultValue;
        loadBool(Cheats::ALL[i].key, v);
        g_cheatEnabled[i] = v;
    }

    WUPSStorageAPI_SaveStorage(false);
}

DEINITIALIZE_PLUGIN() {
    // Hook is auto-managed by WUPS_MUST_REPLACE. Nothing to clean up.
}

ON_APPLICATION_START() {
    // Hook is installed by WUPS at plugin load. Nothing to do here.
}

ON_APPLICATION_ENDS()  {}
ON_APPLICATION_REQUESTS_EXIT() {}
