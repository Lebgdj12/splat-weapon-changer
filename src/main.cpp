// main.cpp - Splat Weapon Changer Aroma plugin
//
// Modern WUPS API rewrite (matches TCPGecko-Plugin / ftpiiu_plugin patterns).
// Lets the user change main/sub/special weapons in Splatoon 1 (Wii U, v2.12.0)
// via the Aroma plugin config menu. No PC, no JGecko U needed at runtime.
//
// Memory layout (EUR v2.12.0):
//   [*0x106E46E8] + 0x290 -> player struct
//   player_struct + 0x78 = main weapon ID    (uint32)
//   player_struct + 0x7C = sub weapon ID     (uint32)
//   player_struct + 0x80 = special weapon ID (uint32)
//
// The worker thread re-pokes the chosen IDs at ~60Hz so they stick across
// respawns, map reloads and lobby transitions.

#include <cstdint>
#include <cstring>

#include <coreinit/cache.h>
#include <coreinit/memory.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/title.h>

#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/config_api.h>
#include <wups/storage.h>

#include "weapons.h"

// ---------------------------------------------------------------------------
// Plugin metadata
// ---------------------------------------------------------------------------
WUPS_PLUGIN_NAME("Splat Weapon Changer");
WUPS_PLUGIN_DESCRIPTION(
    "Change main/sub/special weapons in Splatoon 1 (v2.12.0) from the Aroma "
    "config menu. No PC required at runtime.");
WUPS_PLUGIN_VERSION("1.0.0");
WUPS_PLUGIN_AUTHOR("GLM (Z.ai) — for the Splatoon 1 modding scene");
WUPS_PLUGIN_LICENSE("MIT");

WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("splat_weapon_changer");

// ---------------------------------------------------------------------------
// Splatoon 1 title IDs
// ---------------------------------------------------------------------------
// EUR (AGME01) : 0x0005000010162200  (confirmed)
// USA (ABXE01) : 0x0005000010112300  (community-documented)
// JPN (ABXJ01) : 0x0005000010110F00  (community-documented)
constexpr uint64_t TITLE_SPLATOON1_EUR = 0x0005000010162200ULL;
constexpr uint64_t TITLE_SPLATOON1_USA = 0x0005000010112300ULL;
constexpr uint64_t TITLE_SPLATOON1_JPN = 0x0005000010110F00ULL;

// Memory layout constants (from Red's "Live Weapon ID Changer" code)
constexpr uintptr_t PLAYER_BASE_PTR_ADDR = 0x106E46E8u;
constexpr uint32_t  PLAYER_BASE_OFF      = 0x290u;
constexpr uint32_t  OFFSET_MAIN_WEAPON   = 0x78u;
constexpr uint32_t  OFFSET_SUB_WEAPON    = 0x7Cu;
constexpr uint32_t  OFFSET_SPECIAL       = 0x80u;

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
}  // namespace CfgKey

bool     g_enabled         = true;
bool     g_overrideMain    = true;
bool     g_overrideSub     = false;
bool     g_overrideSpecial = false;
uint32_t g_mainWeaponId    = 0x0Bu;  // Tentatek Splattershot
uint32_t g_subWeaponId     = 0x00u;  // Splat Bomb
uint32_t g_specialWeaponId = 0x02u;  // Bubbler

OSThread* g_workerThread = nullptr;
uint8_t*  g_workerStack  = nullptr;
bool      g_workerRunning = false;
}  // namespace

// ---------------------------------------------------------------------------
// Storage helpers
// ---------------------------------------------------------------------------
static void storeBool(const char* key, bool v) {
    WUPSStorageAPI_StoreBool(nullptr, key, v);
}
static void storeInt(const char* key, int v) {
    WUPSStorageAPI_StoreInt(nullptr, key, v);
}
static void loadBool(const char* key, bool& v) {
    WUPSStorageError e = WUPSStorageAPI_GetBool(nullptr, key, &v);
    if (e == WUPS_STORAGE_ERROR_NOT_FOUND) storeBool(key, v);
}
static void loadInt(const char* key, int& v) {
    WUPSStorageError e = WUPSStorageAPI_GetInt(nullptr, key, &v);
    if (e == WUPS_STORAGE_ERROR_NOT_FOUND) storeInt(key, v);
}

// ---------------------------------------------------------------------------
// Memory access
// ---------------------------------------------------------------------------
static inline uint32_t readU32(uintptr_t addr) {
    return *reinterpret_cast<volatile uint32_t*>(addr);
}
static inline void writeU32(uintptr_t addr, uint32_t value) {
    *reinterpret_cast<volatile uint32_t*>(addr) = value;
    OSEffectiveFlushToDirtyMemory(addr, 4);
}

static bool isSplatoon1Running() {
    const uint64_t tid = OSGetTitleID();
    return tid == TITLE_SPLATOON1_EUR ||
           tid == TITLE_SPLATOON1_USA ||
           tid == TITLE_SPLATOON1_JPN;
}

// Sanity-checked pointer chain resolution.
static uintptr_t resolvePlayerStruct() {
    if (PLAYER_BASE_PTR_ADDR < 0x10000000u ||
        PLAYER_BASE_PTR_ADDR >= 0x50000000u) return 0;

    uint32_t p1 = readU32(PLAYER_BASE_PTR_ADDR);
    if (p1 == 0 || p1 < 0x10000000u || p1 >= 0x50000000u) return 0;

    uintptr_t p2Addr = p1 + PLAYER_BASE_OFF;
    if (p2Addr >= 0x50000000u) return 0;

    uint32_t p2 = readU32(p2Addr);
    if (p2 == 0 || p2 < 0x10000000u || p2 >= 0x50000000u) return 0;

    return p2;
}

static void pokeWeaponsOnce() {
    if (!g_enabled || !isSplatoon1Running()) return;

    uintptr_t player = resolvePlayerStruct();
    if (player == 0) return;

    if (g_overrideMain)    writeU32(player + OFFSET_MAIN_WEAPON, g_mainWeaponId);
    if (g_overrideSub)     writeU32(player + OFFSET_SUB_WEAPON,  g_subWeaponId);
    if (g_overrideSpecial) writeU32(player + OFFSET_SPECIAL,     g_specialWeaponId);
}

// ---------------------------------------------------------------------------
// Worker thread (~60Hz)
// ---------------------------------------------------------------------------
static int workerMain(int, const char**) {
    while (g_workerRunning) {
        pokeWeaponsOnce();
        OSSleepTicks(OSMicrosecondsToTicks(16667));
    }
    return 0;
}

static bool startWorker() {
    if (g_workerRunning) return true;

    g_workerStack = static_cast<uint8_t*>(
        MEMAllocFromDefaultHeapEx(0x8000, 8));
    if (!g_workerStack) return false;

    g_workerThread = static_cast<OSThread*>(
        MEMAllocFromDefaultHeapEx(sizeof(OSThread), 8));
    if (!g_workerThread) {
        MEMFreeToDefaultHeap(g_workerStack);
        g_workerStack = nullptr;
        return false;
    }

    if (!OSCreateThread(
            g_workerThread, workerMain, 0, nullptr,
            g_workerStack, 0x8000, 0x40,
            OS_THREAD_ATTRIB_AFFINITY_CPU2)) {
        MEMFreeToDefaultHeap(g_workerThread);
        MEMFreeToDefaultHeap(g_workerStack);
        g_workerThread = nullptr;
        g_workerStack  = nullptr;
        return false;
    }

    g_workerRunning = true;
    OSResumeThread(g_workerThread);
    return true;
}

static void stopWorker() {
    if (!g_workerRunning) return;
    g_workerRunning = false;
    if (g_workerThread) {
        OSJoinThread(g_workerThread, nullptr);
        MEMFreeToDefaultHeap(g_workerThread);
        g_workerThread = nullptr;
    }
    if (g_workerStack) {
        MEMFreeToDefaultHeap(g_workerStack);
        g_workerStack = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Config menu callbacks
// ---------------------------------------------------------------------------
// Build a C-style pair array on first call. MultipleValues API wants a
// plain ConfigItemMultipleValuesPair*, not std::vector.
static ConfigItemMultipleValuesPair* makePairs(
    const splat::WeaponEntry* entries, size_t count) {
    auto* p = static_cast<ConfigItemMultipleValuesPair*>(
        MEMAllocFromDefaultHeap(sizeof(ConfigItemMultipleValuesPair) * count));
    if (!p) return nullptr;
    for (size_t i = 0; i < count; ++i) {
        p[i].value = entries[i].id;
        p[i].name  = entries[i].name;
    }
    return p;
}

static WUPSConfigAPICallbackStatus configMenuOpened(WUPSConfigCategoryHandle root) {
    // ---- General ----
    WUPSConfigCategoryHandle catGen;
    WUPSConfigAPICreateCategoryOptionsV1 genOpts = {.name = "General"};
    if (WUPSConfigAPI_Category_Create(genOpts, &catGen) != WUPSCONFIG_API_RESULT_SUCCESS)
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    if (WUPSConfigAPI_Category_AddCategory(root, catGen) != WUPSCONFIG_API_RESULT_SUCCESS)
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;

    WUPSConfigItemBoolean_AddToCategory(
        catGen, CfgKey::Enabled, "Plugin enabled", true, g_enabled,
        [](ConfigItemBoolean*, bool v) {
            g_enabled = v;
            storeBool(CfgKey::Enabled, v);
        });

    // ---- Main weapon ----
    WUPSConfigCategoryHandle catMain;
    WUPSConfigAPICreateCategoryOptionsV1 mainOpts = {.name = "Main weapon"};
    if (WUPSConfigAPI_Category_Create(mainOpts, &catMain) != WUPSCONFIG_API_RESULT_SUCCESS)
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    if (WUPSConfigAPI_Category_AddCategory(root, catMain) != WUPSCONFIG_API_RESULT_SUCCESS)
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;

    WUPSConfigItemBoolean_AddToCategory(
        catMain, CfgKey::OverrideMain, "Override main weapon", true, g_overrideMain,
        [](ConfigItemBoolean*, bool v) {
            g_overrideMain = v;
            storeBool(CfgKey::OverrideMain, v);
        });

    {
        static ConfigItemMultipleValuesPair* pairs = nullptr;
        if (!pairs) pairs = makePairs(splat::MAIN_WEAPONS, splat::MAIN_WEAPONS_COUNT);
        if (pairs) {
            WUPSConfigItemMultipleValues_AddToCategory(
                catMain, CfgKey::MainWeaponId, "Weapon", 0u,
                g_mainWeaponId, pairs, splat::MAIN_WEAPONS_COUNT,
                [](ConfigItemMultipleValues*, unsigned int v) {
                    g_mainWeaponId = v;
                    storeInt(CfgKey::MainWeaponId, static_cast<int>(v));
                });
        }
    }

    // ---- Sub weapon ----
    WUPSConfigCategoryHandle catSub;
    WUPSConfigAPICreateCategoryOptionsV1 subOpts = {.name = "Sub weapon"};
    if (WUPSConfigAPI_Category_Create(subOpts, &catSub) != WUPSCONFIG_API_RESULT_SUCCESS)
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    if (WUPSConfigAPI_Category_AddCategory(root, catSub) != WUPSCONFIG_API_RESULT_SUCCESS)
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;

    WUPSConfigItemBoolean_AddToCategory(
        catSub, CfgKey::OverrideSub, "Override sub weapon", false, g_overrideSub,
        [](ConfigItemBoolean*, bool v) {
            g_overrideSub = v;
            storeBool(CfgKey::OverrideSub, v);
        });

    {
        static ConfigItemMultipleValuesPair* pairs = nullptr;
        if (!pairs) pairs = makePairs(splat::SUB_WEAPONS, splat::SUB_WEAPONS_COUNT);
        if (pairs) {
            WUPSConfigItemMultipleValues_AddToCategory(
                catSub, CfgKey::SubWeaponId, "Weapon", 0u,
                g_subWeaponId, pairs, splat::SUB_WEAPONS_COUNT,
                [](ConfigItemMultipleValues*, unsigned int v) {
                    g_subWeaponId = v;
                    storeInt(CfgKey::SubWeaponId, static_cast<int>(v));
                });
        }
    }

    // ---- Special weapon ----
    WUPSConfigCategoryHandle catSpec;
    WUPSConfigAPICreateCategoryOptionsV1 specOpts = {.name = "Special weapon"};
    if (WUPSConfigAPI_Category_Create(specOpts, &catSpec) != WUPSCONFIG_API_RESULT_SUCCESS)
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    if (WUPSConfigAPI_Category_AddCategory(root, catSpec) != WUPSCONFIG_API_RESULT_SUCCESS)
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;

    WUPSConfigItemBoolean_AddToCategory(
        catSpec, CfgKey::OverrideSpecial, "Override special weapon", false,
        g_overrideSpecial,
        [](ConfigItemBoolean*, bool v) {
            g_overrideSpecial = v;
            storeBool(CfgKey::OverrideSpecial, v);
        });

    {
        static ConfigItemMultipleValuesPair* pairs = nullptr;
        if (!pairs) pairs = makePairs(splat::SPECIAL_WEAPONS, splat::SPECIAL_WEAPONS_COUNT);
        if (pairs) {
            WUPSConfigItemMultipleValues_AddToCategory(
                catSpec, CfgKey::SpecialWeaponId, "Weapon", 0u,
                g_specialWeaponId, pairs, splat::SPECIAL_WEAPONS_COUNT,
                [](ConfigItemMultipleValues*, unsigned int v) {
                    g_specialWeaponId = v;
                    storeInt(CfgKey::SpecialWeaponId, static_cast<int>(v));
                });
        }
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
    WUPSConfigAPIOptionsV1 opts = {.name = "Splat Weapon Changer"};
    if (WUPSConfigAPI_Init(opts, configMenuOpened, configMenuClosed) !=
        WUPSCONFIG_API_RESULT_SUCCESS) {
        // Soft-fail: the plugin still loads, just no config menu.
    }

    // Load persisted settings.
    loadBool(CfgKey::Enabled,         g_enabled);
    loadBool(CfgKey::OverrideMain,    g_overrideMain);
    loadBool(CfgKey::OverrideSub,     g_overrideSub);
    loadBool(CfgKey::OverrideSpecial, g_overrideSpecial);

    int tmp;
    loadInt(CfgKey::MainWeaponId,    tmp); g_mainWeaponId    = static_cast<uint32_t>(tmp);
    loadInt(CfgKey::SubWeaponId,     tmp); g_subWeaponId     = static_cast<uint32_t>(tmp);
    loadInt(CfgKey::SpecialWeaponId, tmp); g_specialWeaponId = static_cast<uint32_t>(tmp);

    WUPSStorageAPI_SaveStorage(false);
}

DEINITIALIZE_PLUGIN() {
    stopWorker();
}

ON_APPLICATION_START() {
    startWorker();
}

ON_APPLICATION_ENDS() {
    stopWorker();
}

ON_APPLICATION_REQUESTS_EXIT() {
    stopWorker();
}
