// main.cpp - Splat Weapon Changer Aroma plugin
//
// Lets the user change main/sub/special weapons in Splatoon 1 (Wii U, v2.12.0)
// via the Aroma plugin config menu. No PC, no JGecko U needed at runtime.
//
// Memory layout (EUR v2.12.0):
//   [0x106E46E8] -> +0x290 -> player struct
//   player_struct + 0x78 = main weapon ID    (uint32)
//   player_struct + 0x7C = sub weapon ID     (uint32)
//   player_struct + 0x80 = special weapon ID (uint32)
//
// The pointer is rebuilt every frame and the weapon IDs are re-poked at ~60Hz,
// so weapons stick even across respawns / map reloads.

#include <cstdint>
#include <cstring>
#include <vector>
#include <utility>

#include <coreinit/cache.h>
#include <coreinit/memory.h>
#include <coreinit/memorymap.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/title.h>
#include <proc_ui/procui.h>

#include <wups.h>
#include <wups/storage.h>
#include <wups/config_api.h>
#include <wups/config/WUPSConfigCategory.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>

#include "weapons.h"

// ----------------------------------------------------------------------------
// Plugin metadata
// ----------------------------------------------------------------------------
WUPS_PLUGIN_NAME("Splat Weapon Changer")
WUPS_PLUGIN_AUTHOR("GLM (Z.ai) for the Splatoon 1 modding scene")
WUPS_PLUGIN_VERSION("1.0.0")
WUPS_PLUGIN_LICENSE("MIT")
WUPS_PLUGIN_DESCRIPTION("Change main/sub/special weapons in Splatoon 1 (v2.12.0) from the Aroma config menu. No PC required at runtime.");

WUPS_USE_WUPS_STORAGE("splat_weapon_changer");

// ----------------------------------------------------------------------------
// Game title IDs (Splatoon 1 final version v2.12.0, never patched since Jan 2016)
// ----------------------------------------------------------------------------
constexpr uint64_t TITLE_SPLATOON1_EUR = 0x0005000010162200ULL;
constexpr uint64_t TITLE_SPLATOON1_USA = 0x0005000010112300ULL;
constexpr uint64_t TITLE_SPLATOON1_JPN = 0x0005000010110F00ULL;

// ----------------------------------------------------------------------------
// Memory layout (from Red's "Live Weapon ID Changer" code in AGME01.xml)
// ----------------------------------------------------------------------------
constexpr uintptr_t DEFAULT_PLAYER_BASE_PTR = 0x106E46E8u; // EUR v2.12.0
constexpr uint32_t  DEFAULT_PLAYER_BASE_OFF = 0x290u;
constexpr uint32_t  OFFSET_MAIN_WEAPON     = 0x78u;
constexpr uint32_t  OFFSET_SUB_WEAPON      = 0x7Cu;
constexpr uint32_t  OFFSET_SPECIAL_WEAPON  = 0x80u;  // presumed (4 bytes after sub)

// ----------------------------------------------------------------------------
// Runtime state (stored in plugin global; persisted via WUPS storage)
// ----------------------------------------------------------------------------
struct PluginState {
    bool      enabled         = true;
    bool      overrideMain    = true;
    bool      overrideSub     = false;
    bool      overrideSpecial = false;
    uint32_t  mainWeaponId    = 0x0B;   // Tentatek Splattershot
    uint32_t  subWeaponId     = 0x00;   // Splat Bomb
    uint32_t  specialWeaponId = 0x02;   // Bubbler
};

static PluginState g_state;
static bool        g_threadRunning = false;
static OSThread*   g_thread        = nullptr;
static uint8_t*    g_stack         = nullptr;

// ----------------------------------------------------------------------------
// Memory access helpers
// ----------------------------------------------------------------------------
static inline uint32_t readU32(uintptr_t addr) {
    return *reinterpret_cast<volatile uint32_t*>(addr);
}

static inline void writeU32(uintptr_t addr, uint32_t value) {
    *reinterpret_cast<volatile uint32_t*>(addr) = value;
    OSEffectiveFlushToDirtyMemory(addr, 4);
}

static bool isSplatoon1Running() {
    const uint64_t tid = OSGetTitleID();
    return tid == TITLE_SPLATOON1_EUR
        || tid == TITLE_SPLATOON1_USA
        || tid == TITLE_SPLATOON1_JPN;
}

// Follow pointer chain: [*0x106E46E8] -> +0x290 -> player struct.
// Returns 0 if any pointer is null or out of valid foreground RAM bounds.
static uintptr_t resolvePlayerStruct() {
    const uintptr_t basePtrAddr = DEFAULT_PLAYER_BASE_PTR;
    if (basePtrAddr < 0x10000000u || basePtrAddr >= 0x50000000u) return 0;

    uint32_t p1 = readU32(basePtrAddr);
    if (p1 < 0x10000000u || p1 >= 0x50000000u) return 0;

    uintptr_t playerPtrAddr = p1 + DEFAULT_PLAYER_BASE_OFF;
    if (playerPtrAddr >= 0x50000000u) return 0;

    uint32_t p2 = readU32(playerPtrAddr);
    if (p2 < 0x10000000u || p2 >= 0x50000000u) return 0;

    return p2;
}

static void pokeWeaponsOnce() {
    if (!g_state.enabled || !isSplatoon1Running()) return;

    uintptr_t player = resolvePlayerStruct();
    if (player == 0) return;

    if (g_state.overrideMain)     writeU32(player + OFFSET_MAIN_WEAPON,     g_state.mainWeaponId);
    if (g_state.overrideSub)      writeU32(player + OFFSET_SUB_WEAPON,      g_state.subWeaponId);
    if (g_state.overrideSpecial)  writeU32(player + OFFSET_SPECIAL_WEAPON,  g_state.specialWeaponId);
}

// ----------------------------------------------------------------------------
// Worker thread: pokes ~60 times/sec while Splatoon 1 is in foreground
// ----------------------------------------------------------------------------
static int workerThreadMain(int argc, const char** argv) {
    (void)argc; (void)argv;
    while (g_threadRunning) {
        pokeWeaponsOnce();
        OSSleepTicks(OSMicrosecondsToTicks(16667)); // ~60 Hz
    }
    return 0;
}

static bool startWorkerThread() {
    if (g_threadRunning) return true;
    g_stack = static_cast<uint8_t*>(MEMAllocFromDefaultHeapEx(0x8000, 8));
    if (!g_stack) return false;
    g_thread = static_cast<OSThread*>(MEMAllocFromDefaultHeapEx(sizeof(OSThread), 8));
    if (!g_thread) { MEMFreeToDefaultHeap(g_stack); g_stack = nullptr; return false; }

    if (!OSCreateThread(g_thread, workerThreadMain,
                        0, nullptr,
                        g_stack, 0x8000,
                        0x40, OS_THREAD_ATTRIB_AFFINITY_CPU2)) {
        MEMFreeToDefaultHeap(g_thread);
        MEMFreeToDefaultHeap(g_stack);
        g_thread = nullptr; g_stack = nullptr;
        return false;
    }
    g_threadRunning = true;
    OSResumeThread(g_thread);
    return true;
}

static void stopWorkerThread() {
    if (!g_threadRunning) return;
    g_threadRunning = false;
    if (g_thread) { OSJoinThread(g_thread, nullptr); MEMFreeToDefaultHeap(g_thread); g_thread = nullptr; }
    if (g_stack)  { MEMFreeToDefaultHeap(g_stack); g_stack = nullptr; }
}

// ----------------------------------------------------------------------------
// Storage load/save
// ----------------------------------------------------------------------------
static void loadState() {
    int32_t v;
    if (WUPS_GetInt("enabled",         &v) == WUPS_STORAGE_ERROR_SUCCESS) g_state.enabled         = v != 0;
    if (WUPS_GetInt("overrideMain",    &v) == WUPS_STORAGE_ERROR_SUCCESS) g_state.overrideMain    = v != 0;
    if (WUPS_GetInt("overrideSub",     &v) == WUPS_STORAGE_ERROR_SUCCESS) g_state.overrideSub     = v != 0;
    if (WUPS_GetInt("overrideSpecial", &v) == WUPS_STORAGE_ERROR_SUCCESS) g_state.overrideSpecial = v != 0;
    if (WUPS_GetInt("mainWeaponId",    &v) == WUPS_STORAGE_ERROR_SUCCESS) g_state.mainWeaponId    = static_cast<uint32_t>(v);
    if (WUPS_GetInt("subWeaponId",     &v) == WUPS_STORAGE_ERROR_SUCCESS) g_state.subWeaponId     = static_cast<uint32_t>(v);
    if (WUPS_GetInt("specialWeaponId", &v) == WUPS_STORAGE_ERROR_SUCCESS) g_state.specialWeaponId = static_cast<uint32_t>(v);
}

static void saveState() {
    WUPS_StoreInt("enabled",         g_state.enabled         ? 1 : 0);
    WUPS_StoreInt("overrideMain",    g_state.overrideMain    ? 1 : 0);
    WUPS_StoreInt("overrideSub",     g_state.overrideSub     ? 1 : 0);
    WUPS_StoreInt("overrideSpecial", g_state.overrideSpecial ? 1 : 0);
    WUPS_StoreInt("mainWeaponId",    static_cast<int32_t>(g_state.mainWeaponId));
    WUPS_StoreInt("subWeaponId",     static_cast<int32_t>(g_state.subWeaponId));
    WUPS_StoreInt("specialWeaponId", static_cast<int32_t>(g_state.specialWeaponId));
}

// ----------------------------------------------------------------------------
// Config menu callbacks (must be plain C functions, no captures)
// ----------------------------------------------------------------------------
static void onEnabledChanged(ConfigItemBoolean*, bool v) {
    g_state.enabled = v; saveState();
}
static void onOverrideMainChanged(ConfigItemBoolean*, bool v) {
    g_state.overrideMain = v; saveState();
}
static void onOverrideSubChanged(ConfigItemBoolean*, bool v) {
    g_state.overrideSub = v; saveState();
}
static void onOverrideSpecialChanged(ConfigItemBoolean*, bool v) {
    g_state.overrideSpecial = v; saveState();
}

static void onMainWeaponChanged(ConfigItemMultipleValues*, uint32_t v) {
    g_state.mainWeaponId = v; saveState();
}
static void onSubWeaponChanged(ConfigItemMultipleValues*, uint32_t v) {
    g_state.subWeaponId = v; saveState();
}
static void onSpecialWeaponChanged(ConfigItemMultipleValues*, uint32_t v) {
    g_state.specialWeaponId = v; saveState();
}

// Build a ConfigItemMultipleValuesPair array for a weapon list.
// Returned pointer points to a static buffer (one per call) - we have only
// 3 weapon categories, so 3 static buffers are fine.
static std::pair<ConfigItemMultipleValuesPair*, int>
buildValuePairs(const splat::WeaponEntry* entries, size_t count, size_t slotIdx) {
    static std::vector<ConfigItemMultipleValuesPair> buffers[3];
    auto& buf = buffers[slotIdx];
    buf.clear();
    buf.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        buf.push_back({ entries[i].id, entries[i].name });
    }
    return { buf.data(), static_cast<int>(buf.size()) };
}

static int indexOfValue(const splat::WeaponEntry* entries, size_t count, uint32_t value) {
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].id == value) return static_cast<int>(i);
    }
    return 0;
}

// ----------------------------------------------------------------------------
// Config menu: build when the user opens it.
// ----------------------------------------------------------------------------
// Helper to create a category with just a name (robust to extra fields in
// WUPSConfigAPICreateCategoryOptionsV1 across WUPS versions).
static WUPSConfigCategoryHandle createCategory(const char* name) {
    WUPSConfigAPICreateCategoryOptionsV1 opts = {};
    opts.name = name;
    WUPSConfigCategoryHandle handle = nullptr;
    if (WUPSConfigAPI_Category_Create(opts, &handle) != WUPS_CONFIG_API_SUCCESS) {
        return nullptr;
    }
    return handle;
}

static WUPSConfigAPICallbackStatus onConfigMenuOpened(WUPSConfigCategoryHandle root) {
    // --- General category ---
    if (WUPSConfigCategoryHandle catGen = createCategory("General")) {
        WUPSConfigItemBoolean_AddToCategory(catGen, "enabled",
            "Plugin enabled", true, g_state.enabled, onEnabledChanged);
        WUPSConfigAPI_Category_AddItem(root, catGen);
    }

    // --- Main weapon category ---
    if (WUPSConfigCategoryHandle catMain = createCategory("Main weapon")) {
        WUPSConfigItemBoolean_AddToCategory(catMain, "overrideMain",
            "Override main weapon", true, g_state.overrideMain, onOverrideMainChanged);
        auto pairs = buildValuePairs(splat::MAIN_WEAPONS, splat::MAIN_WEAPONS_COUNT, 0);
        WUPSConfigItemMultipleValues_AddToCategory(catMain, "mainWeapon",
            "Weapon", 0, indexOfValue(splat::MAIN_WEAPONS, splat::MAIN_WEAPONS_COUNT, g_state.mainWeaponId),
            pairs.first, pairs.second, onMainWeaponChanged);
        WUPSConfigAPI_Category_AddItem(root, catMain);
    }

    // --- Sub weapon category ---
    if (WUPSConfigCategoryHandle catSub = createCategory("Sub weapon")) {
        WUPSConfigItemBoolean_AddToCategory(catSub, "overrideSub",
            "Override sub weapon", false, g_state.overrideSub, onOverrideSubChanged);
        auto pairs = buildValuePairs(splat::SUB_WEAPONS, splat::SUB_WEAPONS_COUNT, 1);
        WUPSConfigItemMultipleValues_AddToCategory(catSub, "subWeapon",
            "Weapon", 0, indexOfValue(splat::SUB_WEAPONS, splat::SUB_WEAPONS_COUNT, g_state.subWeaponId),
            pairs.first, pairs.second, onSubWeaponChanged);
        WUPSConfigAPI_Category_AddItem(root, catSub);
    }

    // --- Special weapon category ---
    if (WUPSConfigCategoryHandle catSpec = createCategory("Special weapon")) {
        WUPSConfigItemBoolean_AddToCategory(catSpec, "overrideSpecial",
            "Override special weapon", false, g_state.overrideSpecial, onOverrideSpecialChanged);
        auto pairs = buildValuePairs(splat::SPECIAL_WEAPONS, splat::SPECIAL_WEAPONS_COUNT, 2);
        WUPSConfigItemMultipleValues_AddToCategory(catSpec, "specialWeapon",
            "Weapon", 0, indexOfValue(splat::SPECIAL_WEAPONS, splat::SPECIAL_WEAPONS_COUNT, g_state.specialWeaponId),
            pairs.first, pairs.second, onSpecialWeaponChanged);
        WUPSConfigAPI_Category_AddItem(root, catSpec);
    }

    return WUPS_CONFIG_API_CALLBACK_RESULT_SUCCESS;
}

static void onConfigMenuClosed() {
    // nothing to do (state is persisted per-change via the callbacks above)
}

// ----------------------------------------------------------------------------
// Plugin lifecycle hooks
// ----------------------------------------------------------------------------
INITIALIZE_PLUGIN() {
    WUPSConfigAPIOptionsV1 opts = { .name = "Splat Weapon Changer" };
    WUPSConfigAPIStatus s = WUPSConfigAPI_Init(opts, onConfigMenuOpened, onConfigMenuClosed);
    if (s != WUPS_CONFIG_API_SUCCESS) {
        // Non-fatal: plugin still works, just no config menu.
    }
}

DEINITIALIZE_PLUGIN() {
    stopWorkerThread();
}

ON_APPLICATION_START() {
    loadState();
    startWorkerThread();
}

ON_APPLICATION_REQUESTS_EXIT() {
    stopWorkerThread();
}
