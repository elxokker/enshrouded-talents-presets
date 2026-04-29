#include "pch.h"

#include <shroudtopia.h>
#include <memory_utils.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cwchar>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <process.h>
#include <sstream>
#include <string>
#include <vector>
#include <windowsx.h>

namespace
{
    constexpr uintptr_t RVA_SKILL_NODE_STATUS_TICK = 0xB5BD04;
    constexpr std::size_t SKILL_NODE_STATUS_STOLEN_SIZE = 6;
    constexpr uintptr_t RVA_DRAW_NODE_LINK = 0xB9F260;
    constexpr std::size_t DRAW_NODE_LINK_STOLEN_SIZE = 20;
    constexpr uintptr_t RVA_SKILL_ACTION_CALLSITE = 0xB5F521;
    constexpr std::size_t SKILL_ACTION_CALLSITE_STOLEN_SIZE = 5;
    constexpr uintptr_t RVA_SKILL_ACTION_CONTEXT_CHECK = 0xC9EE90;
    constexpr std::size_t SKILL_ACTION_CONTEXT_CHECK_STOLEN_SIZE = 5;
    constexpr uintptr_t RVA_SKILL_ACTION_SLOT_RESERVE = 0xBFA880;
    constexpr std::size_t SKILL_ACTION_SLOT_RESERVE_PATCH_SIZE = 7;

    constexpr uintptr_t RVA_SKILL_TREE_CONTEXT_ENTRY = 0xBE3D00;
    constexpr std::size_t SKILL_TREE_CONTEXT_ENTRY_STOLEN_SIZE = 38;
    constexpr uintptr_t RVA_UI_MESSAGE_ALLOCATOR = 0xC1D230;
    constexpr uintptr_t RVA_GAME_CONTEXT_GLOBAL = 0x2721588;
    constexpr std::size_t GAME_CONTEXT_SLOT_OFFSET = 0xC8;
    constexpr std::size_t TALENT_QUEUE_OFFSET = 0x51530;
    constexpr std::size_t TALENT_POINTS_STATE_OFFSET = 0x28;
    constexpr std::size_t TALENT_MESSAGE_NODE_ID_OFFSET = 0x5B0;
    constexpr std::size_t TALENT_MESSAGE_RESET_SKILLS_OFFSET = TALENT_MESSAGE_NODE_ID_OFFSET + 0x04;
    constexpr std::uint8_t TALENT_MESSAGE_LEARN = 0x42;
    constexpr std::uint8_t TALENT_MESSAGE_UNLEARN = 0x43;
    constexpr DWORD NODE_CAPTURE_STALE_MS = 10000;
    constexpr DWORD NODE_DIRECT_ACTION_FRESH_MS = 1500;
    constexpr int MAX_NODE_SAMPLES = 256;
    constexpr std::size_t SKILL_TREE_NODE_ARRAY_OFFSET = 0x98;
    constexpr std::size_t SKILL_TREE_NODE_STRIDE = 0x50;
    constexpr std::size_t SKILL_TREE_NODE_COUNT_OFFSET = 0x5098;
    constexpr std::uint16_t MAX_SKILL_TREE_NODES = 512;
    constexpr std::uint32_t TALENT_ID_TALAR_RIGHT_3PT = 3126571237u;
    constexpr std::uint32_t TALENT_ID_GRAPPLE = 2778233134u;
    constexpr std::uint32_t TALENT_ID_TALAR_LEFT_3PT = 2116701756u;
    constexpr int PRESET_COUNT = 5;
    constexpr UINT WM_TALENT_PRESETS_SHOW = WM_APP + 0x173;
    constexpr UINT WM_TALENT_PRESETS_REFRESH = WM_APP + 0x174;
    constexpr UINT WM_TALENT_PRESETS_SHUTDOWN = WM_APP + 0x175;
    constexpr UINT_PTR PRESET_UI_TIMER = 0x173;
    constexpr int PRESET_BUTTON_APPLY_BASE = 4100;
    constexpr int PRESET_BUTTON_SAVE_BASE = 4200;
    constexpr int PRESET_BUTTON_ADD_HOVER_BASE = 4250;
    constexpr int PRESET_BUTTON_REMOVE_HOVER_BASE = 4260;
    constexpr int PRESET_EDIT_NAME_BASE = 4300;
    constexpr int PRESET_BUTTON_CLEAR_ALL = 4400;
    constexpr int PRESET_WINDOW_WIDTH = 840;
    constexpr int PRESET_WINDOW_HEIGHT = 430;
    constexpr int PRESET_HEADER_HEIGHT = 82;
    constexpr int PRESET_CORNER_RADIUS = 22;
    constexpr int PRESET_MAX_JOB_PASSES = 10;
    constexpr DWORD PRESET_ACTION_TIMER_MS = 50;
    constexpr DWORD PRESET_CLEAR_ACTION_INTERVAL_MS = 160;
    constexpr DWORD PRESET_LEARN_ACTION_INTERVAL_MS = 260;
    constexpr DWORD PRESET_PHASE_SETTLE_MS = 900;
    constexpr DWORD PRESET_BULK_CLEAR_SETTLE_MS = 1200;
    constexpr DWORD PRESET_CLEAR_VERIFY_MIN_MS = 40;
    constexpr DWORD PRESET_LEARN_VERIFY_MIN_MS = 80;

    ModMetaData g_metaData = {
        "talents_presets",
        "Live in-game talent preset panel.",
        "0.5.27",
        "xoker and contributors",
        "0.0.3",
        true,
        false
    };

    ModContext* g_modContext = nullptr;
    HMODULE g_moduleHandle = nullptr;
    uintptr_t g_exeBase = 0;
    Mem::Detour* g_skillNodeStatusHook = nullptr;
    Mem::Detour* g_drawNodeLinkHook = nullptr;
    Mem::Detour* g_skillTreeContextHook = nullptr;
    Mem::Detour* g_skillActionCallsiteHook = nullptr;

    volatile LONG g_skillTreeRootSeen = 0;
    volatile LONG g_nodeHitCount = 0;
    volatile LONG g_lastNodeIdBits = 0;
    volatile LONG g_lastNodeState4 = -1;
    volatile LONG g_lastNodeState40 = -1;
    volatile LONG g_lastNodeState41 = -1;
    volatile LONG g_lastNodeState42 = -1;
    volatile LONG g_lastNodeState43 = -1;
    volatile LONG g_lastNodeCost = -1;
    volatile LONG g_lastNodeLocal28 = -1;
    volatile LONG64 g_lastNodePtr = 0;
    volatile LONG64 g_lastNodeStatePtr = 0;
    volatile LONG g_lastNodeTick = 0;
    volatile LONG64 g_lastGameRoot = 0;
    volatile LONG64 g_lastContextSlot = 0;
    volatile LONG64 g_lastActivePlayer = 0;
    volatile LONG64 g_lastTalentState = 0;
    volatile LONG64 g_lastTalentQueue = 0;
    volatile LONG g_lastContextTick = 0;
    volatile LONG64 g_lastTreeUiRoot = 0;
    volatile LONG g_lastTreeNodeCount = 0;
    volatile LONG g_lastTreeTick = 0;
    volatile LONG g_pendingActionArmed = 0;
    volatile LONG g_pendingActionMode = 0;
    volatile LONG g_pendingActionUnlearn = 0;
    volatile LONG g_pendingActionNodeIdBits = 0;
    volatile LONG64 g_pendingActionNodePtr = 0;
    volatile LONG g_presetUiVisible = 0;

    DWORD g_lastLogTick = 0;
    std::uint32_t g_lastLoggedNodeId = 0;
    LONG g_lastLoggedHitCount = 0;

    struct NodeSample
    {
        uintptr_t node = 0;
        uintptr_t state = 0;
        std::uint32_t nodeId = 0;
        std::uint8_t local28 = 0;
        std::uint8_t state4 = 0;
        std::uint16_t cost = 0;
        std::uint8_t state40 = 0;
        std::uint8_t state41 = 0;
        std::uint8_t state42 = 0;
        std::uint8_t state43 = 0;
        LONG hits = 0;
    };

    struct LiveNodeMatch
    {
        uintptr_t node = 0;
        uintptr_t state = 0;
        std::uint32_t nodeId = 0;
        std::uint8_t local28 = 0xFF;
        std::uint8_t state4 = 0xFF;
        std::uint16_t cost = 0xFFFF;
        std::uint8_t state40 = 0xFF;
        std::uint8_t state41 = 0xFF;
        std::uint8_t state42 = 0xFF;
        std::uint8_t state43 = 0xFF;
        float uiX = 0.0f;
        float uiY = 0.0f;
        int score = (std::numeric_limits<int>::lowest)();
    };

    struct PresetNode
    {
        std::uint32_t nodeId = 0;
        std::uint8_t level = 0;
    };

    struct TalentPresetSlot
    {
        std::string name;
        std::vector<PresetNode> nodes;
    };

    enum class UiLanguage
    {
        English,
        Spanish,
        French,
        German,
        Italian,
        Brazilian
    };

    struct PresetUiText
    {
        const char* languageCode;
        const char* title;
        const char* subtitle;
        const char* apply;
        const char* saveCurrent;
        const char* clearTalents;
        const char* defaultStatus;
        const char* saveFileFailed;
        const char* openSkillsBeforeSave;
        const char* savedWith;
        const char* nodes;
        const char* addedNode;
        const char* removedNode;
        const char* nodeWasNotIn;
        const char* hoverNodePrompt;
        const char* nodeReadFailed;
        const char* emptyPreset;
        const char* firstSave;
        const char* alreadyMatches;
        const char* preparing;
        const char* applyingTalents;
        const char* clearingTalents;
        const char* changes;
        const char* keepSkillsOpen;
        const char* noAssignedTalents;
        const char* clearStart;
        const char* unableClear;
        const char* treeClean;
        const char* appliedVerified;
        const char* mismatchNoSafe;
        const char* unableApply;
        const char* applying;
        const char* pass;
        const char* pendingChanges;
        const char* editName;
        const char* createUiFailed;
        const char* startUiFailed;
        const char* checkingTree;
        const char* presetAppliedFallback;
        const char* applyingPreset;
        const char* remaining;
    };

    enum class PresetJobMode
    {
        None,
        ClearOnly,
        ApplyPreset
    };

    enum class PresetJobPhase
    {
        Idle,
        Clearing,
        Applying
    };

    struct QueuedTalentAction
    {
        std::uint32_t nodeId = 0;
        bool unlearn = false;
        int attempts = 0;
        int expectedLevelAfter = -1;
        bool resetAll = false;
    };

    enum class PresetActionResult
    {
        Busy,
        Progress,
        Retried
    };

    struct PresetJobState
    {
        PresetJobMode mode = PresetJobMode::None;
        PresetJobPhase phase = PresetJobPhase::Idle;
        int slot = -1;
        int clearPass = 0;
        int applyPass = 0;
        DWORD waitUntilTick = 0;
        bool bulkResetAttempted = false;
        std::string name;
        std::vector<PresetNode> targetNodes;
    };

    struct PresetPacingState
    {
        bool pending = false;
        std::uint32_t nodeId = 0;
        bool unlearn = false;
        int expectedLevelAfter = -1;
        DWORD sentTick = 0;
    };

    struct ScopedInterlockedFlag
    {
        volatile LONG* flag = nullptr;
        bool acquired = false;

        explicit ScopedInterlockedFlag(volatile LONG* target)
            : flag(target),
            acquired(InterlockedCompareExchange(target, 1, 0) == 0)
        {
        }

        ~ScopedInterlockedFlag()
        {
            if (acquired && flag != nullptr)
                InterlockedExchange(flag, 0);
        }

        explicit operator bool() const
        {
            return acquired;
        }
    };

    std::array<TalentPresetSlot, PRESET_COUNT> g_presets = {};
    std::mutex g_presetsMutex;
    std::mutex g_presetQueueMutex;
    std::mutex g_presetJobMutex;
    std::mutex g_presetPacingMutex;
    std::mutex g_statusMutex;
    std::vector<QueuedTalentAction> g_presetActionQueue;
    PresetJobState g_presetJob;
    PresetPacingState g_presetPacing;
    std::string g_presetStatus = "F4 opens/closes presets. Save current captures your assigned talents.";
    std::string g_presetFilePath;
    std::string g_presetUiFilePath;
    std::string g_presetLangFilePath;
    UiLanguage g_uiLanguage = UiLanguage::English;
    int g_presetWindowOffsetX = -1;
    int g_presetWindowOffsetY = -1;
    bool g_presetWindowPositionLoaded = false;
    bool g_positioningPresetWindow = false;
    volatile LONG g_presetActionExecuting = 0;
    DWORD g_lastPresetQueueProcessTick = 0;
    HANDLE g_presetUiThread = nullptr;
    DWORD g_presetUiThreadId = 0;
    HWND g_presetWindow = nullptr;
    HWND g_presetNameEdits[PRESET_COUNT] = {};
    HWND g_presetApplyButtons[PRESET_COUNT] = {};
    HWND g_presetSaveButtons[PRESET_COUNT] = {};
    HWND g_presetAddHoverButtons[PRESET_COUNT] = {};
    HWND g_presetRemoveHoverButtons[PRESET_COUNT] = {};
    HWND g_presetClearAllButton = nullptr;
    HWND g_presetStatusLabel = nullptr;
    HFONT g_presetTitleFont = nullptr;
    HFONT g_presetTextFont = nullptr;

    NodeSample g_nodeSamples[MAX_NODE_SAMPLES] = {};
    volatile LONG g_nodeSampleCount = 0;

    std::array<std::uint8_t, SKILL_NODE_STATUS_STOLEN_SIZE> g_skillNodeStatusExpected = {
        0x41, 0x8B, 0x06,       // mov eax, [r14]
        0x40, 0x32, 0xFF        // xor dil, dil
    };

    std::array<std::uint8_t, SKILL_ACTION_CALLSITE_STOLEN_SIZE> g_skillActionCallsiteExpected = {
        0xE8, 0xDA, 0x47, 0x08, 0x00
    };

    std::array<std::uint8_t, SKILL_ACTION_CONTEXT_CHECK_STOLEN_SIZE> g_skillActionContextCheckExpected = {
        0x32, 0xC0, 0xC3, 0xCC, 0xCC
    };

    std::array<std::uint8_t, 3> g_skillActionContextCheckDisabledBytes = {
        0x32, 0xC0, 0xC3
    };

    std::array<std::uint8_t, 3> g_skillActionContextCheckEnabledBytes = {
        0xB0, 0x01, 0xC3
    };

    std::array<std::uint8_t, SKILL_ACTION_SLOT_RESERVE_PATCH_SIZE> g_skillActionSlotReserveExpected = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x8B
    };

    std::array<std::uint8_t, SKILL_ACTION_SLOT_RESERVE_PATCH_SIZE> g_skillActionSlotReserveEnabledBytes = {
        0xB0, 0x01, 0xC3, 0x90, 0x90, 0x90, 0x90
    };

    std::array<std::uint8_t, SKILL_ACTION_SLOT_RESERVE_PATCH_SIZE> g_skillActionSlotReserveUnlearnBytes = {
        0x80, 0xF9, 0x04, 0x0F, 0x94, 0xC0, 0xC3
    };

    std::array<std::uint8_t, DRAW_NODE_LINK_STOLEN_SIZE> g_drawNodeLinkExpected = {
        0x40, 0x55,
        0x56,
        0x57,
        0x41, 0x56,
        0x41, 0x57,
        0x48, 0x8D, 0x6C, 0x24, 0xF0,
        0x48, 0x81, 0xEC, 0x10, 0x01, 0x00, 0x00
    };

    std::array<std::uint8_t, SKILL_TREE_CONTEXT_ENTRY_STOLEN_SIZE> g_skillTreeContextExpected = {
        0x48, 0x89, 0x6C, 0x24, 0x10,
        0x48, 0x89, 0x74, 0x24, 0x20,
        0x57,
        0x48, 0x83, 0xEC, 0x50,
        0x48, 0x8B, 0x2D, 0x72, 0xD8, 0xB3, 0x01,
        0x48, 0x8B, 0xF9,
        0x0F, 0xB6, 0xF2,
        0x48, 0x8B, 0x85, 0xC8, 0x00, 0x00, 0x00,
        0x48, 0x8B, 0x08
    };

    std::string Hex(uintptr_t value)
    {
        std::ostringstream oss;
        oss << "0x" << std::hex << value;
        return oss.str();
    }

    std::uint32_t CurrentNodeId()
    {
        return static_cast<std::uint32_t>(g_lastNodeIdBits);
    }

    const char* TalentName(std::uint32_t nodeId)
    {
        switch (nodeId)
        {
        case TALENT_ID_TALAR_RIGHT_3PT:
            return "talar-right-3pt";
        case TALENT_ID_GRAPPLE:
            return "grapple";
        case TALENT_ID_TALAR_LEFT_3PT:
            return "talar-left-3pt";
        default:
            return "unknown";
        }
    }

    bool IsSingleUnlockTalent(std::uint32_t nodeId)
    {
        return nodeId == TALENT_ID_GRAPPLE;
    }

    std::uint8_t NormalizePresetLevel(std::uint32_t nodeId, std::uint8_t level)
    {
        if (level == 0)
            return 0;

        return (std::min)(level, static_cast<std::uint8_t>(3));
    }

    int LearnedApplyLayer(std::uint32_t nodeId)
    {
        // Learned from the verified apply passes: these layers are the order in
        // which the game accepted the currently saved main path from a clean tree.
        switch (nodeId)
        {
        case 29426941u:
        case 3223249169u:
        case 134064960u:
        case 4292213056u:
        case 3206142296u:
        case 1313439627u:
        case 2571965916u:
            return 0;

        case 494749233u:
        case 2903868533u:
        case 1149171418u:
        case 4261093250u:
        case 3088989644u:
            return 1;

        case 1173687821u:
        case 3141319256u:
        case 1131166881u:
        case 601427122u:
        case 4264247508u:
            return 2;

        case 3646732803u:
        case 2508204276u:
            return 3;

        case 527879180u:
        case 190967951u:
            return 4;

        default:
            return 2;
        }
    }

    using UiMessageAllocatorFn = void* (__fastcall*)(void*);
    using SkillTreeContextEntryFn = void (__fastcall*)(void*, std::uint8_t);

    void Log(const std::string& message)
    {
        if (g_modContext != nullptr)
            g_modContext->Log(message.c_str());
    }

    bool SafeRead(uintptr_t address, void* buffer, std::size_t size)
    {
        __try
        {
            std::memcpy(buffer, reinterpret_cast<const void*>(address), size);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    template <typename T>
    bool SafeReadValue(uintptr_t address, T& outValue)
    {
        return SafeRead(address, &outValue, sizeof(T));
    }

    bool SafeWrite(uintptr_t address, const void* buffer, std::size_t size)
    {
        __try
        {
            std::memcpy(reinterpret_cast<void*>(address), buffer, size);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    template <typename T>
    bool SafeWriteValue(uintptr_t address, const T& value)
    {
        return SafeWrite(address, &value, sizeof(T));
    }

    int ScoreNodeMatch(const LiveNodeMatch& match)
    {
        int score = 0;

        if (match.state4 == 3)
            score += 8;
        if (match.state40 == 3)
            score += 4;
        if (match.local28 <= 1)
            score += 2;
        if (match.state43 <= 1)
            score += 1;
        if (match.node != 0 && match.state != 0 && match.node != match.state)
            score += 1;

        return score;
    }

    bool IsLikelyUserPointer(uintptr_t value)
    {
        return value >= 0x10000ull && value < 0x0000800000000000ull;
    }

    bool TryBuildNodeMatch(uintptr_t candidate, std::uint32_t nodeId, LiveNodeMatch& outMatch)
    {
        std::uint32_t foundNodeId = 0;
        if (!SafeReadValue(candidate, foundNodeId) || foundNodeId != nodeId)
            return false;

        uintptr_t state = 0;
        if (!SafeReadValue(candidate + 0x48, state) || !IsLikelyUserPointer(state))
            return false;

        std::uint32_t stateNodeId = 0;
        if (!SafeReadValue(state, stateNodeId) || stateNodeId != nodeId)
            return false;

        LiveNodeMatch match = {};
        match.node = candidate;
        match.state = state;
        match.nodeId = nodeId;
        SafeReadValue(candidate + 0x04, match.uiX);
        SafeReadValue(candidate + 0x08, match.uiY);
        SafeReadValue(candidate + 0x28, match.local28);
        SafeReadValue(state + 0x04, match.state4);
        SafeReadValue(state + 0x1C, match.cost);
        SafeReadValue(state + 0x40, match.state40);
        SafeReadValue(state + 0x41, match.state41);
        SafeReadValue(state + 0x42, match.state42);
        SafeReadValue(state + 0x43, match.state43);
        match.score = ScoreNodeMatch(match);

        outMatch = match;
        return true;
    }

    bool TryBuildNodeMatchFromPtr(uintptr_t candidate, LiveNodeMatch& outMatch)
    {
        if (!IsLikelyUserPointer(candidate))
            return false;

        std::uint32_t nodeId = 0;
        if (!SafeReadValue(candidate, nodeId) || nodeId == 0)
            return false;

        return TryBuildNodeMatch(candidate, nodeId, outMatch);
    }

    void RememberTreeUiRoot(uintptr_t root)
    {
        if (root == 0)
            return;

        std::uint16_t count = 0;
        if (!SafeReadValue(root + SKILL_TREE_NODE_COUNT_OFFSET, count))
            return;

        if (count == 0 || count > MAX_SKILL_TREE_NODES)
            return;

        InterlockedExchange64(&g_lastTreeUiRoot, static_cast<LONG64>(root));
        InterlockedExchange(&g_lastTreeNodeCount, static_cast<LONG>(count));
        InterlockedExchange(&g_lastTreeTick, static_cast<LONG>(GetTickCount()));
    }

    bool ResolveTreeUiRoot(uintptr_t& root, std::uint16_t& count)
    {
        root = static_cast<uintptr_t>(g_lastTreeUiRoot);
        count = static_cast<std::uint16_t>(g_lastTreeNodeCount);

        if (root == 0 || count == 0 || count > MAX_SKILL_TREE_NODES)
            return false;

        std::uint16_t liveCount = 0;
        if (!SafeReadValue(root + SKILL_TREE_NODE_COUNT_OFFSET, liveCount))
            return false;

        if (liveCount == 0 || liveCount > MAX_SKILL_TREE_NODES)
            return false;

        count = liveCount;
        InterlockedExchange(&g_lastTreeNodeCount, static_cast<LONG>(liveCount));
        return true;
    }

    bool FindLiveNodeById(std::uint32_t nodeId, LiveNodeMatch& outMatch, std::size_t* outMatchCount = nullptr)
    {
        uintptr_t treeRoot = 0;
        std::uint16_t nodeCount = 0;
        if (!ResolveTreeUiRoot(treeRoot, nodeCount))
        {
            if (outMatchCount != nullptr)
                *outMatchCount = 0;
            return false;
        }

        std::size_t matchCount = 0;
        LiveNodeMatch bestMatch = {};

        for (std::uint16_t index = 0; index < nodeCount; ++index)
        {
            LiveNodeMatch match = {};
            const uintptr_t candidate = treeRoot + SKILL_TREE_NODE_ARRAY_OFFSET + static_cast<std::size_t>(index) * SKILL_TREE_NODE_STRIDE;
            if (!TryBuildNodeMatch(candidate, nodeId, match))
                continue;

            ++matchCount;
            if (bestMatch.node == 0 || match.score > bestMatch.score)
                bestMatch = match;
        }

        if (outMatchCount != nullptr)
            *outMatchCount = matchCount;

        outMatch = bestMatch;
        return bestMatch.node != 0;
    }

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
            return L"";

        const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (length <= 1)
            return L"";

        std::wstring result(static_cast<std::size_t>(length - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &result[0], length);
        return result;
    }

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty())
            return "";

        const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (length <= 1)
            return "";

        std::string result(static_cast<std::size_t>(length - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &result[0], length, nullptr, nullptr);
        return result;
    }

    const PresetUiText& Text()
    {
        static const PresetUiText english = {
            "english",
            "Talent Presets",
            "F4 hides the panel. Drag the header to move it.",
            "Apply",
            "Save current",
            "Clear talents",
            "F4 opens/closes presets. Save current captures your assigned talents.",
            "Could not save talent_presets.txt",
            "Open Skills once before saving the preset.",
            " saved with ",
            " nodes.",
            "Added node ",
            "Removed node ",
            "That node was not in ",
            "Hover a tree node and press the button again.",
            "Could not read the node under the cursor.",
            "That preset is empty. Press Save current first.",
            "Press Save current first.",
            " already matches your current talents.",
            "Preparing ",
            "applying talents",
            "clearing current talents",
            " changes). ",
            "Keep Skills open.",
            "No assigned talents to clear.",
            "Clearing talents: ",
            "Could not fully clear the tree. The game keeps rejecting some nodes.",
            "Talent tree cleared and verified.",
            " applied and verified.",
            "The preset does not match, but I cannot find safe changes to apply. Save/retry with Skills open.",
            "Could not apply all talents. The game keeps rejecting some nodes.",
            "Applying ",
            ", pass ",
            " pending changes.",
            "Editing preset name. It will be saved when you leave the field.",
            "Could not create the preset UI.",
            "Could not start the preset UI.",
            "Checking the tree before the next phase...",
            "Preset applied. If one node is still pending, press Apply again with Skills open.",
            "Applying preset... ",
            " remaining changes."
        };

        static const PresetUiText spanish = {
            "spanish",
            "Presets de Talentos",
            "F4 oculta el panel. Arrastra la cabecera para moverlo.",
            "Aplicar",
            "Guardar actual",
            "Borrar talentos",
            "F4 abre/cierra presets. Guardar actual captura todos los talentos asignados.",
            "No se pudo guardar talent_presets.txt",
            "Abre Habilidades una vez antes de guardar el preset.",
            " guardado con ",
            " nodos.",
            "Anadido nodo ",
            "Quitado nodo ",
            "Ese nodo no estaba en ",
            "Pon el raton encima de un nodo del arbol y pulsa el boton otra vez.",
            "No pude leer el nodo bajo el raton.",
            "Ese preset esta vacio. Primero pulsa Guardar actual.",
            "Primero pulsa Guardar actual.",
            " ya coincide con tus talentos actuales.",
            "Preparando ",
            "aplicando talentos",
            "borrando talentos actuales",
            " cambios). ",
            "Manten Habilidades abierto.",
            "No hay talentos asignados para borrar.",
            "Borrando talentos: ",
            "No pude dejar el arbol a cero. Hay nodos que el juego sigue rechazando.",
            "Arbol de talentos limpio y verificado.",
            " aplicado y verificado.",
            "El preset no coincide, pero no encuentro cambios seguros que aplicar. Guarda/reintenta con Habilidades abierto.",
            "No pude aplicar todos los talentos. El juego sigue rechazando algunos nodos.",
            "Aplicando ",
            ", pasada ",
            " cambios pendientes.",
            "Editando nombre del preset. Se guardara al salir del campo.",
            "No se pudo crear la interfaz de presets.",
            "No se pudo iniciar la interfaz de presets.",
            "Comprobando el arbol antes de la siguiente fase...",
            "Preset aplicado. Si ves un nodo pendiente, pulsa Aplicar otra vez con Habilidades abierto.",
            "Aplicando preset... quedan ",
            " cambios."
        };

        static const PresetUiText french = {
            "french",
            "Presets de Talents",
            "F4 masque le panneau. Faites glisser l'en-tete pour le deplacer.",
            "Appliquer",
            "Sauver actuel",
            "Effacer talents",
            "F4 ouvre/ferme les presets. Sauver actuel capture vos talents assignes.",
            "Impossible de sauvegarder talent_presets.txt",
            "Ouvrez Competences une fois avant de sauvegarder le preset.",
            " sauvegarde avec ",
            " noeuds.",
            "Noeud ajoute ",
            "Noeud retire ",
            "Ce noeud n'etait pas dans ",
            "Placez la souris sur un noeud et appuyez encore sur le bouton.",
            "Impossible de lire le noeud sous la souris.",
            "Ce preset est vide. Appuyez d'abord sur Sauver actuel.",
            "Appuyez d'abord sur Sauver actuel.",
            " correspond deja a vos talents actuels.",
            "Preparation ",
            "application des talents",
            "effacement des talents actuels",
            " changements). ",
            "Gardez Competences ouvert.",
            "Aucun talent assigne a effacer.",
            "Effacement des talents: ",
            "Impossible de vider totalement l'arbre. Le jeu rejette encore certains noeuds.",
            "Arbre de talents vide et verifie.",
            " applique et verifie.",
            "Le preset ne correspond pas, mais aucun changement sur n'a ete trouve. Sauvegardez/reessayez avec Competences ouvert.",
            "Impossible d'appliquer tous les talents. Le jeu rejette encore certains noeuds.",
            "Application ",
            ", passe ",
            " changements restants.",
            "Edition du nom du preset. Il sera sauvegarde en quittant le champ.",
            "Impossible de creer l'interface des presets.",
            "Impossible de demarrer l'interface des presets.",
            "Verification de l'arbre avant la phase suivante...",
            "Preset applique. Si un noeud reste en attente, appuyez encore sur Appliquer avec Competences ouvert.",
            "Application du preset... ",
            " changements restants."
        };

        static const PresetUiText german = {
            "german",
            "Talent-Presets",
            "F4 blendet das Panel aus. Ziehe die Kopfzeile zum Verschieben.",
            "Anwenden",
            "Aktuell speichern",
            "Talente leeren",
            "F4 oeffnet/schliesst Presets. Aktuell speichern erfasst deine Talente.",
            "talent_presets.txt konnte nicht gespeichert werden",
            "Oeffne Faehigkeiten einmal, bevor du das Preset speicherst.",
            " gespeichert mit ",
            " Knoten.",
            "Knoten hinzugefuegt ",
            "Knoten entfernt ",
            "Dieser Knoten war nicht in ",
            "Halte die Maus ueber einen Knoten und druecke die Taste erneut.",
            "Knoten unter der Maus konnte nicht gelesen werden.",
            "Dieses Preset ist leer. Druecke zuerst Aktuell speichern.",
            "Druecke zuerst Aktuell speichern.",
            " entspricht bereits deinen aktuellen Talenten.",
            "Vorbereitung ",
            "Talente anwenden",
            "aktuelle Talente leeren",
            " Aenderungen). ",
            "Lass Faehigkeiten geoeffnet.",
            "Keine zu leerenden Talente zugewiesen.",
            "Talente werden geleert: ",
            "Der Baum konnte nicht vollstaendig geleert werden. Das Spiel lehnt einige Knoten weiter ab.",
            "Talentbaum geleert und geprueft.",
            " angewendet und geprueft.",
            "Das Preset stimmt nicht ueberein, aber es wurden keine sicheren Aenderungen gefunden. Speichern/erneut versuchen mit geoeffneten Faehigkeiten.",
            "Nicht alle Talente konnten angewendet werden. Das Spiel lehnt einige Knoten weiter ab.",
            "Anwenden ",
            ", Durchlauf ",
            " ausstehende Aenderungen.",
            "Preset-Name wird bearbeitet. Er wird beim Verlassen des Feldes gespeichert.",
            "Preset-UI konnte nicht erstellt werden.",
            "Preset-UI konnte nicht gestartet werden.",
            "Baum wird vor der naechsten Phase geprueft...",
            "Preset angewendet. Wenn ein Knoten fehlt, druecke Anwenden erneut mit geoeffneten Faehigkeiten.",
            "Preset wird angewendet... ",
            " verbleibende Aenderungen."
        };

        static const PresetUiText italian = {
            "italian",
            "Preset Talenti",
            "F4 nasconde il pannello. Trascina l'intestazione per spostarlo.",
            "Applica",
            "Salva attuale",
            "Cancella talenti",
            "F4 apre/chiude i preset. Salva attuale cattura i talenti assegnati.",
            "Impossibile salvare talent_presets.txt",
            "Apri Abilita una volta prima di salvare il preset.",
            " salvato con ",
            " nodi.",
            "Nodo aggiunto ",
            "Nodo rimosso ",
            "Quel nodo non era in ",
            "Metti il mouse su un nodo e premi di nuovo il pulsante.",
            "Impossibile leggere il nodo sotto il mouse.",
            "Questo preset e vuoto. Premi prima Salva attuale.",
            "Premi prima Salva attuale.",
            " corrisponde gia ai talenti attuali.",
            "Preparazione ",
            "applicazione talenti",
            "cancellazione talenti attuali",
            " modifiche). ",
            "Tieni Abilita aperto.",
            "Nessun talento assegnato da cancellare.",
            "Cancellazione talenti: ",
            "Impossibile pulire del tutto l'albero. Il gioco continua a rifiutare alcuni nodi.",
            "Albero talenti pulito e verificato.",
            " applicato e verificato.",
            "Il preset non corrisponde, ma non trovo modifiche sicure da applicare. Salva/riprova con Abilita aperto.",
            "Impossibile applicare tutti i talenti. Il gioco continua a rifiutare alcuni nodi.",
            "Applicazione ",
            ", passata ",
            " modifiche in attesa.",
            "Modifica nome preset. Sara salvato quando esci dal campo.",
            "Impossibile creare l'interfaccia preset.",
            "Impossibile avviare l'interfaccia preset.",
            "Controllo dell'albero prima della fase successiva...",
            "Preset applicato. Se resta un nodo in sospeso, premi di nuovo Applica con Abilita aperto.",
            "Applicazione preset... ",
            " modifiche rimaste."
        };

        static const PresetUiText brazilian = {
            "brazilian",
            "Predefinicoes de Talentos",
            "F4 oculta o painel. Arraste o cabecalho para mover.",
            "Aplicar",
            "Salvar atual",
            "Limpar talentos",
            "F4 abre/fecha presets. Salvar atual captura seus talentos atribuidos.",
            "Nao foi possivel salvar talent_presets.txt",
            "Abra Habilidades uma vez antes de salvar o preset.",
            " salvo com ",
            " nos.",
            "No adicionado ",
            "No removido ",
            "Esse no nao estava em ",
            "Passe o mouse sobre um no da arvore e pressione o botao novamente.",
            "Nao foi possivel ler o no sob o mouse.",
            "Esse preset esta vazio. Pressione Salvar atual primeiro.",
            "Pressione Salvar atual primeiro.",
            " ja corresponde aos seus talentos atuais.",
            "Preparando ",
            "aplicando talentos",
            "limpando talentos atuais",
            " alteracoes). ",
            "Mantenha Habilidades aberto.",
            "Nao ha talentos atribuidos para limpar.",
            "Limpando talentos: ",
            "Nao foi possivel zerar a arvore. O jogo ainda rejeita alguns nos.",
            "Arvore de talentos limpa e verificada.",
            " aplicado e verificado.",
            "O preset nao corresponde, mas nao encontro alteracoes seguras para aplicar. Salve/tente de novo com Habilidades aberto.",
            "Nao foi possivel aplicar todos os talentos. O jogo ainda rejeita alguns nos.",
            "Aplicando ",
            ", passada ",
            " alteracoes pendentes.",
            "Editando nome do preset. Sera salvo ao sair do campo.",
            "Nao foi possivel criar a interface de presets.",
            "Nao foi possivel iniciar a interface de presets.",
            "Verificando a arvore antes da proxima fase...",
            "Preset aplicado. Se ainda houver um no pendente, pressione Aplicar de novo com Habilidades aberto.",
            "Aplicando preset... faltam ",
            " alteracoes."
        };

        switch (g_uiLanguage)
        {
        case UiLanguage::Spanish:
            return spanish;
        case UiLanguage::French:
            return french;
        case UiLanguage::German:
            return german;
        case UiLanguage::Italian:
            return italian;
        case UiLanguage::Brazilian:
            return brazilian;
        case UiLanguage::English:
        default:
            return english;
        }
    }

    std::string TrimAscii(const std::string& value)
    {
        std::size_t begin = 0;
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0)
            ++begin;

        std::size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
            --end;

        return value.substr(begin, end - begin);
    }

    std::string ToLowerAscii(std::string value)
    {
        for (char& ch : value)
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return value;
    }

    UiLanguage LanguageFromToken(const std::string& rawToken)
    {
        const std::string token = ToLowerAscii(TrimAscii(rawToken));
        if (token == "spanish" || token == "es" || token == "es-es" || token == "latam")
            return UiLanguage::Spanish;
        if (token == "french" || token == "fr" || token == "fr-fr")
            return UiLanguage::French;
        if (token == "german" || token == "de" || token == "de-de")
            return UiLanguage::German;
        if (token == "italian" || token == "it" || token == "it-it")
            return UiLanguage::Italian;
        if (token == "brazilian" || token == "portuguese" || token == "pt" || token == "pt-br")
            return UiLanguage::Brazilian;

        return UiLanguage::English;
    }

    std::string ReadLanguageOverride()
    {
        if (g_presetLangFilePath.empty())
            return "";

        std::ifstream file(g_presetLangFilePath);
        if (!file)
            return "";

        std::string line;
        while (std::getline(file, line))
        {
            line = TrimAscii(line);
            if (line.empty() || line[0] == '#')
                continue;

            const std::size_t equals = line.find('=');
            if (equals != std::string::npos)
                line = line.substr(equals + 1);

            return TrimAscii(line);
        }

        return "";
    }

    std::string ExtractSteamLanguageFromManifest(const std::string& manifestPath)
    {
        std::ifstream file(manifestPath);
        if (!file)
            return "";

        std::string line;
        while (std::getline(file, line))
        {
            const std::string lowered = ToLowerAscii(line);
            if (lowered.find("\"language\"") == std::string::npos)
                continue;

            const std::size_t lastQuote = line.find_last_of('"');
            if (lastQuote == std::string::npos || lastQuote == 0)
                continue;

            const std::size_t prevQuote = line.find_last_of('"', lastQuote - 1);
            if (prevQuote == std::string::npos || prevQuote + 1 >= lastQuote)
                continue;

            return line.substr(prevQuote + 1, lastQuote - prevQuote - 1);
        }

        return "";
    }

    std::string BuildFullPath(const std::string& base, const std::string& relative)
    {
        char buffer[MAX_PATH] = {};
        const std::string combined = base + "\\" + relative;
        const DWORD length = GetFullPathNameA(combined.c_str(), static_cast<DWORD>(sizeof(buffer)), buffer, nullptr);
        if (length == 0 || length >= sizeof(buffer))
            return combined;

        return buffer;
    }

    std::string DetectSteamLanguage(const std::string& moduleDir)
    {
        if (!moduleDir.empty())
        {
            const std::string manifestPath = BuildFullPath(moduleDir, "..\\..\\..\\..\\appmanifest_1203620.acf");
            const std::string language = ExtractSteamLanguageFromManifest(manifestPath);
            if (!language.empty())
                return language;
        }

        const char* fallbackManifest = "C:\\Program Files (x86)\\Steam\\steamapps\\appmanifest_1203620.acf";
        return ExtractSteamLanguageFromManifest(fallbackManifest);
    }

    std::string DetectWindowsLanguage()
    {
        wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {};
        if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) <= 0)
            return "";

        return WideToUtf8(localeName);
    }

    void DetectUiLanguage(const std::string& moduleDir)
    {
        std::string token = ReadLanguageOverride();
        if (token.empty())
            token = DetectSteamLanguage(moduleDir);
        if (token.empty())
            token = DetectWindowsLanguage();

        g_uiLanguage = LanguageFromToken(token);
        g_presetStatus = Text().defaultStatus;
    }

    void SetPresetStatus(const std::string& status)
    {
        {
            std::lock_guard<std::mutex> lock(g_statusMutex);
            g_presetStatus = status;
        }

        HWND window = g_presetWindow;
        if (window != nullptr)
            PostMessageW(window, WM_TALENT_PRESETS_REFRESH, 0, 0);
    }

    std::wstring GetPresetStatusWide()
    {
        std::lock_guard<std::mutex> lock(g_statusMutex);
        return Utf8ToWide(g_presetStatus);
    }

    std::string CleanPresetName(const std::string& name, int slot)
    {
        std::string cleaned;
        cleaned.reserve(name.size());
        for (char ch : name)
        {
            if (ch == '|' || ch == '\r' || ch == '\n' || ch == '\t')
                continue;
            cleaned.push_back(ch);
        }

        if (cleaned.empty())
        {
            std::ostringstream oss;
            oss << "Preset " << (slot + 1);
            return oss.str();
        }

        if (cleaned.size() > 48)
            cleaned.resize(48);

        return cleaned;
    }

    void EnsurePresetDefaults()
    {
        std::lock_guard<std::mutex> lock(g_presetsMutex);
        for (int index = 0; index < PRESET_COUNT; ++index)
        {
            if (g_presets[index].name.empty())
            {
                std::ostringstream oss;
                oss << "Preset " << (index + 1);
                g_presets[index].name = oss.str();
            }
        }
    }

    std::string ModuleDirectory()
    {
        char path[MAX_PATH] = {};
        if (g_moduleHandle == nullptr || GetModuleFileNameA(g_moduleHandle, path, static_cast<DWORD>(sizeof(path))) == 0)
            return "";

        std::string value = path;
        const std::size_t slash = value.find_last_of("\\/");
        if (slash == std::string::npos)
            return "";

        return value.substr(0, slash);
    }

    bool ParseUint32(const std::string& value, std::uint32_t& out)
    {
        if (value.empty())
            return false;

        char* endPtr = nullptr;
        const unsigned long parsed = std::strtoul(value.c_str(), &endPtr, 10);
        if (endPtr == nullptr || *endPtr != '\0' || parsed > 0xFFFFFFFFul)
            return false;

        out = static_cast<std::uint32_t>(parsed);
        return true;
    }

    bool ParseUint8(const std::string& value, std::uint8_t& out)
    {
        std::uint32_t parsed = 0;
        if (!ParseUint32(value, parsed) || parsed > 255)
            return false;
        out = static_cast<std::uint8_t>(parsed);
        return true;
    }

    void SavePresetsToDisk()
    {
        if (g_presetFilePath.empty())
            return;

        std::array<TalentPresetSlot, PRESET_COUNT> snapshot = {};
        {
            std::lock_guard<std::mutex> lock(g_presetsMutex);
            snapshot = g_presets;
        }

        std::ofstream file(g_presetFilePath, std::ios::out | std::ios::trunc);
        if (!file)
        {
            SetPresetStatus(Text().saveFileFailed);
            return;
        }

        file << "TalentPresetsV1\n";
        for (int index = 0; index < PRESET_COUNT; ++index)
        {
            file << index << "|" << CleanPresetName(snapshot[index].name, index) << "|";
            for (std::size_t nodeIndex = 0; nodeIndex < snapshot[index].nodes.size(); ++nodeIndex)
            {
                if (nodeIndex != 0)
                    file << ",";
                file << snapshot[index].nodes[nodeIndex].nodeId << ":"
                    << static_cast<unsigned int>(snapshot[index].nodes[nodeIndex].level);
            }
            file << "\n";
        }
    }

    void LoadPresetsFromDisk()
    {
        EnsurePresetDefaults();

        if (g_presetFilePath.empty())
            return;

        std::ifstream file(g_presetFilePath);
        if (!file)
            return;

        std::array<TalentPresetSlot, PRESET_COUNT> loaded = {};
        for (int index = 0; index < PRESET_COUNT; ++index)
        {
            std::ostringstream oss;
            oss << "Preset " << (index + 1);
            loaded[index].name = oss.str();
        }

        std::string line;
        bool firstLine = true;
        while (std::getline(file, line))
        {
            if (firstLine)
            {
                firstLine = false;
                if (line == "TalentPresetsV1")
                    continue;
            }

            const std::size_t first = line.find('|');
            const std::size_t second = first == std::string::npos ? std::string::npos : line.find('|', first + 1);
            if (first == std::string::npos || second == std::string::npos)
                continue;

            std::uint32_t slotValue = 0;
            if (!ParseUint32(line.substr(0, first), slotValue) || slotValue >= PRESET_COUNT)
                continue;

            TalentPresetSlot slot = {};
            slot.name = CleanPresetName(line.substr(first + 1, second - first - 1), static_cast<int>(slotValue));

            std::string nodes = line.substr(second + 1);
            std::size_t offset = 0;
            while (offset < nodes.size())
            {
                const std::size_t comma = nodes.find(',', offset);
                const std::size_t end = comma == std::string::npos ? nodes.size() : comma;
                const std::size_t colon = nodes.find(':', offset);
                if (colon != std::string::npos && colon < end)
                {
                    std::uint32_t nodeId = 0;
                    std::uint8_t level = 0;
                    if (ParseUint32(nodes.substr(offset, colon - offset), nodeId) &&
                        ParseUint8(nodes.substr(colon + 1, end - colon - 1), level) &&
                        nodeId != 0 && level != 0)
                    {
                        level = NormalizePresetLevel(nodeId, level);
                        slot.nodes.push_back({ nodeId, level });
                    }
                }

                if (comma == std::string::npos)
                    break;
                offset = comma + 1;
            }

            loaded[slotValue] = slot;
        }

        {
            std::lock_guard<std::mutex> lock(g_presetsMutex);
            g_presets = loaded;
        }
    }

    void LoadPresetUiPosition()
    {
        g_presetWindowPositionLoaded = false;
        if (g_presetUiFilePath.empty())
            return;

        std::ifstream file(g_presetUiFilePath);
        if (!file)
            return;

        std::string line;
        while (std::getline(file, line))
        {
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos)
                continue;

            std::uint32_t value = 0;
            if (!ParseUint32(line.substr(equals + 1), value))
                continue;

            if (line.compare(0, equals, "offsetX") == 0)
                g_presetWindowOffsetX = static_cast<int>(value);
            else if (line.compare(0, equals, "offsetY") == 0)
                g_presetWindowOffsetY = static_cast<int>(value);
        }

        g_presetWindowPositionLoaded = g_presetWindowOffsetX >= 0 && g_presetWindowOffsetY >= 0;
    }

    void SavePresetUiPosition()
    {
        if (g_presetUiFilePath.empty() || g_presetWindowOffsetX < 0 || g_presetWindowOffsetY < 0)
            return;

        std::ofstream file(g_presetUiFilePath, std::ios::out | std::ios::trunc);
        if (!file)
            return;

        file << "TalentPresetUiV1\n";
        file << "offsetX=" << g_presetWindowOffsetX << "\n";
        file << "offsetY=" << g_presetWindowOffsetY << "\n";
    }

    int CurrentTalentLevel(const LiveNodeMatch& match)
    {
        if (match.nodeId == 0 || match.cost == 0 || match.cost == 0xFFFF)
            return 0;

        if (match.state40 != 0xFF && match.state40 > 0 && match.state41 != 0xFF && match.state41 > 0)
            return (std::min)(static_cast<int>(match.state41), static_cast<int>(match.state40));

        if (IsSingleUnlockTalent(match.nodeId))
            return match.state43 > 0 ? 1 : 0;

        if (match.state43 > 0 && match.state4 != 0)
            return 1;

        return 0;
    }

    void PushOrRaisePresetNode(std::vector<PresetNode>& nodes, std::uint32_t nodeId, std::uint8_t level)
    {
        level = NormalizePresetLevel(nodeId, level);
        if (level == 0)
            return;

        for (PresetNode& node : nodes)
        {
            if (node.nodeId == nodeId)
            {
                node.level = (std::max)(node.level, level);
                return;
            }
        }

        nodes.push_back({ nodeId, level });
    }

    bool CaptureCurrentTalentNodes(std::vector<PresetNode>& outNodes, std::string& error)
    {
        uintptr_t treeRoot = 0;
        std::uint16_t nodeCount = 0;
        if (!ResolveTreeUiRoot(treeRoot, nodeCount))
        {
            error = Text().openSkillsBeforeSave;
            return false;
        }

        std::vector<PresetNode> captured;
        for (std::uint16_t index = 0; index < nodeCount; ++index)
        {
            LiveNodeMatch match = {};
            const uintptr_t candidate = treeRoot + SKILL_TREE_NODE_ARRAY_OFFSET + static_cast<std::size_t>(index) * SKILL_TREE_NODE_STRIDE;
            if (!TryBuildNodeMatchFromPtr(candidate, match))
                continue;

            const int level = CurrentTalentLevel(match);
            if (level <= 0)
                continue;

            PushOrRaisePresetNode(captured, match.nodeId, static_cast<std::uint8_t>(level));
        }

        outNodes = captured;
        return true;
    }

    void SavePresetNamesFromUi()
    {
        if (g_presetWindow == nullptr)
            return;

        std::lock_guard<std::mutex> lock(g_presetsMutex);
        for (int index = 0; index < PRESET_COUNT; ++index)
        {
            HWND edit = g_presetNameEdits[index];
            if (edit == nullptr)
                continue;

            wchar_t text[96] = {};
            GetWindowTextW(edit, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
            g_presets[index].name = CleanPresetName(WideToUtf8(text), index);
        }
    }

    void SaveCurrentToPreset(int slot)
    {
        if (slot < 0 || slot >= PRESET_COUNT)
            return;

        SavePresetNamesFromUi();

        std::vector<PresetNode> nodes;
        std::string error;
        if (!CaptureCurrentTalentNodes(nodes, error))
        {
            SetPresetStatus(error);
            return;
        }

        std::string name;
        {
            std::lock_guard<std::mutex> lock(g_presetsMutex);
            g_presets[slot].nodes = nodes;
            g_presets[slot].name = CleanPresetName(g_presets[slot].name, slot);
            name = g_presets[slot].name;
        }

        SavePresetsToDisk();

        std::ostringstream oss;
        oss << name << Text().savedWith << nodes.size() << Text().nodes;
        SetPresetStatus(oss.str());
    }

    bool GetFreshHoveredNode(LiveNodeMatch& outMatch, std::string& error)
    {
        const DWORD lastTick = static_cast<DWORD>(g_lastNodeTick);
        if (lastTick == 0 || GetTickCount() - lastTick > NODE_CAPTURE_STALE_MS)
        {
            error = Text().hoverNodePrompt;
            return false;
        }

        const uintptr_t nodePtr = static_cast<uintptr_t>(g_lastNodePtr);
        if (!TryBuildNodeMatchFromPtr(nodePtr, outMatch))
        {
            error = Text().nodeReadFailed;
            return false;
        }

        return true;
    }

    void AddHoveredNodeToPreset(int slot)
    {
        if (slot < 0 || slot >= PRESET_COUNT)
            return;

        SavePresetNamesFromUi();

        LiveNodeMatch match = {};
        std::string error;
        if (!GetFreshHoveredNode(match, error))
        {
            SetPresetStatus(error);
            return;
        }

        int level = CurrentTalentLevel(match);
        if (level <= 0)
            level = 1;

        std::string name;
        {
            std::lock_guard<std::mutex> lock(g_presetsMutex);
            PushOrRaisePresetNode(g_presets[slot].nodes, match.nodeId, static_cast<std::uint8_t>(level));
            g_presets[slot].name = CleanPresetName(g_presets[slot].name, slot);
            name = g_presets[slot].name;
        }

        SavePresetsToDisk();

        std::ostringstream oss;
        oss << Text().addedNode << match.nodeId << " -> " << name << ".";
        SetPresetStatus(oss.str());
    }

    void RemoveHoveredNodeFromPreset(int slot)
    {
        if (slot < 0 || slot >= PRESET_COUNT)
            return;

        SavePresetNamesFromUi();

        LiveNodeMatch match = {};
        std::string error;
        if (!GetFreshHoveredNode(match, error))
        {
            SetPresetStatus(error);
            return;
        }

        bool removed = false;
        std::string name;
        {
            std::lock_guard<std::mutex> lock(g_presetsMutex);
            auto& nodes = g_presets[slot].nodes;
            for (auto it = nodes.begin(); it != nodes.end(); ++it)
            {
                if (it->nodeId == match.nodeId)
                {
                    nodes.erase(it);
                    removed = true;
                    break;
                }
            }
            g_presets[slot].name = CleanPresetName(g_presets[slot].name, slot);
            name = g_presets[slot].name;
        }

        SavePresetsToDisk();

        std::ostringstream oss;
        if (removed)
            oss << Text().removedNode << match.nodeId << " <- " << name << ".";
        else
            oss << Text().nodeWasNotIn << name << ".";
        SetPresetStatus(oss.str());
    }

    std::map<std::uint32_t, std::uint8_t> PresetNodeMap(const std::vector<PresetNode>& nodes)
    {
        std::map<std::uint32_t, std::uint8_t> result;
        for (const PresetNode& node : nodes)
        {
            const std::uint8_t level = NormalizePresetLevel(node.nodeId, node.level);
            if (level == 0)
                continue;

            auto it = result.find(node.nodeId);
            if (it == result.end() || it->second < level)
                result[node.nodeId] = level;
        }
        return result;
    }

    void ReplacePresetQueue(const std::vector<QueuedTalentAction>& actions)
    {
        std::lock_guard<std::mutex> lock(g_presetQueueMutex);
        g_presetActionQueue = actions;
    }

    bool IsPresetJobActive()
    {
        std::lock_guard<std::mutex> lock(g_presetJobMutex);
        return g_presetJob.mode != PresetJobMode::None;
    }

    void ClearPresetJob()
    {
        std::lock_guard<std::mutex> lock(g_presetJobMutex);
        g_presetJob = {};
    }

    void SetPresetJobWait(DWORD waitUntilTick)
    {
        std::lock_guard<std::mutex> lock(g_presetJobMutex);
        if (g_presetJob.mode != PresetJobMode::None)
            g_presetJob.waitUntilTick = waitUntilTick;
    }

    void QueuePresetActionsForJob(const std::vector<QueuedTalentAction>& actions)
    {
        ReplacePresetQueue(actions);
        g_lastPresetQueueProcessTick = 0;
        {
            std::lock_guard<std::mutex> lock(g_presetPacingMutex);
            g_presetPacing = {};
        }
        SetPresetJobWait(0);
    }

    bool TryPeekPresetAction(QueuedTalentAction& outAction, std::size_t& queueSize)
    {
        std::lock_guard<std::mutex> lock(g_presetQueueMutex);
        queueSize = g_presetActionQueue.size();
        if (g_presetActionQueue.empty())
            return false;

        outAction = g_presetActionQueue.front();
        return true;
    }

    void DropPresetAction()
    {
        std::lock_guard<std::mutex> lock(g_presetQueueMutex);
        if (!g_presetActionQueue.empty())
            g_presetActionQueue.erase(g_presetActionQueue.begin());
    }

    void RetryPresetActionLater(QueuedTalentAction action)
    {
        std::lock_guard<std::mutex> lock(g_presetQueueMutex);
        if (!g_presetActionQueue.empty())
            g_presetActionQueue.erase(g_presetActionQueue.begin());

        ++action.attempts;
        g_presetActionQueue.push_back(action);
    }

    std::size_t PresetQueueSize()
    {
        std::lock_guard<std::mutex> lock(g_presetQueueMutex);
        return g_presetActionQueue.size();
    }

    std::vector<QueuedTalentAction> BuildClearTalentActions(const std::vector<PresetNode>& currentNodes)
    {
        std::vector<QueuedTalentAction> actions;
        for (auto it = currentNodes.rbegin(); it != currentNodes.rend(); ++it)
        {
            const std::uint8_t level = NormalizePresetLevel(it->nodeId, it->level);
            for (int count = 0; count < static_cast<int>(level); ++count)
                actions.push_back({ it->nodeId, true, 0, static_cast<int>(level) - count - 1 });
        }
        return actions;
    }

    std::vector<QueuedTalentAction> BuildBulkClearTalentActions()
    {
        return { { 0, false, 0, -1, true } };
    }

    bool IsUsableNodeCoordinate(float value)
    {
        return std::isfinite(value) && std::fabs(value) > 0.001f && std::fabs(value) < 100000.0f;
    }

    bool ComputeLiveTreeCenter(float& centerX, float& centerY)
    {
        uintptr_t treeRoot = 0;
        std::uint16_t nodeCount = 0;
        if (!ResolveTreeUiRoot(treeRoot, nodeCount))
            return false;

        float minX = (std::numeric_limits<float>::max)();
        float minY = (std::numeric_limits<float>::max)();
        float maxX = -(std::numeric_limits<float>::max)();
        float maxY = -(std::numeric_limits<float>::max)();
        int valid = 0;

        for (std::uint16_t index = 0; index < nodeCount; ++index)
        {
            LiveNodeMatch match = {};
            const uintptr_t candidate = treeRoot + SKILL_TREE_NODE_ARRAY_OFFSET + static_cast<std::size_t>(index) * SKILL_TREE_NODE_STRIDE;
            if (!TryBuildNodeMatchFromPtr(candidate, match))
                continue;

            if (!IsUsableNodeCoordinate(match.uiX) || !IsUsableNodeCoordinate(match.uiY))
                continue;

            minX = (std::min)(minX, match.uiX);
            minY = (std::min)(minY, match.uiY);
            maxX = (std::max)(maxX, match.uiX);
            maxY = (std::max)(maxY, match.uiY);
            ++valid;
        }

        if (valid < 4)
            return false;

        centerX = (minX + maxX) * 0.5f;
        centerY = (minY + maxY) * 0.5f;
        return true;
    }

    struct ApplyTalentCandidate
    {
        PresetNode node = {};
        int currentLevel = 0;
        int originalOrder = 0;
        int learnedLayer = 0;
        bool hasLiveMatch = false;
        bool hasCoordinate = false;
        int state42 = 0;
        double distanceSq = 0.0;
    };

    std::vector<ApplyTalentCandidate> BuildOrderedApplyCandidates(
        const std::vector<PresetNode>& currentNodes,
        const std::vector<PresetNode>& targetNodes)
    {
        const auto currentMap = PresetNodeMap(currentNodes);
        float centerX = 0.0f;
        float centerY = 0.0f;
        const bool haveCenter = ComputeLiveTreeCenter(centerX, centerY);

        std::vector<ApplyTalentCandidate> candidates;
        int originalOrder = 0;
        for (const PresetNode& targetNode : targetNodes)
        {
            const std::uint8_t targetLevel = NormalizePresetLevel(targetNode.nodeId, targetNode.level);
            if (targetLevel == 0)
            {
                ++originalOrder;
                continue;
            }

            const auto currentIt = currentMap.find(targetNode.nodeId);
            const int currentLevel = currentIt == currentMap.end() ? 0 : static_cast<int>(currentIt->second);
            if (currentLevel >= static_cast<int>(targetLevel))
            {
                ++originalOrder;
                continue;
            }

            ApplyTalentCandidate candidate = {};
            candidate.node = { targetNode.nodeId, targetLevel };
            candidate.currentLevel = currentLevel;
            candidate.originalOrder = originalOrder++;
            candidate.learnedLayer = LearnedApplyLayer(targetNode.nodeId);

            LiveNodeMatch match = {};
            if (FindLiveNodeById(targetNode.nodeId, match, nullptr))
            {
                candidate.hasLiveMatch = true;
                candidate.state42 = static_cast<int>(match.state42);
                candidate.hasCoordinate = haveCenter &&
                    IsUsableNodeCoordinate(match.uiX) &&
                    IsUsableNodeCoordinate(match.uiY);
                if (candidate.hasCoordinate)
                {
                    const double dx = static_cast<double>(match.uiX - centerX);
                    const double dy = static_cast<double>(match.uiY - centerY);
                    candidate.distanceSq = dx * dx + dy * dy;
                }
            }

            candidates.push_back(candidate);
        }

        std::stable_sort(candidates.begin(), candidates.end(),
            [](const ApplyTalentCandidate& left, const ApplyTalentCandidate& right)
            {
                if (left.hasCoordinate != right.hasCoordinate)
                    return left.hasCoordinate;

                if (left.hasCoordinate && std::fabs(left.distanceSq - right.distanceSq) > 0.001)
                    return left.distanceSq < right.distanceSq;

                if (left.learnedLayer != right.learnedLayer)
                    return left.learnedLayer < right.learnedLayer;

                return left.originalOrder < right.originalOrder;
            });

        return candidates;
    }

    std::vector<QueuedTalentAction> BuildApplyTalentActions(
        const std::vector<PresetNode>& currentNodes,
        const std::vector<PresetNode>& targetNodes)
    {
        std::vector<QueuedTalentAction> actions;
        const std::vector<ApplyTalentCandidate> candidates = BuildOrderedApplyCandidates(currentNodes, targetNodes);

        int maxTargetLevel = 0;
        for (const ApplyTalentCandidate& candidate : candidates)
        {
            const int targetLevel = static_cast<int>(NormalizePresetLevel(candidate.node.nodeId, candidate.node.level));
            maxTargetLevel = (std::max)(maxTargetLevel, targetLevel);
        }

        // Multi-point talents must be interleaved. The game can accept a first
        // point but ignore later back-to-back messages for the same 3-point node.
        for (int desiredLevel = 1; desiredLevel <= maxTargetLevel; ++desiredLevel)
        {
            for (const ApplyTalentCandidate& candidate : candidates)
            {
                const int targetLevel = static_cast<int>(NormalizePresetLevel(candidate.node.nodeId, candidate.node.level));
                if (candidate.currentLevel < desiredLevel && targetLevel >= desiredLevel)
                    actions.push_back({ candidate.node.nodeId, false, 0, desiredLevel });
            }
        }

        if (!actions.empty())
        {
            std::ostringstream oss;
            oss << "[TalentPresets] apply action rounds actions=" << actions.size();
            for (const ApplyTalentCandidate& candidate : candidates)
            {
                const int targetLevel = static_cast<int>(NormalizePresetLevel(candidate.node.nodeId, candidate.node.level));
                if (targetLevel <= candidate.currentLevel)
                    continue;
                oss << " " << candidate.node.nodeId
                    << ":" << candidate.currentLevel
                    << "->" << targetLevel
                    << "@D" << (candidate.hasCoordinate ? static_cast<int>(candidate.distanceSq) : -1)
                    << "@L" << candidate.learnedLayer;
            }
            Log(oss.str());
        }

        return actions;
    }

    bool TalentMapsMatch(const std::vector<PresetNode>& currentNodes, const std::vector<PresetNode>& targetNodes)
    {
        return PresetNodeMap(currentNodes) == PresetNodeMap(targetNodes);
    }

    void ApplyPresetSlot(int slot)
    {
        if (slot < 0 || slot >= PRESET_COUNT)
            return;

        SavePresetNamesFromUi();
        SavePresetsToDisk();

        TalentPresetSlot preset = {};
        {
            std::lock_guard<std::mutex> lock(g_presetsMutex);
            preset = g_presets[slot];
        }

        if (preset.nodes.empty())
        {
            SetPresetStatus(Text().emptyPreset);
            return;
        }

        std::vector<PresetNode> currentNodes;
        std::string error;
        if (!CaptureCurrentTalentNodes(currentNodes, error))
        {
            SetPresetStatus(error);
            return;
        }

        if (TalentMapsMatch(currentNodes, preset.nodes))
        {
            SetPresetStatus(preset.name + Text().alreadyMatches);
            return;
        }

        const std::vector<QueuedTalentAction> nodeClearActions = BuildClearTalentActions(currentNodes);
        const bool needsClear = !nodeClearActions.empty();
        std::vector<QueuedTalentAction> actions = needsClear
            ? BuildBulkClearTalentActions()
            : BuildApplyTalentActions(currentNodes, preset.nodes);

        {
            std::lock_guard<std::mutex> lock(g_presetJobMutex);
            g_presetJob = {};
            g_presetJob.mode = PresetJobMode::ApplyPreset;
            g_presetJob.phase = needsClear ? PresetJobPhase::Clearing : PresetJobPhase::Applying;
            g_presetJob.slot = slot;
            g_presetJob.name = preset.name;
            g_presetJob.bulkResetAttempted = needsClear;
            g_presetJob.targetNodes = preset.nodes;
        }

        QueuePresetActionsForJob(actions);

        std::ostringstream oss;
        oss << Text().preparing << preset.name << ": "
            << (needsClear ? Text().clearingTalents : Text().applyingTalents)
            << " (" << actions.size() << Text().changes << Text().keepSkillsOpen;
        SetPresetStatus(oss.str());
    }

    void ClearAllAssignedTalents()
    {
        std::vector<PresetNode> currentNodes;
        std::string error;
        if (!CaptureCurrentTalentNodes(currentNodes, error))
        {
            SetPresetStatus(error);
            return;
        }

        const std::vector<QueuedTalentAction> nodeClearActions = BuildClearTalentActions(currentNodes);
        if (nodeClearActions.empty())
        {
            ClearPresetJob();
            SetPresetStatus(Text().noAssignedTalents);
            return;
        }

        std::vector<QueuedTalentAction> actions = BuildBulkClearTalentActions();

        {
            std::lock_guard<std::mutex> lock(g_presetJobMutex);
            g_presetJob = {};
            g_presetJob.mode = PresetJobMode::ClearOnly;
            g_presetJob.phase = PresetJobPhase::Clearing;
            g_presetJob.name = Text().clearTalents;
            g_presetJob.bulkResetAttempted = true;
        }

        QueuePresetActionsForJob(actions);

        std::ostringstream oss;
        oss << Text().clearStart << actions.size()
            << Text().changes << Text().keepSkillsOpen;
        SetPresetStatus(oss.str());
    }

    void ProcessPresetJob(DWORD now)
    {
        if (PresetQueueSize() != 0)
            return;

        PresetJobState job = {};
        {
            std::lock_guard<std::mutex> lock(g_presetJobMutex);
            if (g_presetJob.mode == PresetJobMode::None)
                return;

            if (g_presetJob.waitUntilTick == 0)
            {
                const DWORD settleMs = (g_presetJob.phase == PresetJobPhase::Clearing &&
                    g_presetJob.bulkResetAttempted &&
                    g_presetJob.clearPass == 0)
                    ? PRESET_BULK_CLEAR_SETTLE_MS
                    : PRESET_PHASE_SETTLE_MS;
                g_presetJob.waitUntilTick = now + settleMs;
                return;
            }

            if (now < g_presetJob.waitUntilTick)
                return;

            g_presetJob.waitUntilTick = 0;
            job = g_presetJob;
        }

        std::vector<PresetNode> currentNodes;
        std::string error;
        if (!CaptureCurrentTalentNodes(currentNodes, error))
        {
            SetPresetJobWait(now + PRESET_PHASE_SETTLE_MS);
            SetPresetStatus(error);
            return;
        }

        if (job.phase == PresetJobPhase::Clearing)
        {
            std::vector<QueuedTalentAction> clearActions = BuildClearTalentActions(currentNodes);
            if (!clearActions.empty())
            {
                int pass = 0;
                bool exceededPassLimit = false;
                {
                    std::lock_guard<std::mutex> lock(g_presetJobMutex);
                    if (g_presetJob.clearPass >= PRESET_MAX_JOB_PASSES)
                    {
                        exceededPassLimit = true;
                    }
                    else
                    {
                        pass = ++g_presetJob.clearPass;
                    }
                }

                if (exceededPassLimit)
                {
                    ClearPresetJob();
                    SetPresetStatus(Text().unableClear);
                    return;
                }

                QueuePresetActionsForJob(clearActions);

                std::ostringstream oss;
                oss << Text().clearStart << Text().pass << pass << ": "
                    << clearActions.size() << Text().pendingChanges;
                SetPresetStatus(oss.str());
                return;
            }

            if (job.mode == PresetJobMode::ClearOnly)
            {
                ClearPresetJob();
                SetPresetStatus(Text().treeClean);
                return;
            }

            {
                std::lock_guard<std::mutex> lock(g_presetJobMutex);
                if (g_presetJob.mode != PresetJobMode::None)
                {
                    g_presetJob.phase = PresetJobPhase::Applying;
                    g_presetJob.waitUntilTick = 0;
                }
                job = g_presetJob;
            }
        }

        if (job.mode != PresetJobMode::ApplyPreset || job.phase != PresetJobPhase::Applying)
            return;

        std::vector<QueuedTalentAction> applyActions = BuildApplyTalentActions(currentNodes, job.targetNodes);
        if (applyActions.empty())
        {
            if (TalentMapsMatch(currentNodes, job.targetNodes))
            {
                const std::string name = job.name;
                ClearPresetJob();
                SetPresetStatus(name + Text().appliedVerified);
                return;
            }

            ClearPresetJob();
            SetPresetStatus(Text().mismatchNoSafe);
            return;
        }

        int pass = 0;
        bool exceededPassLimit = false;
        {
            std::lock_guard<std::mutex> lock(g_presetJobMutex);
            if (g_presetJob.applyPass >= PRESET_MAX_JOB_PASSES)
            {
                exceededPassLimit = true;
            }
            else
            {
                pass = ++g_presetJob.applyPass;
            }
        }

        if (exceededPassLimit)
        {
            ClearPresetJob();
            SetPresetStatus(Text().unableApply);
            return;
        }

        QueuePresetActionsForJob(applyActions);

        std::ostringstream oss;
        oss << Text().applying << job.name << Text().pass << pass << ": "
            << applyActions.size() << Text().pendingChanges;
        SetPresetStatus(oss.str());
    }

    void RefreshPresetUiControls()
    {
        std::array<TalentPresetSlot, PRESET_COUNT> snapshot = {};
        {
            std::lock_guard<std::mutex> lock(g_presetsMutex);
            snapshot = g_presets;
        }

        for (int index = 0; index < PRESET_COUNT; ++index)
        {
            if (g_presetNameEdits[index] != nullptr)
                SetWindowTextW(g_presetNameEdits[index], Utf8ToWide(snapshot[index].name).c_str());
        }

        if (g_presetStatusLabel != nullptr)
            SetWindowTextW(g_presetStatusLabel, GetPresetStatusWide().c_str());
    }

    BOOL CALLBACK FindGameWindowProc(HWND hwnd, LPARAM lParam)
    {
        if (!IsWindowVisible(hwnd) || hwnd == g_presetWindow)
            return TRUE;

        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);
        if (processId != GetCurrentProcessId())
            return TRUE;

        wchar_t className[128] = {};
        GetClassNameW(hwnd, className, static_cast<int>(sizeof(className) / sizeof(className[0])));
        if (std::wcscmp(className, L"TalentPresetsPanelWindow") == 0)
            return TRUE;

        wchar_t title[256] = {};
        GetWindowTextW(hwnd, title, static_cast<int>(sizeof(title) / sizeof(title[0])));
        if (title[0] == L'\0')
            return TRUE;

        *reinterpret_cast<HWND*>(lParam) = hwnd;
        return FALSE;
    }

    HWND FindGameWindow()
    {
        HWND result = nullptr;
        EnumWindows(FindGameWindowProc, reinterpret_cast<LPARAM>(&result));
        return result;
    }

    bool IsPresetWindowOrChild(HWND hwnd)
    {
        HWND presetWindow = g_presetWindow;
        if (hwnd == nullptr || presetWindow == nullptr)
            return false;

        return hwnd == presetWindow || IsChild(presetWindow, hwnd);
    }

    bool IsGameWindowOrChild(HWND hwnd)
    {
        HWND gameWindow = FindGameWindow();
        if (hwnd == nullptr || gameWindow == nullptr)
            return false;

        return hwnd == gameWindow || IsChild(gameWindow, hwnd);
    }

    bool IsGameForeground()
    {
        return IsGameWindowOrChild(GetForegroundWindow());
    }

    bool IsGameInteractionForeground()
    {
        HWND foreground = GetForegroundWindow();
        return IsGameWindowOrChild(foreground) || IsPresetWindowOrChild(foreground);
    }

    bool IsPresetNameEdit(HWND hwnd)
    {
        if (hwnd == nullptr)
            return false;

        for (HWND edit : g_presetNameEdits)
        {
            if (hwnd == edit)
                return true;
        }

        return false;
    }

    bool IsCursorOverPresetNameEdit()
    {
        POINT point = {};
        if (!GetCursorPos(&point))
            return false;

        HWND hit = WindowFromPoint(point);
        if (hit == nullptr)
            return false;

        if (IsPresetNameEdit(hit))
            return true;

        for (HWND edit : g_presetNameEdits)
        {
            if (edit != nullptr && IsChild(edit, hit))
                return true;
        }

        return false;
    }

    void ReturnFocusToGameWindow()
    {
        HWND gameWindow = FindGameWindow();
        if (gameWindow != nullptr)
            SetForegroundWindow(gameWindow);
    }

    bool GetPresetTargetRect(RECT& targetRect)
    {
        targetRect = {};
        HWND gameWindow = FindGameWindow();
        if (gameWindow != nullptr)
        {
            RECT client = {};
            if (GetClientRect(gameWindow, &client))
            {
                POINT topLeft = { 0, 0 };
                ClientToScreen(gameWindow, &topLeft);
                targetRect.left = topLeft.x;
                targetRect.top = topLeft.y;
                targetRect.right = topLeft.x + (client.right - client.left);
                targetRect.bottom = topLeft.y + (client.bottom - client.top);
            }
        }

        if (targetRect.right <= targetRect.left || targetRect.bottom <= targetRect.top)
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &targetRect, 0);

        return targetRect.right > targetRect.left && targetRect.bottom > targetRect.top;
    }

    POINT ClampPresetWindowPoint(POINT point, const RECT& targetRect)
    {
        const LONG maxX = (std::max)(targetRect.left, targetRect.right - PRESET_WINDOW_WIDTH);
        const LONG maxY = (std::max)(targetRect.top, targetRect.bottom - PRESET_WINDOW_HEIGHT);
        point.x = (std::min)((std::max)(point.x, targetRect.left), maxX);
        point.y = (std::min)((std::max)(point.y, targetRect.top), maxY);
        return point;
    }

    void SaveCurrentPresetWindowPosition()
    {
        HWND window = g_presetWindow;
        if (window == nullptr || g_positioningPresetWindow)
            return;

        RECT windowRect = {};
        RECT targetRect = {};
        if (!GetWindowRect(window, &windowRect) || !GetPresetTargetRect(targetRect))
            return;

        g_presetWindowOffsetX = static_cast<int>((std::max)(0L, windowRect.left - targetRect.left));
        g_presetWindowOffsetY = static_cast<int>((std::max)(0L, windowRect.top - targetRect.top));
        g_presetWindowPositionLoaded = true;
        SavePresetUiPosition();
    }

    void UpdatePresetWindowShape(HWND window)
    {
        HRGN region = CreateRoundRectRgn(
            0,
            0,
            PRESET_WINDOW_WIDTH + 1,
            PRESET_WINDOW_HEIGHT + 1,
            PRESET_CORNER_RADIUS,
            PRESET_CORNER_RADIUS);
        if (region != nullptr)
            SetWindowRgn(window, region, TRUE);
    }

    void PositionPresetWindow()
    {
        HWND window = g_presetWindow;
        if (window == nullptr)
            return;

        RECT targetRect = {};
        if (!GetPresetTargetRect(targetRect))
            return;

        const int targetWidth = targetRect.right - targetRect.left;
        POINT point = {};
        if (g_presetWindowPositionLoaded)
        {
            point.x = targetRect.left + g_presetWindowOffsetX;
            point.y = targetRect.top + g_presetWindowOffsetY;
        }
        else
        {
            point.x = targetRect.left + ((std::max)(0, targetWidth - PRESET_WINDOW_WIDTH) / 2);
            point.y = targetRect.top + 100;
        }

        point = ClampPresetWindowPoint(point, targetRect);
        g_positioningPresetWindow = true;
        SetWindowPos(window, HWND_TOPMOST, point.x, point.y, PRESET_WINDOW_WIDTH, PRESET_WINDOW_HEIGHT, SWP_NOACTIVATE);
        g_positioningPresetWindow = false;
        UpdatePresetWindowShape(window);
    }

    void DrawPresetPanelBackground(HWND hwnd, HDC dc)
    {
        RECT rect = {};
        GetClientRect(hwnd, &rect);

        HBRUSH background = CreateSolidBrush(RGB(15, 12, 20));
        HBRUSH header = CreateSolidBrush(RGB(42, 28, 51));
        HBRUSH glow = CreateSolidBrush(RGB(96, 50, 118));
        HPEN border = CreatePen(PS_SOLID, 2, RGB(211, 167, 74));
        HPEN softLine = CreatePen(PS_SOLID, 1, RGB(112, 78, 134));

        HGDIOBJ oldBrush = SelectObject(dc, background);
        HGDIOBJ oldPen = SelectObject(dc, border);
        RoundRect(dc, rect.left, rect.top, rect.right - 1, rect.bottom - 1, PRESET_CORNER_RADIUS, PRESET_CORNER_RADIUS);

        RECT headerRect = { 2, 2, rect.right - 2, PRESET_HEADER_HEIGHT };
        FillRect(dc, &headerRect, header);

        RECT glowRect = { 14, PRESET_HEADER_HEIGHT - 3, rect.right - 14, PRESET_HEADER_HEIGHT + 2 };
        FillRect(dc, &glowRect, glow);

        SelectObject(dc, softLine);
        MoveToEx(dc, 22, PRESET_HEADER_HEIGHT + 8, nullptr);
        LineTo(dc, rect.right - 22, PRESET_HEADER_HEIGHT + 8);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(248, 229, 174));
        HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, g_presetTitleFont));
        RECT titleRect = { 28, 18, rect.right - 34, 48 };
        const std::wstring title = Utf8ToWide(Text().title);
        DrawTextW(dc, title.c_str(), -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        SetTextColor(dc, RGB(222, 208, 183));
        SelectObject(dc, g_presetTextFont);
        RECT subRect = { 30, 52, rect.right - 34, 76 };
        const std::wstring subtitle = Utf8ToWide(Text().subtitle);
        DrawTextW(dc, subtitle.c_str(), -1, &subRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        if (oldFont != nullptr)
            SelectObject(dc, oldFont);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(background);
        DeleteObject(header);
        DeleteObject(glow);
        DeleteObject(border);
        DeleteObject(softLine);
    }

    void DrawPresetButton(const DRAWITEMSTRUCT* item)
    {
        if (item == nullptr)
            return;

        HDC dc = item->hDC;
        RECT rect = item->rcItem;
        const bool pressed = (item->itemState & ODS_SELECTED) != 0;
        const bool clearButton = item->CtlID == PRESET_BUTTON_CLEAR_ALL;

        const COLORREF fill = clearButton
            ? (pressed ? RGB(117, 45, 45) : RGB(92, 33, 40))
            : (pressed ? RGB(91, 63, 124) : RGB(58, 42, 78));
        const COLORREF outline = clearButton ? RGB(231, 137, 92) : RGB(201, 164, 83);
        const COLORREF text = RGB(250, 238, 205);

        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, outline);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, 10, 10);

        wchar_t label[128] = {};
        GetWindowTextW(item->hwndItem, label, static_cast<int>(sizeof(label) / sizeof(label[0])));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, text);
        HFONT oldFont = reinterpret_cast<HFONT>(SelectObject(dc, g_presetTextFont));
        DrawTextW(dc, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if (oldFont != nullptr)
            SelectObject(dc, oldFont);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);
    }

    void ProcessPresetActionQueueTick(DWORD now);

    LRESULT CALLBACK PresetWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        static HBRUSH backgroundBrush = CreateSolidBrush(RGB(15, 12, 20));
        static HBRUSH editBrush = CreateSolidBrush(RGB(39, 30, 48));

        switch (message)
        {
        case WM_CREATE:
        {
            g_presetWindow = hwnd;
            UpdatePresetWindowShape(hwnd);
            SetLayeredWindowAttributes(hwnd, 0, 244, LWA_ALPHA);

            g_presetTitleFont = CreateFontW(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_ROMAN, L"Palatino Linotype");
            g_presetTextFont = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Semibold");

            for (int index = 0; index < PRESET_COUNT; ++index)
            {
                const int y = 104 + index * 52;
                std::wostringstream label;
                label << L"#" << (index + 1);
                HWND slotLabel = CreateWindowExW(0, L"STATIC", label.str().c_str(), WS_CHILD | WS_VISIBLE,
                    30, y + 7, 38, 24, hwnd, nullptr, nullptr, nullptr);
                SendMessageW(slotLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_presetTextFont), TRUE);

                g_presetNameEdits[index] = CreateWindowExW(0, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                    72, y, 270, 34, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PRESET_EDIT_NAME_BASE + index)), nullptr, nullptr);
                SendMessageW(g_presetNameEdits[index], WM_SETFONT, reinterpret_cast<WPARAM>(g_presetTextFont), TRUE);

                g_presetApplyButtons[index] = CreateWindowExW(0, L"BUTTON", Utf8ToWide(Text().apply).c_str(),
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                    360, y, 130, 34, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PRESET_BUTTON_APPLY_BASE + index)), nullptr, nullptr);
                SendMessageW(g_presetApplyButtons[index], WM_SETFONT, reinterpret_cast<WPARAM>(g_presetTextFont), TRUE);

                g_presetSaveButtons[index] = CreateWindowExW(0, L"BUTTON", Utf8ToWide(Text().saveCurrent).c_str(),
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                    508, y, 150, 34, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PRESET_BUTTON_SAVE_BASE + index)), nullptr, nullptr);
                SendMessageW(g_presetSaveButtons[index], WM_SETFONT, reinterpret_cast<WPARAM>(g_presetTextFont), TRUE);
            }

            g_presetClearAllButton = CreateWindowExW(0, L"BUTTON", Utf8ToWide(Text().clearTalents).c_str(),
                WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                676, 104, 138, 34, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(PRESET_BUTTON_CLEAR_ALL)), nullptr, nullptr);
            SendMessageW(g_presetClearAllButton, WM_SETFONT, reinterpret_cast<WPARAM>(g_presetTextFont), TRUE);

            g_presetStatusLabel = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                26, 374, 780, 32, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(g_presetStatusLabel, WM_SETFONT, reinterpret_cast<WPARAM>(g_presetTextFont), TRUE);

            SetTimer(hwnd, PRESET_UI_TIMER, PRESET_ACTION_TIMER_MS, nullptr);
            RefreshPresetUiControls();
            return 0;
        }

        case WM_TIMER:
            if (wParam == PRESET_UI_TIMER)
            {
                const bool gameInteractionForeground = IsGameInteractionForeground();
                if (gameInteractionForeground)
                    ProcessPresetActionQueueTick(GetTickCount());

                if (InterlockedCompareExchange(&g_presetUiVisible, 0, 0) == 1)
                {
                    if (gameInteractionForeground)
                    {
                        if (!IsWindowVisible(hwnd))
                        {
                            PositionPresetWindow();
                            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                        }
                        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                    }
                    else if (IsWindowVisible(hwnd))
                    {
                        ShowWindow(hwnd, SW_HIDE);
                    }
                }
                if (g_presetStatusLabel != nullptr)
                    SetWindowTextW(g_presetStatusLabel, GetPresetStatusWide().c_str());
            }
            return 0;

        case WM_TALENT_PRESETS_SHOW:
            if (wParam != 0)
            {
                RefreshPresetUiControls();
                PositionPresetWindow();
                ShowWindow(hwnd, SW_SHOWNOACTIVATE);
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            else
            {
                SavePresetNamesFromUi();
                SavePresetsToDisk();
                SaveCurrentPresetWindowPosition();
                ShowWindow(hwnd, SW_HIDE);
                ReturnFocusToGameWindow();
            }
            return 0;

        case WM_TALENT_PRESETS_REFRESH:
            if (g_presetStatusLabel != nullptr)
                SetWindowTextW(g_presetStatusLabel, GetPresetStatusWide().c_str());
            return 0;

        case WM_MOUSEACTIVATE:
            return IsCursorOverPresetNameEdit() ? MA_ACTIVATE : MA_NOACTIVATE;

        case WM_NCHITTEST:
        {
            POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hwnd, &point);
            if (point.y >= 0 && point.y < PRESET_HEADER_HEIGHT)
                return HTCAPTION;
            return HTCLIENT;
        }

        case WM_EXITSIZEMOVE:
            SaveCurrentPresetWindowPosition();
            return 0;

        case WM_DRAWITEM:
            DrawPresetButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
            return TRUE;

        case WM_COMMAND:
        {
            const int commandId = LOWORD(wParam);
            const int notification = HIWORD(wParam);

            if (commandId >= PRESET_EDIT_NAME_BASE && commandId < PRESET_EDIT_NAME_BASE + PRESET_COUNT)
            {
                if (notification == EN_SETFOCUS)
                {
                    SendMessageW(reinterpret_cast<HWND>(lParam), EM_SETSEL, 0, -1);
                    SetPresetStatus(Text().editName);
                    return 0;
                }

                if (notification == EN_KILLFOCUS)
                {
                    SavePresetNamesFromUi();
                    SavePresetsToDisk();
                    return 0;
                }
            }

            if (commandId == PRESET_BUTTON_CLEAR_ALL)
            {
                ClearAllAssignedTalents();
                ReturnFocusToGameWindow();
                return 0;
            }

            if (commandId >= PRESET_BUTTON_APPLY_BASE && commandId < PRESET_BUTTON_APPLY_BASE + PRESET_COUNT)
            {
                ApplyPresetSlot(commandId - PRESET_BUTTON_APPLY_BASE);
                ReturnFocusToGameWindow();
                return 0;
            }

            if (commandId >= PRESET_BUTTON_SAVE_BASE && commandId < PRESET_BUTTON_SAVE_BASE + PRESET_COUNT)
            {
                SaveCurrentToPreset(commandId - PRESET_BUTTON_SAVE_BASE);
                RefreshPresetUiControls();
                ReturnFocusToGameWindow();
                return 0;
            }

            if (commandId >= PRESET_BUTTON_ADD_HOVER_BASE && commandId < PRESET_BUTTON_ADD_HOVER_BASE + PRESET_COUNT)
            {
                AddHoveredNodeToPreset(commandId - PRESET_BUTTON_ADD_HOVER_BASE);
                RefreshPresetUiControls();
                ReturnFocusToGameWindow();
                return 0;
            }

            if (commandId >= PRESET_BUTTON_REMOVE_HOVER_BASE && commandId < PRESET_BUTTON_REMOVE_HOVER_BASE + PRESET_COUNT)
            {
                RemoveHoveredNodeFromPreset(commandId - PRESET_BUTTON_REMOVE_HOVER_BASE);
                RefreshPresetUiControls();
                ReturnFocusToGameWindow();
                return 0;
            }
            break;
        }

        case WM_CTLCOLORSTATIC:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, RGB(238, 231, 216));
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(backgroundBrush);
        }

        case WM_CTLCOLOREDIT:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, RGB(248, 238, 214));
            SetBkColor(dc, RGB(39, 30, 48));
            return reinterpret_cast<LRESULT>(editBrush);
        }

        case WM_PAINT:
        {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(hwnd, &paint);
            DrawPresetPanelBackground(hwnd, dc);
            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_CLOSE:
            SavePresetNamesFromUi();
            SavePresetsToDisk();
            SaveCurrentPresetWindowPosition();
            InterlockedExchange(&g_presetUiVisible, 0);
            ShowWindow(hwnd, SW_HIDE);
            ReturnFocusToGameWindow();
            return 0;

        case WM_TALENT_PRESETS_SHUTDOWN:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, PRESET_UI_TIMER);
            if (g_presetTitleFont != nullptr)
            {
                DeleteObject(g_presetTitleFont);
                g_presetTitleFont = nullptr;
            }
            if (g_presetTextFont != nullptr)
            {
                DeleteObject(g_presetTextFont);
                g_presetTextFont = nullptr;
            }
            g_presetWindow = nullptr;
            g_presetClearAllButton = nullptr;
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    unsigned __stdcall PresetUiThreadProc(void*)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = PresetWindowProc;
        wc.hInstance = g_moduleHandle;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"TalentPresetsPanelWindow";
        RegisterClassExW(&wc);

        HWND gameWindow = FindGameWindow();
        HWND hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            wc.lpszClassName,
            Utf8ToWide(Text().title).c_str(),
            WS_POPUP,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            PRESET_WINDOW_WIDTH,
            PRESET_WINDOW_HEIGHT,
            gameWindow,
            nullptr,
            g_moduleHandle,
            nullptr);

        if (hwnd == nullptr)
        {
            SetPresetStatus(Text().createUiFailed);
            return 0;
        }

        MSG msg = {};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        return 0;
    }

    bool EnsurePresetUiThread()
    {
        if (g_presetUiThread != nullptr)
            return true;

        unsigned int threadId = 0;
        HANDLE thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, PresetUiThreadProc, nullptr, 0, &threadId));
        if (thread == nullptr)
        {
            SetPresetStatus(Text().startUiFailed);
            return false;
        }

        g_presetUiThread = thread;
        g_presetUiThreadId = threadId;
        return true;
    }

    void ShutdownPresetUiThread()
    {
        HWND window = g_presetWindow;
        if (window != nullptr)
            PostMessageW(window, WM_TALENT_PRESETS_SHUTDOWN, 0, 0);

        if (g_presetUiThreadId != 0)
            PostThreadMessageW(g_presetUiThreadId, WM_QUIT, 0, 0);

        if (g_presetUiThread != nullptr)
        {
            WaitForSingleObject(g_presetUiThread, 750);
            CloseHandle(g_presetUiThread);
            g_presetUiThread = nullptr;
            g_presetUiThreadId = 0;
        }
    }

    void TogglePresetOverlay()
    {
        const LONG wasVisible = InterlockedCompareExchange(&g_presetUiVisible, 0, 0);
        const LONG newVisible = wasVisible == 0 ? 1 : 0;
        if (newVisible != 0 && !IsGameForeground())
        {
            Log("[TalentPresets] F4 ignored because Enshrouded is not the foreground window");
            return;
        }

        if (newVisible == 0 && !IsGameInteractionForeground())
        {
            Log("[TalentPresets] F4 ignored because neither Enshrouded nor the preset panel is foreground");
            return;
        }

        if (!EnsurePresetUiThread())
            return;

        InterlockedExchange(&g_presetUiVisible, newVisible);

        for (int attempt = 0; attempt < 50 && g_presetWindow == nullptr; ++attempt)
            Sleep(10);

        HWND window = g_presetWindow;
        if (window != nullptr)
            PostMessageW(window, WM_TALENT_PRESETS_SHOW, static_cast<WPARAM>(newVisible), 0);
    }

    std::string HexBytes(uintptr_t address, std::size_t count)
    {
        if (address == 0)
            return "<null>";

        std::array<std::uint8_t, 96> bytes = {};
        count = (std::min)(count, bytes.size());
        if (!SafeRead(address, bytes.data(), count))
            return "<unreadable>";

        std::ostringstream oss;
        oss << std::hex;
        for (std::size_t index = 0; index < count; ++index)
        {
            if (index != 0)
                oss << ' ';
            const auto value = static_cast<unsigned int>(bytes[index]);
            if (value < 0x10)
                oss << '0';
            oss << value;
        }
        return oss.str();
    }

    void RememberSample(
        uintptr_t node,
        uintptr_t state,
        std::uint32_t nodeId,
        std::uint8_t local28,
        std::uint8_t state4,
        std::uint16_t cost,
        std::uint8_t state40,
        std::uint8_t state41,
        std::uint8_t state42,
        std::uint8_t state43)
    {
        const LONG currentCount = (std::min)(static_cast<LONG>(g_nodeSampleCount), static_cast<LONG>(MAX_NODE_SAMPLES));
        for (LONG index = 0; index < currentCount; ++index)
        {
            if (g_nodeSamples[index].node == node)
            {
                g_nodeSamples[index].state = state;
                g_nodeSamples[index].nodeId = nodeId;
                g_nodeSamples[index].local28 = local28;
                g_nodeSamples[index].state4 = state4;
                g_nodeSamples[index].cost = cost;
                g_nodeSamples[index].state40 = state40;
                g_nodeSamples[index].state41 = state41;
                g_nodeSamples[index].state42 = state42;
                g_nodeSamples[index].state43 = state43;
                InterlockedIncrement(&g_nodeSamples[index].hits);
                return;
            }
        }

        const LONG slot = InterlockedIncrement(&g_nodeSampleCount) - 1;
        if (slot < 0 || slot >= MAX_NODE_SAMPLES)
        {
            InterlockedExchange(&g_nodeSampleCount, MAX_NODE_SAMPLES);
            return;
        }

        g_nodeSamples[slot].node = node;
        g_nodeSamples[slot].state = state;
        g_nodeSamples[slot].nodeId = nodeId;
        g_nodeSamples[slot].local28 = local28;
        g_nodeSamples[slot].state4 = state4;
        g_nodeSamples[slot].cost = cost;
        g_nodeSamples[slot].state40 = state40;
        g_nodeSamples[slot].state41 = state41;
        g_nodeSamples[slot].state42 = state42;
        g_nodeSamples[slot].state43 = state43;
        g_nodeSamples[slot].hits = 1;
    }

    bool ValidateBytes(uintptr_t address, const std::uint8_t* expected, std::size_t size, const char* label)
    {
        std::vector<std::uint8_t> current(size, 0);
        if (!SafeRead(address, current.data(), current.size()))
        {
            Log(std::string("[TalentPresets] failed to read bytes for ") + label);
            return false;
        }

        if (std::memcmp(current.data(), expected, size) != 0)
        {
            std::ostringstream oss;
            oss << "[TalentPresets] version guard failed for " << label
                << " at RVA " << Hex(address - g_exeBase);
            Log(oss.str());
            return false;
        }

        return true;
    }

    void RememberTalentContext(uintptr_t gameRoot, uintptr_t contextSlot, uintptr_t activePlayer)
    {
        uintptr_t talentState = 0;
        uintptr_t talentQueue = 0;

        if (activePlayer != 0)
            SafeReadValue(activePlayer + TALENT_POINTS_STATE_OFFSET, talentState);

        if (contextSlot != 0)
            SafeReadValue(contextSlot + TALENT_QUEUE_OFFSET, talentQueue);

        InterlockedExchange64(&g_lastGameRoot, static_cast<LONG64>(gameRoot));
        InterlockedExchange64(&g_lastContextSlot, static_cast<LONG64>(contextSlot));
        InterlockedExchange64(&g_lastActivePlayer, static_cast<LONG64>(activePlayer));
        InterlockedExchange64(&g_lastTalentState, static_cast<LONG64>(talentState));
        InterlockedExchange64(&g_lastTalentQueue, static_cast<LONG64>(talentQueue));
        InterlockedExchange(&g_lastContextTick, static_cast<LONG>(GetTickCount()));
    }

    void __fastcall CaptureSkillTreeContext(void* gameRoot, void* contextSlot, void* activePlayer)
    {
        InterlockedExchange(&g_skillTreeRootSeen, 1);
        RememberTalentContext(
            reinterpret_cast<uintptr_t>(gameRoot),
            reinterpret_cast<uintptr_t>(contextSlot),
            reinterpret_cast<uintptr_t>(activePlayer));
    }

    void __fastcall CaptureDrawNodeLink(void* treeRoot)
    {
        RememberTreeUiRoot(reinterpret_cast<uintptr_t>(treeRoot));
    }

    void __fastcall CaptureSkillNode(void* node)
    {
        if (node == nullptr)
            return;

        std::uint32_t nodeId = 0;
        if (!SafeReadValue(reinterpret_cast<uintptr_t>(node), nodeId))
            return;

        std::uint8_t local28 = 0;
        SafeReadValue(reinterpret_cast<uintptr_t>(node) + 0x28, local28);

        uintptr_t state = 0;
        SafeReadValue(reinterpret_cast<uintptr_t>(node) + 0x48, state);

        std::uint8_t state4 = 0xFF;
        std::uint16_t cost = 0xFFFF;
        std::uint8_t state40 = 0xFF;
        std::uint8_t state41 = 0xFF;
        std::uint8_t state42 = 0xFF;
        std::uint8_t state43 = 0xFF;
        if (state != 0)
        {
            SafeReadValue(state + 0x04, state4);
            SafeReadValue(state + 0x1C, cost);
            SafeReadValue(state + 0x40, state40);
            SafeReadValue(state + 0x41, state41);
            SafeReadValue(state + 0x42, state42);
            SafeReadValue(state + 0x43, state43);
        }

        InterlockedExchange(&g_lastNodeIdBits, static_cast<LONG>(nodeId));
        InterlockedExchange(&g_lastNodeLocal28, static_cast<LONG>(local28));
        InterlockedExchange(&g_lastNodeState4, static_cast<LONG>(state4));
        InterlockedExchange(&g_lastNodeCost, static_cast<LONG>(cost));
        InterlockedExchange(&g_lastNodeState40, static_cast<LONG>(state40));
        InterlockedExchange(&g_lastNodeState41, static_cast<LONG>(state41));
        InterlockedExchange(&g_lastNodeState42, static_cast<LONG>(state42));
        InterlockedExchange(&g_lastNodeState43, static_cast<LONG>(state43));
        InterlockedExchange64(&g_lastNodePtr, static_cast<LONG64>(reinterpret_cast<uintptr_t>(node)));
        InterlockedExchange64(&g_lastNodeStatePtr, static_cast<LONG64>(state));
        InterlockedExchange(&g_lastNodeTick, static_cast<LONG>(GetTickCount()));
        InterlockedIncrement(&g_nodeHitCount);
        RememberSample(reinterpret_cast<uintptr_t>(node), state, nodeId, local28, state4, cost, state40, state41, state42, state43);
    }

    bool ResolveTalentContext(uintptr_t& gameRoot, uintptr_t& contextSlot, uintptr_t& activePlayer, uintptr_t& talentState, uintptr_t& talentQueue)
    {
        gameRoot = static_cast<uintptr_t>(g_lastGameRoot);
        contextSlot = static_cast<uintptr_t>(g_lastContextSlot);
        activePlayer = static_cast<uintptr_t>(g_lastActivePlayer);
        talentState = static_cast<uintptr_t>(g_lastTalentState);
        talentQueue = static_cast<uintptr_t>(g_lastTalentQueue);

        if (gameRoot != 0 && contextSlot != 0 && talentQueue != 0)
            return true;

        gameRoot = 0;
        contextSlot = 0;
        activePlayer = 0;
        talentState = 0;
        talentQueue = 0;

        const uintptr_t globalAddress = g_exeBase + RVA_GAME_CONTEXT_GLOBAL;
        if (!SafeReadValue(globalAddress, gameRoot) || gameRoot == 0)
            return false;

        if (!SafeReadValue(gameRoot + GAME_CONTEXT_SLOT_OFFSET, contextSlot) || contextSlot == 0)
            return false;

        SafeReadValue(contextSlot, activePlayer);

        if (activePlayer != 0)
            SafeReadValue(activePlayer + TALENT_POINTS_STATE_OFFSET, talentState);

        if (!SafeReadValue(contextSlot + TALENT_QUEUE_OFFSET, talentQueue) || talentQueue == 0)
            return false;

        RememberTalentContext(gameRoot, contextSlot, activePlayer);
        return true;
    }

    bool ResolveSkillPointBucket(uintptr_t& activePlayer, uintptr_t& skillBucket, std::uint32_t& availablePoints)
    {
        uintptr_t gameRoot = 0;
        uintptr_t contextSlot = 0;
        uintptr_t talentState = 0;
        uintptr_t talentQueue = 0;
        if (!ResolveTalentContext(gameRoot, contextSlot, activePlayer, talentState, talentQueue))
            return false;

        if (activePlayer == 0)
            return false;

        if (!SafeReadValue(activePlayer + 0x28, skillBucket) || skillBucket == 0)
            return false;

        if (!SafeReadValue(skillBucket + 0x820, availablePoints))
            return false;

        return true;
    }

    bool ReadTalentQueueMetrics(uintptr_t talentQueue, uintptr_t& base, std::uint64_t& count, std::uint64_t& capacity)
    {
        if (talentQueue == 0)
            return false;

        if (!SafeReadValue(talentQueue, base))
            return false;
        if (!SafeReadValue(talentQueue + 0x08, count))
            return false;
        if (!SafeReadValue(talentQueue + 0x10, capacity))
            return false;

        return true;
    }

    std::uint8_t* TryAllocateTalentMessage(uintptr_t talentQueue)
    {
        auto allocator = reinterpret_cast<UiMessageAllocatorFn>(g_exeBase + RVA_UI_MESSAGE_ALLOCATOR);
        __try
        {
            return reinterpret_cast<std::uint8_t*>(allocator(reinterpret_cast<void*>(talentQueue)));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return nullptr;
        }
    }

    bool EnqueueTalentMessage(std::uint32_t nodeId, std::uint8_t opcode, uintptr_t* outQueue = nullptr, bool resetSkills = false)
    {
        uintptr_t gameRoot = 0;
        uintptr_t contextSlot = 0;
        uintptr_t activePlayer = 0;
        uintptr_t talentState = 0;
        uintptr_t talentQueue = 0;
        if (!ResolveTalentContext(gameRoot, contextSlot, activePlayer, talentState, talentQueue))
        {
            std::ostringstream oss;
            oss << "[TalentPresets] talent context not ready"
                << " gameRoot=" << Hex(gameRoot)
                << " contextSlot=" << Hex(contextSlot)
                << " activePlayer=" << Hex(activePlayer)
                << " talentState=" << Hex(talentState)
                << " queue=" << Hex(talentQueue);
            Log(oss.str());
            return false;
        }

        std::uint8_t* message = TryAllocateTalentMessage(talentQueue);
        if (message == nullptr)
        {
            Log("[TalentPresets] ui allocator returned null or raised an exception");
            return false;
        }

        const uintptr_t messagePtr = reinterpret_cast<uintptr_t>(message);
        if (!SafeWriteValue(messagePtr, opcode) ||
            !SafeWriteValue(messagePtr + TALENT_MESSAGE_NODE_ID_OFFSET, static_cast<std::int32_t>(nodeId)))
        {
            Log("[TalentPresets] failed to write queued talent message");
            return false;
        }

        if (opcode == TALENT_MESSAGE_LEARN)
        {
            const std::uint8_t resetSkillsByte = resetSkills ? 1 : 0;
            if (!SafeWriteValue(messagePtr + TALENT_MESSAGE_RESET_SKILLS_OFFSET, resetSkillsByte))
            {
                Log("[TalentPresets] failed to write resetSkills flag");
                return false;
            }
        }

        if (outQueue != nullptr)
            *outQueue = talentQueue;

        return true;
    }

    bool EnqueueResetAllTalentsMessage()
    {
        uintptr_t queueBeforePtr = 0;
        uintptr_t queueAfterPtr = 0;
        uintptr_t queueBaseBefore = 0;
        uintptr_t queueBaseAfter = 0;
        std::uint64_t queueCountBefore = 0;
        std::uint64_t queueCapacityBefore = 0;
        std::uint64_t queueCountAfter = 0;
        std::uint64_t queueCapacityAfter = 0;

        uintptr_t gameRoot = 0;
        uintptr_t contextSlot = 0;
        uintptr_t activePlayer = 0;
        uintptr_t talentState = 0;
        ResolveTalentContext(gameRoot, contextSlot, activePlayer, talentState, queueBeforePtr);
        ReadTalentQueueMetrics(queueBeforePtr, queueBaseBefore, queueCountBefore, queueCapacityBefore);

        uintptr_t queuedPtr = 0;
        const bool queued = EnqueueTalentMessage(0, TALENT_MESSAGE_LEARN, &queuedPtr, true);

        ResolveTalentContext(gameRoot, contextSlot, activePlayer, talentState, queueAfterPtr);
        ReadTalentQueueMetrics(queueAfterPtr, queueBaseAfter, queueCountAfter, queueCapacityAfter);

        std::ostringstream post;
        post << "[TalentPresets] reset-all talents message"
            << " queued=" << (queued ? 1 : 0)
            << " queueBefore=" << Hex(queueBeforePtr)
            << " countBefore=" << queueCountBefore
            << " queueAfter=" << Hex(queueAfterPtr)
            << " countAfter=" << queueCountAfter
            << " outQueue=" << Hex(queuedPtr);

        if (queued && queueAfterPtr != 0 && queueBaseAfter != 0 && queueCountAfter > queueCountBefore)
        {
            const uintptr_t messagePtr = queueBaseAfter + queueCountBefore * 0x698ull;
            std::uint8_t opcode = 0;
            std::int32_t queuedNodeId = 0;
            std::uint8_t resetSkills = 0;
            SafeReadValue(messagePtr, opcode);
            SafeReadValue(messagePtr + TALENT_MESSAGE_NODE_ID_OFFSET, queuedNodeId);
            SafeReadValue(messagePtr + TALENT_MESSAGE_RESET_SKILLS_OFFSET, resetSkills);

            post << " enqueued=1"
                << " messagePtr=" << Hex(messagePtr)
                << " opcode=" << static_cast<unsigned int>(opcode)
                << " queuedNode=" << static_cast<std::uint32_t>(queuedNodeId)
                << " resetSkills=" << static_cast<unsigned int>(resetSkills);
        }

        Log(post.str());
        return queued;
    }

    bool IsLastNodeFreshFor(DWORD maxAgeMs)
    {
        const DWORD lastTick = static_cast<DWORD>(g_lastNodeTick);
        if (lastTick == 0)
            return false;

        const DWORD now = GetTickCount();
        return now - lastTick <= maxAgeMs;
    }

    bool IsLastNodeFresh()
    {
        return IsLastNodeFreshFor(NODE_CAPTURE_STALE_MS);
    }

    DWORD LastNodeAgeMs()
    {
        const DWORD lastTick = static_cast<DWORD>(g_lastNodeTick);
        if (lastTick == 0)
            return MAXDWORD;

        return GetTickCount() - lastTick;
    }

    bool QueueLastHoveredNode(std::uint8_t opcode, const char* reason, DWORD maxAgeMs = NODE_CAPTURE_STALE_MS)
    {
        if (!IsLastNodeFreshFor(maxAgeMs))
        {
            std::ostringstream oss;
            oss << "[TalentPresets] no fresh hovered talent node captured"
                << " reason=" << reason
                << " ageMs=" << LastNodeAgeMs()
                << " maxAgeMs=" << maxAgeMs;
            Log(oss.str());
            return false;
        }

        const std::uint32_t nodeId = CurrentNodeId();
        if (nodeId == 0)
        {
            Log("[TalentPresets] hovered talent node id is invalid");
            return false;
        }

        const uintptr_t nodePtr = static_cast<uintptr_t>(g_lastNodePtr);
        LiveNodeMatch match = {};
        if (!TryBuildNodeMatchFromPtr(nodePtr, match) || match.nodeId != nodeId)
        {
            std::ostringstream oss;
            oss << "[TalentPresets] hovered talent node pointer is invalid"
                << " reason=" << reason
                << " node=" << nodeId
                << " nodePtr=" << Hex(nodePtr);
            Log(oss.str());
            return false;
        }

        uintptr_t queuePtr = 0;
        if (!EnqueueTalentMessage(nodeId, opcode, &queuePtr))
            return false;

        std::ostringstream oss;
        oss << "[TalentPresets] queued "
            << (opcode == TALENT_MESSAGE_UNLEARN ? "unlearn" : "learn")
            << " node=" << nodeId
            << " reason=" << reason
            << " queue=" << Hex(queuePtr)
            << " ageMs=" << LastNodeAgeMs()
            << " nodePtr=" << Hex(match.node)
            << " local28=" << static_cast<int>(match.local28)
            << " state4=" << static_cast<int>(match.state4)
            << " cost=" << static_cast<unsigned int>(match.cost)
            << " state40=" << static_cast<int>(match.state40)
            << " state41=" << static_cast<int>(match.state41)
            << " state42=" << static_cast<int>(match.state42)
            << " state43=" << static_cast<int>(match.state43);
        Log(oss.str());
        return true;
    }

    bool QueueHoveredNodeToggle()
    {
        const LONG state43 = g_lastNodeState43;

        if (state43 > 0)
            return QueueLastHoveredNode(TALENT_MESSAGE_UNLEARN, "toggle-unlearn");

        return QueueLastHoveredNode(TALENT_MESSAGE_LEARN, "toggle-learn");
    }

    bool LogKnownTalentLookup(std::uint32_t nodeId)
    {
        LiveNodeMatch match = {};
        std::size_t matchCount = 0;
        const bool found = FindLiveNodeById(nodeId, match, &matchCount);

        std::ostringstream oss;
        oss << "[TalentPresets] lookup talent=" << TalentName(nodeId)
            << " node=" << nodeId
            << " found=" << (found ? 1 : 0)
            << " matches=" << matchCount;

        if (found)
        {
            oss << " nodePtr=" << Hex(match.node)
                << " statePtr=" << Hex(match.state)
                << " local28=" << static_cast<int>(match.local28)
                << " state4=" << static_cast<int>(match.state4)
                << " cost=" << static_cast<unsigned int>(match.cost)
                << " state40=" << static_cast<int>(match.state40)
                << " state41=" << static_cast<int>(match.state41)
                << " state42=" << static_cast<int>(match.state42)
                << " state43=" << static_cast<int>(match.state43)
                << " score=" << match.score;
        }

        Log(oss.str());
        return found;
    }

    bool InvokeOriginalSkillTreeActionRaw(uintptr_t nodePtr, std::uint8_t mode)
    {
        if (nodePtr == 0)
            return false;

        auto* target = reinterpret_cast<SkillTreeContextEntryFn>(g_exeBase + RVA_SKILL_TREE_CONTEXT_ENTRY);
        const bool reactivateHook = g_skillTreeContextHook != nullptr && g_skillTreeContextHook->active;
        if (reactivateHook)
            g_skillTreeContextHook->deactivate();

        bool ok = false;
        __try
        {
            target(reinterpret_cast<void*>(nodePtr), mode);
            ok = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ok = false;
        }

        if (reactivateHook)
            g_skillTreeContextHook->activate();

        return ok;
    }

    bool CallSkillTreeActionWithSeh(uintptr_t nodePtr, std::uint8_t mode)
    {
        if (nodePtr == 0)
            return false;

        auto* target = reinterpret_cast<SkillTreeContextEntryFn>(g_exeBase + RVA_SKILL_TREE_CONTEXT_ENTRY);
        bool ok = false;
        __try
        {
            target(reinterpret_cast<void*>(nodePtr), mode);
            ok = true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            ok = false;
        }

        return ok;
    }

    bool InvokeOriginalHoveredNodeAction(std::uint8_t mode, const char* label)
    {
        if (!IsLastNodeFresh())
        {
            Log("[TalentPresets] no fresh hovered talent node captured");
            return false;
        }

        const auto nodePtr = static_cast<uintptr_t>(g_lastNodePtr);
        if (!InvokeOriginalSkillTreeActionRaw(nodePtr, mode))
        {
            Log("[TalentPresets] original skill action failed");
            return false;
        }

        std::ostringstream oss;
        oss << "[TalentPresets] invoked original skill action"
            << " mode=" << static_cast<int>(mode)
            << " label=" << label
            << " node=" << CurrentNodeId()
            << " nodePtr=" << Hex(nodePtr)
            << " local28=" << g_lastNodeLocal28
            << " state4=" << g_lastNodeState4
            << " cost=" << g_lastNodeCost
            << " state40=" << g_lastNodeState40
            << " state41=" << g_lastNodeState41
            << " state42=" << g_lastNodeState42
            << " state43=" << g_lastNodeState43;
        Log(oss.str());
        return true;
    }

    bool InvokeKnownTalentById(std::uint32_t nodeId, std::uint8_t mode, const char* label)
    {
        LiveNodeMatch match = {};
        std::size_t matchCount = 0;
        if (!FindLiveNodeById(nodeId, match, &matchCount))
        {
            std::ostringstream oss;
            oss << "[TalentPresets] invoke talent failed"
                << " label=" << label
                << " talent=" << TalentName(nodeId)
                << " node=" << nodeId
                << " matches=0";
            Log(oss.str());
            return false;
        }

        std::ostringstream start;
        start << "[TalentPresets] invoke talent"
            << " label=" << label
            << " talent=" << TalentName(nodeId)
            << " node=" << nodeId
            << " mode=" << static_cast<int>(mode)
            << " matches=" << matchCount
            << " nodePtr=" << Hex(match.node)
            << " statePtr=" << Hex(match.state)
            << " local28=" << static_cast<int>(match.local28)
            << " state4=" << static_cast<int>(match.state4)
            << " cost=" << static_cast<unsigned int>(match.cost)
            << " state40=" << static_cast<int>(match.state40)
            << " state41=" << static_cast<int>(match.state41)
            << " state42=" << static_cast<int>(match.state42)
            << " state43=" << static_cast<int>(match.state43);
        Log(start.str());

        if (!InvokeOriginalSkillTreeActionRaw(match.node, mode))
        {
            Log("[TalentPresets] invoke known talent failed inside game call");
            return false;
        }

        return true;
    }

    void ArmPendingTalentAction(std::uint32_t nodeId, uintptr_t nodePtr, std::uint8_t mode, const char* label, bool preferUnlearn = false)
    {
        InterlockedExchange(&g_pendingActionNodeIdBits, static_cast<LONG>(nodeId));
        InterlockedExchange64(&g_pendingActionNodePtr, static_cast<LONG64>(nodePtr));
        InterlockedExchange(&g_pendingActionMode, static_cast<LONG>(mode));
        InterlockedExchange(&g_pendingActionUnlearn, preferUnlearn ? 1 : 0);
        InterlockedExchange(&g_pendingActionArmed, 1);

        std::ostringstream oss;
        oss << "[TalentPresets] pending action armed"
            << " label=" << label
            << " talent=" << TalentName(nodeId)
            << " node=" << nodeId
            << " nodePtr=" << Hex(nodePtr)
            << " mode=" << static_cast<int>(mode)
            << " preferUnlearn=" << (preferUnlearn ? 1 : 0);
        Log(oss.str());
    }

    bool ArmPendingCurrentTalentAction(std::uint8_t mode, const char* label, bool preferUnlearn = false)
    {
        std::uint32_t nodeId = 0;
        uintptr_t nodePtr = 0;
        if (!IsLastNodeFresh())
        {
            std::ostringstream oss;
            oss << "[TalentPresets] no fresh hovered talent node captured; pending action will use current UI node"
                << " ageMs=" << LastNodeAgeMs()
                << " maxAgeMs=" << NODE_CAPTURE_STALE_MS;
            Log(oss.str());
        }
        else
        {
            nodeId = CurrentNodeId();
            nodePtr = static_cast<uintptr_t>(g_lastNodePtr);
            LiveNodeMatch captured = {};
            if (nodeId == 0 || !TryBuildNodeMatchFromPtr(nodePtr, captured) || captured.nodeId != nodeId)
            {
                std::ostringstream oss;
                oss << "[TalentPresets] hovered talent node is invalid; pending action will use current UI node"
                    << " node=" << nodeId
                    << " nodePtr=" << Hex(nodePtr);
                Log(oss.str());
                nodeId = 0;
                nodePtr = 0;
            }
        }

        ArmPendingTalentAction(nodeId, nodePtr, mode, label, preferUnlearn);
        return true;
    }

    bool PatchSkillActionContextCheck(const std::array<std::uint8_t, 3>& bytes, const char* label)
    {
        const uintptr_t address = g_exeBase + RVA_SKILL_ACTION_CONTEXT_CHECK;
        std::array<std::uint8_t, 3> current = {};
        if (!SafeRead(address, current.data(), current.size()))
        {
            Log(std::string("[TalentPresets] failed to read context check bytes for ") + label);
            return false;
        }

        const bool knownState =
            current == g_skillActionContextCheckDisabledBytes ||
            current == g_skillActionContextCheckEnabledBytes;
        if (!knownState)
        {
            std::ostringstream oss;
            oss << "[TalentPresets] unexpected context check bytes before " << label
                << " bytes=" << HexBytes(address, 5);
            Log(oss.str());
            return false;
        }

        if (current == bytes)
            return true;

        if (!Mem::Write(address, bytes.data(), bytes.size()))
        {
            Log(std::string("[TalentPresets] failed to patch context check for ") + label);
            return false;
        }

        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), bytes.size());
        return true;
    }

    bool EnableSkillActionContextBypass(const LiveNodeMatch& match)
    {
        if (!PatchSkillActionContextCheck(g_skillActionContextCheckEnabledBytes, "enable"))
            return false;

        std::ostringstream oss;
        oss << "[TalentPresets] context check patched"
            << " talent=" << TalentName(match.nodeId)
            << " node=" << match.nodeId
            << " nodePtr=" << Hex(match.node);
        Log(oss.str());
        return true;
    }

    void DisableSkillActionContextBypass(const char* reason)
    {
        if (PatchSkillActionContextCheck(g_skillActionContextCheckDisabledBytes, "restore"))
            return;

        std::ostringstream oss;
        oss << "[TalentPresets] failed to restore context check"
            << " reason=" << reason;
        Log(oss.str());
    }

    bool PatchSkillActionSlotReserve(const std::array<std::uint8_t, SKILL_ACTION_SLOT_RESERVE_PATCH_SIZE>& bytes, const char* label)
    {
        const uintptr_t address = g_exeBase + RVA_SKILL_ACTION_SLOT_RESERVE;
        std::array<std::uint8_t, SKILL_ACTION_SLOT_RESERVE_PATCH_SIZE> current = {};
        if (!SafeRead(address, current.data(), current.size()))
        {
            Log(std::string("[TalentPresets] failed to read slot reserve bytes for ") + label);
            return false;
        }

        const bool knownState =
            current == g_skillActionSlotReserveExpected ||
            current == g_skillActionSlotReserveEnabledBytes ||
            current == g_skillActionSlotReserveUnlearnBytes;
        if (!knownState)
        {
            std::ostringstream oss;
            oss << "[TalentPresets] unexpected slot reserve bytes before " << label
                << " bytes=" << HexBytes(address, 8);
            Log(oss.str());
            return false;
        }

        if (current == bytes)
            return true;

        if (!Mem::Write(address, bytes.data(), bytes.size()))
        {
            Log(std::string("[TalentPresets] failed to patch slot reserve for ") + label);
            return false;
        }

        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), bytes.size());
        return true;
    }

    bool EnableSkillActionSlotReserveBypass(const LiveNodeMatch& match, bool preferUnlearn)
    {
        const auto& patchBytes = preferUnlearn ? g_skillActionSlotReserveUnlearnBytes : g_skillActionSlotReserveEnabledBytes;
        if (!PatchSkillActionSlotReserve(patchBytes, preferUnlearn ? "enable-unlearn" : "enable-learn"))
            return false;

        std::ostringstream oss;
        oss << "[TalentPresets] slot reserve patched"
            << " talent=" << TalentName(match.nodeId)
            << " node=" << match.nodeId
            << " nodePtr=" << Hex(match.node)
            << " preferUnlearn=" << (preferUnlearn ? 1 : 0);
        Log(oss.str());
        return true;
    }

    void DisableSkillActionSlotReserveBypass(const char* reason)
    {
        if (PatchSkillActionSlotReserve(g_skillActionSlotReserveExpected, "restore"))
            return;

        std::ostringstream oss;
        oss << "[TalentPresets] failed to restore slot reserve"
            << " reason=" << reason;
        Log(oss.str());
    }

    void BeginPresetActionPacing(const QueuedTalentAction& action, DWORD now)
    {
        std::lock_guard<std::mutex> lock(g_presetPacingMutex);
        if (action.resetAll)
        {
            g_presetPacing = {};
            return;
        }

        g_presetPacing.pending = action.expectedLevelAfter >= 0;
        g_presetPacing.nodeId = action.nodeId;
        g_presetPacing.unlearn = action.unlearn;
        g_presetPacing.expectedLevelAfter = action.expectedLevelAfter;
        g_presetPacing.sentTick = now;
    }

    void ClearPresetActionPacing()
    {
        std::lock_guard<std::mutex> lock(g_presetPacingMutex);
        g_presetPacing = {};
    }

    bool IsPresetActionConfirmed(const PresetPacingState& pacing)
    {
        if (!pacing.pending || pacing.expectedLevelAfter < 0)
            return true;

        LiveNodeMatch match = {};
        if (!FindLiveNodeById(pacing.nodeId, match, nullptr))
            return false;

        const int level = CurrentTalentLevel(match);
        return pacing.unlearn
            ? level <= pacing.expectedLevelAfter
            : level >= pacing.expectedLevelAfter;
    }

    bool ShouldWaitForPresetPacing(const QueuedTalentAction& nextAction, DWORD now)
    {
        PresetPacingState pacing = {};
        {
            std::lock_guard<std::mutex> lock(g_presetPacingMutex);
            pacing = g_presetPacing;
        }

        if (!pacing.pending)
            return false;

        const DWORD stableFallback = pacing.unlearn
            ? PRESET_CLEAR_ACTION_INTERVAL_MS
            : PRESET_LEARN_ACTION_INTERVAL_MS;
        const DWORD adaptiveMinimum = pacing.unlearn
            ? PRESET_CLEAR_VERIFY_MIN_MS
            : PRESET_LEARN_VERIFY_MIN_MS;

        const bool sameLearnNode = !pacing.unlearn && !nextAction.unlearn && pacing.nodeId == nextAction.nodeId;
        const DWORD minimumWait = sameLearnNode ? PRESET_LEARN_ACTION_INTERVAL_MS : adaptiveMinimum;

        if (now - pacing.sentTick < minimumWait)
            return true;

        if (IsPresetActionConfirmed(pacing) || now - pacing.sentTick >= stableFallback)
        {
            ClearPresetActionPacing();
            return false;
        }

        return true;
    }

    PresetActionResult ExecutePresetQueuedAction(const QueuedTalentAction& action)
    {
        ScopedInterlockedFlag executionGuard(&g_presetActionExecuting);
        if (!executionGuard)
            return PresetActionResult::Busy;

        if (action.resetAll)
        {
            const bool queued = EnqueueResetAllTalentsMessage();
            bool willRetry = false;
            if (queued || action.attempts >= 8)
            {
                DropPresetAction();
            }
            else
            {
                willRetry = true;
                RetryPresetActionLater(action);
            }

            const std::size_t remaining = PresetQueueSize();
            std::ostringstream status;
            if (remaining == 0)
            {
                SetPresetStatus(Text().checkingTree);
            }
            else
            {
                status << Text().applyingPreset << remaining << Text().remaining;
                SetPresetStatus(status.str());
            }

            return willRetry ? PresetActionResult::Retried : PresetActionResult::Progress;
        }

        LiveNodeMatch match = {};
        std::size_t matchCount = 0;
        const bool haveMatch = FindLiveNodeById(action.nodeId, match, &matchCount);
        const int liveLevel = haveMatch ? CurrentTalentLevel(match) : -1;

        if (haveMatch && action.unlearn && liveLevel == 0)
        {
            DropPresetAction();
            return PresetActionResult::Progress;
        }

        if (haveMatch && action.unlearn && action.expectedLevelAfter >= 0 && liveLevel <= action.expectedLevelAfter)
        {
            DropPresetAction();
            return PresetActionResult::Progress;
        }

        if (haveMatch && !action.unlearn && action.expectedLevelAfter >= 0 && liveLevel >= action.expectedLevelAfter)
        {
            DropPresetAction();
            return PresetActionResult::Progress;
        }

        if (haveMatch && !action.unlearn &&
            (IsSingleUnlockTalent(match.nodeId) || match.state40 == 0 || match.state40 == 0xFF) &&
            liveLevel > 0)
        {
            DropPresetAction();
            return PresetActionResult::Progress;
        }

        if (haveMatch && !action.unlearn && match.state40 != 0xFF && match.state40 > 0 &&
            match.state41 != 0xFF && match.state41 >= match.state40)
        {
            DropPresetAction();
            return PresetActionResult::Progress;
        }

        uintptr_t queueBeforePtr = 0;
        uintptr_t queueAfterPtr = 0;
        uintptr_t queueBaseBefore = 0;
        uintptr_t queueBaseAfter = 0;
        std::uint64_t queueCountBefore = 0;
        std::uint64_t queueCapacityBefore = 0;
        std::uint64_t queueCountAfter = 0;
        std::uint64_t queueCapacityAfter = 0;

        uintptr_t gameRoot = 0;
        uintptr_t contextSlot = 0;
        uintptr_t activePlayer = 0;
        uintptr_t talentState = 0;
        ResolveTalentContext(gameRoot, contextSlot, activePlayer, talentState, queueBeforePtr);
        ReadTalentQueueMetrics(queueBeforePtr, queueBaseBefore, queueCountBefore, queueCapacityBefore);

        const std::uint8_t opcode = action.unlearn ? TALENT_MESSAGE_UNLEARN : TALENT_MESSAGE_LEARN;
        uintptr_t queuedPtr = 0;
        const bool queued = EnqueueTalentMessage(action.nodeId, opcode, &queuedPtr);

        ResolveTalentContext(gameRoot, contextSlot, activePlayer, talentState, queueAfterPtr);
        ReadTalentQueueMetrics(queueAfterPtr, queueBaseAfter, queueCountAfter, queueCapacityAfter);

        bool willRetry = false;
        if (queued || action.attempts >= 8)
        {
            if (queued)
                BeginPresetActionPacing(action, GetTickCount());
            DropPresetAction();
        }
        else
        {
            willRetry = true;
            RetryPresetActionLater(action);
        }

        const std::size_t remaining = PresetQueueSize();

        std::ostringstream post;
        post << "[TalentPresets] preset action result"
            << " talent=" << TalentName(action.nodeId)
            << " node=" << action.nodeId
            << " unlearn=" << (action.unlearn ? 1 : 0)
            << " directQueue=1"
            << " queued=" << (queued ? 1 : 0)
            << " matches=" << matchCount
            << " queueBefore=" << Hex(queueBeforePtr)
            << " countBefore=" << queueCountBefore
            << " queueAfter=" << Hex(queueAfterPtr)
            << " countAfter=" << queueCountAfter
            << " outQueue=" << Hex(queuedPtr)
            << " remaining=" << remaining;

        if (queued && queueAfterPtr != 0 && queueBaseAfter != 0 && queueCountAfter > queueCountBefore)
        {
            const uintptr_t messagePtr = queueBaseAfter + queueCountBefore * 0x698ull;
            std::uint8_t opcode = 0;
            std::int32_t queuedNodeId = 0;
            SafeReadValue(messagePtr, opcode);
            SafeReadValue(messagePtr + TALENT_MESSAGE_NODE_ID_OFFSET, queuedNodeId);

            post << " enqueued=1"
                << " messagePtr=" << Hex(messagePtr)
                << " opcode=" << static_cast<unsigned int>(opcode)
                << " queuedNode=" << static_cast<std::uint32_t>(queuedNodeId);
        }
        else
        {
            post << " retry=" << (willRetry ? 1 : 0)
                << " attempts=" << action.attempts;
        }

        Log(post.str());

        if (remaining == 0)
        {
            if (IsPresetJobActive())
                SetPresetStatus(Text().checkingTree);
            else
                SetPresetStatus(Text().presetAppliedFallback);
        }
        else
        {
            std::ostringstream status;
            status << Text().applyingPreset << remaining << Text().remaining;
            SetPresetStatus(status.str());
        }

        return willRetry ? PresetActionResult::Retried : PresetActionResult::Progress;
    }

    void ProcessPresetActionQueueTick(DWORD now)
    {
        QueuedTalentAction presetAction = {};
        std::size_t presetQueueSize = 0;
        if (!TryPeekPresetAction(presetAction, presetQueueSize))
            return;

        if (ShouldWaitForPresetPacing(presetAction, now))
            return;

        const DWORD minimumInterval = presetAction.unlearn
            ? PRESET_CLEAR_VERIFY_MIN_MS
            : PRESET_LEARN_VERIFY_MIN_MS;
        if (now - g_lastPresetQueueProcessTick < minimumInterval)
            return;

        const PresetActionResult result = ExecutePresetQueuedAction(presetAction);
        if (result != PresetActionResult::Busy)
            g_lastPresetQueueProcessTick = now;
    }

    bool IsPresetActionQueueActive()
    {
        return PresetQueueSize() != 0;
    }

    void __fastcall HandleSkillTreeActionCallsite(void* currentNode, std::uint8_t currentMode)
    {
        auto* target = reinterpret_cast<SkillTreeContextEntryFn>(g_exeBase + RVA_SKILL_TREE_CONTEXT_ENTRY);

        if (InterlockedCompareExchange(&g_pendingActionArmed, 0, 1) == 1)
        {
            const std::uint32_t armedNodeId = static_cast<std::uint32_t>(g_pendingActionNodeIdBits);
            const uintptr_t armedNodePtr = static_cast<uintptr_t>(InterlockedExchange64(&g_pendingActionNodePtr, 0));
            const std::uint8_t armedMode = static_cast<std::uint8_t>(g_pendingActionMode);
            const bool preferUnlearn = InterlockedExchange(&g_pendingActionUnlearn, 0) == 1;

            LiveNodeMatch match = {};
            LiveNodeMatch currentMatch = {};
            LiveNodeMatch armedMatch = {};
            std::size_t matchCount = 0;
            bool usedCurrentNode = TryBuildNodeMatchFromPtr(reinterpret_cast<uintptr_t>(currentNode), currentMatch);
            bool usedArmedPtr = false;

            if (armedNodePtr != 0)
            {
                usedArmedPtr = TryBuildNodeMatchFromPtr(armedNodePtr, armedMatch) &&
                    (armedNodeId == 0 || armedMatch.nodeId == armedNodeId);
                if (usedArmedPtr)
                {
                    match = armedMatch;
                    matchCount = 1;
                }
                else
                {
                    std::ostringstream oss;
                    oss << "[TalentPresets] armed node pointer is no longer valid"
                        << " armedNode=" << armedNodeId
                        << " armedNodePtr=" << Hex(armedNodePtr);
                    Log(oss.str());
                    match = {};
                }
            }

            if (match.node == 0 && armedNodeId != 0)
            {
                FindLiveNodeById(armedNodeId, match, &matchCount);
            }

            if (match.node == 0 && usedCurrentNode)
            {
                match = currentMatch;
                matchCount = 1;
            }

            if (match.node != 0)
            {
                const std::uint32_t nodeId = match.nodeId;
                std::ostringstream oss;
                oss << "[TalentPresets] dispatch pending action"
                    << " talent=" << TalentName(nodeId)
                    << " node=" << nodeId
                    << " armedNode=" << armedNodeId
                    << " armedNodePtr=" << Hex(armedNodePtr)
                    << " armedMode=" << static_cast<int>(armedMode)
                    << " matches=" << matchCount
                    << " nodePtr=" << Hex(match.node)
                    << " statePtr=" << Hex(match.state)
                    << " local28=" << static_cast<int>(match.local28)
                    << " state4=" << static_cast<int>(match.state4)
                    << " cost=" << static_cast<unsigned int>(match.cost)
                    << " state40=" << static_cast<int>(match.state40)
                    << " state41=" << static_cast<int>(match.state41)
                    << " state42=" << static_cast<int>(match.state42)
                    << " state43=" << static_cast<int>(match.state43)
                    << " currentNode=" << Hex(reinterpret_cast<uintptr_t>(currentNode))
                    << " currentNodeValid=" << (usedCurrentNode ? 1 : 0);
                if (usedCurrentNode)
                {
                    oss << " currentNodeId=" << currentMatch.nodeId
                        << " currentLocal28=" << static_cast<int>(currentMatch.local28)
                        << " currentState4=" << static_cast<int>(currentMatch.state4)
                        << " currentState40=" << static_cast<int>(currentMatch.state40)
                        << " currentState41=" << static_cast<int>(currentMatch.state41)
                        << " currentState42=" << static_cast<int>(currentMatch.state42)
                        << " currentState43=" << static_cast<int>(currentMatch.state43);
                }
                oss
                    << " currentMode=" << static_cast<int>(currentMode)
                    << " usedCurrentNode=" << (usedCurrentNode ? 1 : 0)
                    << " usedArmedPtr=" << (usedArmedPtr ? 1 : 0)
                    << " preferUnlearn=" << (preferUnlearn ? 1 : 0);
                Log(oss.str());

                if (preferUnlearn && match.state43 == 0 && match.state41 == 0 && match.local28 == 0)
                {
                    Log("[TalentPresets] pending action skipped because the target already looks inactive");
                    return;
                }

                if (!preferUnlearn && match.state40 != 0xFF && match.state40 > 0 &&
                    match.state41 != 0xFF && match.state41 >= match.state40)
                {
                    Log("[TalentPresets] pending action skipped because the target already looks maxed");
                    return;
                }

                uintptr_t queueBeforePtr = 0;
                uintptr_t queueAfterPtr = 0;
                uintptr_t queueBaseBefore = 0;
                uintptr_t queueBaseAfter = 0;
                std::uint64_t queueCountBefore = 0;
                std::uint64_t queueCapacityBefore = 0;
                std::uint64_t queueCountAfter = 0;
                std::uint64_t queueCapacityAfter = 0;

                uintptr_t gameRoot = 0;
                uintptr_t contextSlot = 0;
                uintptr_t activePlayer = 0;
                uintptr_t talentState = 0;
                ResolveTalentContext(gameRoot, contextSlot, activePlayer, talentState, queueBeforePtr);
                ReadTalentQueueMetrics(queueBeforePtr, queueBaseBefore, queueCountBefore, queueCapacityBefore);

                const bool contextBypassEnabled = EnableSkillActionContextBypass(match);
                const bool slotReserveBypassEnabled = EnableSkillActionSlotReserveBypass(match, preferUnlearn);

                std::uint8_t originalState43 = 0;
                bool state43Patched = false;
                if (preferUnlearn && match.state != 0 &&
                    SafeReadValue(match.state + 0x43, originalState43))
                {
                    const std::uint8_t enabled = 1;
                    state43Patched = SafeWriteValue(match.state + 0x43, enabled);
                }

                const std::uint8_t effectiveMode = armedMode;
                const bool callSucceeded = CallSkillTreeActionWithSeh(match.node, effectiveMode);

                if (state43Patched)
                    SafeWriteValue(match.state + 0x43, originalState43);

                ResolveTalentContext(gameRoot, contextSlot, activePlayer, talentState, queueAfterPtr);
                ReadTalentQueueMetrics(queueAfterPtr, queueBaseAfter, queueCountAfter, queueCapacityAfter);

                if (slotReserveBypassEnabled)
                    DisableSkillActionSlotReserveBypass("post-action");
                if (contextBypassEnabled)
                    DisableSkillActionContextBypass("post-action");

                std::ostringstream post;
                post << "[TalentPresets] pending action result"
                    << " talent=" << TalentName(nodeId)
                    << " node=" << nodeId
                    << " callSucceeded=" << (callSucceeded ? 1 : 0)
                    << " preferUnlearn=" << (preferUnlearn ? 1 : 0)
                    << " armedMode=" << static_cast<int>(armedMode)
                    << " effectiveMode=" << static_cast<int>(effectiveMode)
                    << " state43Patched=" << (state43Patched ? 1 : 0)
                    << " queueBefore=" << Hex(queueBeforePtr)
                    << " countBefore=" << queueCountBefore
                    << " capacityBefore=" << queueCapacityBefore
                    << " queueAfter=" << Hex(queueAfterPtr)
                    << " countAfter=" << queueCountAfter
                    << " capacityAfter=" << queueCapacityAfter;

                if (queueAfterPtr != 0 && queueBaseAfter != 0 && queueCountAfter > queueCountBefore)
                {
                    const uintptr_t messagePtr = queueBaseAfter + queueCountBefore * 0x698ull;
                    std::uint8_t opcode = 0;
                    std::int32_t queuedNodeId = 0;
                    SafeReadValue(messagePtr, opcode);
                    SafeReadValue(messagePtr + TALENT_MESSAGE_NODE_ID_OFFSET, queuedNodeId);

                    post << " enqueued=1"
                        << " messagePtr=" << Hex(messagePtr)
                        << " opcode=" << static_cast<unsigned int>(opcode)
                        << " queuedNode=" << static_cast<std::uint32_t>(queuedNodeId);
                }
                else
                {
                    post << " enqueued=0";
                }

                Log(post.str());
                return;
            }

            std::ostringstream oss;
            oss << "[TalentPresets] pending action target not found"
                << " talent=" << TalentName(armedNodeId)
                << " armedNode=" << armedNodeId
                << " armedNodePtr=" << Hex(armedNodePtr)
                << " currentNode=" << Hex(reinterpret_cast<uintptr_t>(currentNode))
                << " currentNodeValid=" << (usedCurrentNode ? 1 : 0);
            Log(oss.str());
            return;
        }

        target(currentNode, currentMode);
    }

    std::vector<std::uint8_t> BuildRootHookShellcode(const std::uint8_t*, std::size_t)
    {
        std::vector<std::uint8_t> code;
        code.reserve(192);

        auto append = [&code](std::initializer_list<std::uint8_t> bytes)
        {
            code.insert(code.end(), bytes.begin(), bytes.end());
        };

        append({ 0x48, 0x89, 0x6C, 0x24, 0x10 });    // mov [rsp+10h], rbp
        append({ 0x48, 0x89, 0x74, 0x24, 0x20 });    // mov [rsp+20h], rsi
        append({ 0x57 });                            // push rdi
        append({ 0x48, 0x83, 0xEC, 0x50 });          // sub rsp, 50h
        append({ 0x48, 0xB8 });                      // mov rax, imm64

        const std::size_t globalOffset = code.size();
        for (int index = 0; index < 8; ++index)
            code.push_back(0x00);

        append({ 0x48, 0x8B, 0x28 });                // mov rbp, [rax]
        append({ 0x48, 0x8B, 0xF9 });                // mov rdi, rcx
        append({ 0x0F, 0xB6, 0xF2 });                // movzx esi, dl
        append({ 0x48, 0x8B, 0x85, 0xC8, 0x00, 0x00, 0x00 }); // mov rax, [rbp+0C8h]
        append({ 0x48, 0x8B, 0x08 });                // mov rcx, [rax]
        append({ 0x48, 0x83, 0xEC, 0x30 });          // sub rsp, 30h
        append({ 0x48, 0x89, 0x44, 0x24, 0x20 });    // mov [rsp+20h], rax
        append({ 0x48, 0x89, 0x4C, 0x24, 0x28 });    // mov [rsp+28h], rcx
        append({ 0x49, 0x8B, 0xC8 });                // mov r8, rcx
        append({ 0x48, 0x89, 0xE9 });                // mov rcx, rbp
        append({ 0x48, 0x89, 0xC2 });                // mov rdx, rax
        append({ 0x48, 0xB8 });                      // mov rax, imm64

        const std::size_t handlerOffset = code.size();
        for (int index = 0; index < 8; ++index)
            code.push_back(0x00);

        append({ 0xFF, 0xD0 });                      // call rax
        append({ 0x48, 0x8B, 0x44, 0x24, 0x20 });    // mov rax, [rsp+20h]
        append({ 0x48, 0x8B, 0x4C, 0x24, 0x28 });    // mov rcx, [rsp+28h]
        append({ 0x48, 0x83, 0xC4, 0x30 });          // add rsp, 30h
        append({ 0xE9, 0x00, 0x00, 0x00, 0x00 });    // jmp back

        *reinterpret_cast<std::uint64_t*>(code.data() + globalOffset) =
            static_cast<std::uint64_t>(g_exeBase + RVA_GAME_CONTEXT_GLOBAL);
        *reinterpret_cast<std::uint64_t*>(code.data() + handlerOffset) =
            reinterpret_cast<std::uint64_t>(&CaptureSkillTreeContext);
        return code;
    }

    std::vector<std::uint8_t> BuildNodeHookShellcode(const std::uint8_t* stolenBytes, std::size_t stolenSize)
    {
        std::vector<std::uint8_t> code;
        code.reserve(128);

        auto append = [&code](std::initializer_list<std::uint8_t> bytes)
        {
            code.insert(code.end(), bytes.begin(), bytes.end());
        };

        append({ 0x48, 0x83, 0xEC, 0x48 });          // sub rsp, 48h
        append({ 0x48, 0x89, 0x4C, 0x24, 0x20 });    // mov [rsp+20h], rcx
        append({ 0x48, 0x89, 0x54, 0x24, 0x28 });    // mov [rsp+28h], rdx
        append({ 0x4C, 0x89, 0x44, 0x24, 0x30 });    // mov [rsp+30h], r8
        append({ 0x4C, 0x89, 0x4C, 0x24, 0x38 });    // mov [rsp+38h], r9
        append({ 0x49, 0x8B, 0xCE });                // mov rcx, r14
        append({ 0x48, 0xB8 });                      // mov rax, imm64

        const std::size_t handlerOffset = code.size();
        for (int index = 0; index < 8; ++index)
            code.push_back(0x00);

        append({ 0xFF, 0xD0 });                      // call rax
        append({ 0x48, 0x8B, 0x4C, 0x24, 0x20 });    // mov rcx, [rsp+20h]
        append({ 0x48, 0x8B, 0x54, 0x24, 0x28 });    // mov rdx, [rsp+28h]
        append({ 0x4C, 0x8B, 0x44, 0x24, 0x30 });    // mov r8, [rsp+30h]
        append({ 0x4C, 0x8B, 0x4C, 0x24, 0x38 });    // mov r9, [rsp+38h]
        append({ 0x48, 0x83, 0xC4, 0x48 });          // add rsp, 48h
        code.insert(code.end(), stolenBytes, stolenBytes + stolenSize);
        append({ 0xE9, 0x00, 0x00, 0x00, 0x00 });    // jmp back

        *reinterpret_cast<std::uint64_t*>(code.data() + handlerOffset) =
            reinterpret_cast<std::uint64_t>(&CaptureSkillNode);
        return code;
    }

    std::vector<std::uint8_t> BuildDrawNodeLinkHookShellcode(const std::uint8_t* stolenBytes, std::size_t stolenSize)
    {
        std::vector<std::uint8_t> code;
        code.reserve(128);

        auto append = [&code](std::initializer_list<std::uint8_t> bytes)
        {
            code.insert(code.end(), bytes.begin(), bytes.end());
        };

        append({ 0x48, 0x83, 0xEC, 0x48 });          // sub rsp, 48h
        append({ 0x48, 0x89, 0x4C, 0x24, 0x20 });    // mov [rsp+20h], rcx
        append({ 0x48, 0x89, 0x54, 0x24, 0x28 });    // mov [rsp+28h], rdx
        append({ 0x4C, 0x89, 0x44, 0x24, 0x30 });    // mov [rsp+30h], r8
        append({ 0x4C, 0x89, 0x4C, 0x24, 0x38 });    // mov [rsp+38h], r9
        append({ 0x48, 0x89, 0xD1 });                // mov rcx, rdx
        append({ 0x48, 0xB8 });                      // mov rax, imm64

        const std::size_t handlerOffset = code.size();
        for (int index = 0; index < 8; ++index)
            code.push_back(0x00);

        append({ 0xFF, 0xD0 });                      // call rax
        append({ 0x48, 0x8B, 0x4C, 0x24, 0x20 });    // mov rcx, [rsp+20h]
        append({ 0x48, 0x8B, 0x54, 0x24, 0x28 });    // mov rdx, [rsp+28h]
        append({ 0x4C, 0x8B, 0x44, 0x24, 0x30 });    // mov r8, [rsp+30h]
        append({ 0x4C, 0x8B, 0x4C, 0x24, 0x38 });    // mov r9, [rsp+38h]
        append({ 0x48, 0x83, 0xC4, 0x48 });          // add rsp, 48h
        code.insert(code.end(), stolenBytes, stolenBytes + stolenSize);
        append({ 0xE9, 0x00, 0x00, 0x00, 0x00 });    // jmp back

        *reinterpret_cast<std::uint64_t*>(code.data() + handlerOffset) =
            reinterpret_cast<std::uint64_t>(&CaptureDrawNodeLink);
        return code;
    }

    std::vector<std::uint8_t> BuildSkillActionCallsiteShellcode()
    {
        std::vector<std::uint8_t> code;
        code.reserve(64);

        auto append = [&code](std::initializer_list<std::uint8_t> bytes)
        {
            code.insert(code.end(), bytes.begin(), bytes.end());
        };

        append({ 0x48, 0x83, 0xEC, 0x20 });          // sub rsp, 20h
        append({ 0x48, 0xB8 });                      // mov rax, imm64

        const std::size_t handlerOffset = code.size();
        for (int index = 0; index < 8; ++index)
            code.push_back(0x00);

        append({ 0xFF, 0xD0 });                      // call rax
        append({ 0x48, 0x83, 0xC4, 0x20 });          // add rsp, 20h
        append({ 0xE9, 0x00, 0x00, 0x00, 0x00 });    // jmp back

        *reinterpret_cast<std::uint64_t*>(code.data() + handlerOffset) =
            reinterpret_cast<std::uint64_t>(&HandleSkillTreeActionCallsite);
        return code;
    }

    Mem::Detour* InstallHook(uintptr_t rva, const std::uint8_t* expected, std::size_t stolenSize, const std::vector<std::uint8_t>& shellcode, const char* label)
    {
        const uintptr_t address = g_exeBase + rva;
        if (!ValidateBytes(address, expected, stolenSize, label))
            return nullptr;

        auto* detour = new Mem::Detour(address, const_cast<std::uint8_t*>(shellcode.data()), shellcode.size(), false, stolenSize - 5);

        const std::size_t jumpImmediateOffset = shellcode.size() - 4;
        const uintptr_t jumpSource = detour->shellcode->data->address + jumpImmediateOffset + sizeof(std::uint32_t);
        const uintptr_t jumpTarget = address + stolenSize;
        detour->shellcode->updateValue<std::uint32_t>(
            jumpImmediateOffset,
            static_cast<std::uint32_t>(jumpTarget - jumpSource)
        );

        return detour;
    }

    void LogCurrentNode(bool includeBytes)
    {
        const std::uint32_t nodeId = CurrentNodeId();
        const LONG hitCount = g_nodeHitCount;
        if (nodeId == 0)
            return;

        const auto nodePtr = static_cast<uintptr_t>(g_lastNodePtr);
        const auto statePtr = static_cast<uintptr_t>(g_lastNodeStatePtr);

        std::ostringstream oss;
        oss << "[TalentPresets] node=" << nodeId
            << " hits=" << hitCount
            << " nodePtr=" << Hex(nodePtr)
            << " statePtr=" << Hex(statePtr)
            << " local28=" << g_lastNodeLocal28
            << " state4=" << g_lastNodeState4
            << " cost=" << g_lastNodeCost
            << " state40=" << g_lastNodeState40
            << " state41=" << g_lastNodeState41
            << " state42=" << g_lastNodeState42
            << " state43=" << g_lastNodeState43
            << " skillTreeUiSeen=" << g_skillTreeRootSeen;
        if (includeBytes)
        {
            oss << " nodeBytes=" << HexBytes(nodePtr, 80)
                << " stateBytes=" << HexBytes(statePtr, 80);
        }
        Log(oss.str());
    }

    void DumpSamples()
    {
        const LONG count = (std::min)(static_cast<LONG>(g_nodeSampleCount), static_cast<LONG>(MAX_NODE_SAMPLES));
        std::ostringstream header;
        header << "[TalentPresets] sample dump count=" << count
            << " totalHits=" << g_nodeHitCount
            << " skillTreeUiSeen=" << g_skillTreeRootSeen;
        Log(header.str());

        for (LONG index = 0; index < count; ++index)
        {
            const NodeSample& sample = g_nodeSamples[index];
            if (sample.node == 0)
                continue;

            std::ostringstream oss;
            oss << "[TalentPresets] sample[" << index << "] node=" << sample.nodeId
                << " hits=" << sample.hits
                << " nodePtr=" << Hex(sample.node)
                << " statePtr=" << Hex(sample.state)
                << " local28=" << static_cast<int>(sample.local28)
                << " state4=" << static_cast<int>(sample.state4)
                << " cost=" << static_cast<unsigned int>(sample.cost)
                << " state40=" << static_cast<int>(sample.state40)
                << " state41=" << static_cast<int>(sample.state41)
                << " state42=" << static_cast<int>(sample.state42)
                << " state43=" << static_cast<int>(sample.state43)
                << " nodeBytes=" << HexBytes(sample.node, 80)
                << " stateBytes=" << HexBytes(sample.state, 80);
            Log(oss.str());
        }
    }

    void DumpLiveTreeNodes()
    {
        uintptr_t treeRoot = 0;
        std::uint16_t nodeCount = 0;
        if (!ResolveTreeUiRoot(treeRoot, nodeCount))
        {
            Log("[TalentPresets] live tree dump unavailable");
            return;
        }

        std::ostringstream header;
        header << "[TalentPresets] live tree dump root=" << Hex(treeRoot)
            << " count=" << nodeCount
            << " stride=" << SKILL_TREE_NODE_STRIDE;
        Log(header.str());

        std::uint16_t validCount = 0;
        for (std::uint16_t index = 0; index < nodeCount; ++index)
        {
            LiveNodeMatch match = {};
            const uintptr_t candidate = treeRoot + SKILL_TREE_NODE_ARRAY_OFFSET + static_cast<std::size_t>(index) * SKILL_TREE_NODE_STRIDE;
            if (!TryBuildNodeMatchFromPtr(candidate, match))
                continue;

            ++validCount;
            std::ostringstream oss;
            oss << "[TalentPresets] live[" << index << "]"
                << " node=" << match.nodeId
                << " talent=" << TalentName(match.nodeId)
                << " nodePtr=" << Hex(match.node)
                << " statePtr=" << Hex(match.state)
                << " local28=" << static_cast<int>(match.local28)
                << " state4=" << static_cast<int>(match.state4)
                << " cost=" << static_cast<unsigned int>(match.cost)
                << " state40=" << static_cast<int>(match.state40)
                << " state41=" << static_cast<int>(match.state41)
                << " state42=" << static_cast<int>(match.state42)
                << " state43=" << static_cast<int>(match.state43)
                << " score=" << match.score;
            Log(oss.str());
        }

        std::ostringstream footer;
        footer << "[TalentPresets] live tree dump complete valid=" << validCount
            << " count=" << nodeCount;
        Log(footer.str());
    }

    void LogTalentContext()
    {
        uintptr_t gameRoot = 0;
        uintptr_t contextSlot = 0;
        uintptr_t activePlayer = 0;
        uintptr_t talentState = 0;
        uintptr_t talentQueue = 0;

        std::ostringstream oss;
        oss << "[TalentPresets] context";
        if (ResolveTalentContext(gameRoot, contextSlot, activePlayer, talentState, talentQueue))
        {
            oss << " gameRoot=" << Hex(gameRoot)
                << " contextSlot=" << Hex(contextSlot)
                << " activePlayer=" << Hex(activePlayer)
                << " talentState=" << Hex(talentState)
                << " queue=" << Hex(talentQueue);
        }
        else
        {
            oss << " unavailable";
        }
        Log(oss.str());
    }
}

class TalentPresetsMod : public Mod
{
public:
    void Load(ModContext* modContext) override
    {
        g_modContext = modContext;
        g_exeBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
        const std::string moduleDir = ModuleDirectory();
        if (!moduleDir.empty())
        {
            g_presetFilePath = moduleDir + "\\talent_presets.txt";
            g_presetUiFilePath = moduleDir + "\\talent_presets_ui.ini";
            g_presetLangFilePath = moduleDir + "\\talent_presets_lang.txt";
        }
        DetectUiLanguage(moduleDir);
        LoadPresetsFromDisk();
        LoadPresetUiPosition();

        Log("[TalentPresets] loading live talent preset panel 0.5.27 for Enshrouded 1004637");
        Log(std::string("[TalentPresets] ui language ") + Text().languageCode);
        if (!g_presetFilePath.empty())
            Log(std::string("[TalentPresets] preset file ") + g_presetFilePath);

        const auto rootShellcode = BuildRootHookShellcode(g_skillTreeContextExpected.data(), g_skillTreeContextExpected.size());
        g_skillTreeContextHook = InstallHook(
            RVA_SKILL_TREE_CONTEXT_ENTRY,
            g_skillTreeContextExpected.data(),
            g_skillTreeContextExpected.size(),
            rootShellcode,
            "skillTreeContextEntry"
        );

        const auto nodeShellcode = BuildNodeHookShellcode(g_skillNodeStatusExpected.data(), g_skillNodeStatusExpected.size());
        g_skillNodeStatusHook = InstallHook(
            RVA_SKILL_NODE_STATUS_TICK,
            g_skillNodeStatusExpected.data(),
            g_skillNodeStatusExpected.size(),
            nodeShellcode,
            "skillNodeStatusTick"
        );

        const auto drawNodeLinkShellcode = BuildDrawNodeLinkHookShellcode(g_drawNodeLinkExpected.data(), g_drawNodeLinkExpected.size());
        g_drawNodeLinkHook = InstallHook(
            RVA_DRAW_NODE_LINK,
            g_drawNodeLinkExpected.data(),
            g_drawNodeLinkExpected.size(),
            drawNodeLinkShellcode,
            "drawNodeLink"
        );

        const auto skillActionCallsiteShellcode = BuildSkillActionCallsiteShellcode();
        g_skillActionCallsiteHook = InstallHook(
            RVA_SKILL_ACTION_CALLSITE,
            g_skillActionCallsiteExpected.data(),
            g_skillActionCallsiteExpected.size(),
            skillActionCallsiteShellcode,
            "skillActionCallsite"
        );

        ValidateBytes(
            g_exeBase + RVA_SKILL_ACTION_CONTEXT_CHECK,
            g_skillActionContextCheckExpected.data(),
            g_skillActionContextCheckExpected.size(),
            "skillActionContextCheck");
    }

    void Unload(ModContext*) override
    {
        ShutdownPresetUiThread();
        delete g_skillNodeStatusHook;
        g_skillNodeStatusHook = nullptr;
        delete g_drawNodeLinkHook;
        g_drawNodeLinkHook = nullptr;
        delete g_skillTreeContextHook;
        g_skillTreeContextHook = nullptr;
        delete g_skillActionCallsiteHook;
        g_skillActionCallsiteHook = nullptr;
        g_modContext = nullptr;
    }

    void Activate(ModContext* modContext) override
    {
        bool ok = true;
        if (g_skillTreeContextHook != nullptr)
            ok = g_skillTreeContextHook->activate() && ok;
        if (g_skillNodeStatusHook != nullptr)
            ok = g_skillNodeStatusHook->activate() && ok;
        if (g_drawNodeLinkHook != nullptr)
            ok = g_drawNodeLinkHook->activate() && ok;
        if (g_skillActionCallsiteHook != nullptr)
            ok = g_skillActionCallsiteHook->activate() && ok;
        EnsurePresetUiThread();
        active = ok;
        modContext->Log(ok ? "[TalentPresets] live talent preset panel activated" : "[TalentPresets] live talent preset panel failed to activate");
    }

    void Deactivate(ModContext* modContext) override
    {
        if (g_skillNodeStatusHook != nullptr && g_skillNodeStatusHook->active)
            g_skillNodeStatusHook->deactivate();
        if (g_drawNodeLinkHook != nullptr && g_drawNodeLinkHook->active)
            g_drawNodeLinkHook->deactivate();
        if (g_skillTreeContextHook != nullptr && g_skillTreeContextHook->active)
            g_skillTreeContextHook->deactivate();
        if (g_skillActionCallsiteHook != nullptr && g_skillActionCallsiteHook->active)
            g_skillActionCallsiteHook->deactivate();
        if (g_presetWindow != nullptr)
            PostMessageW(g_presetWindow, WM_TALENT_PRESETS_SHOW, 0, 0);
        InterlockedExchange(&g_presetUiVisible, 0);
        active = false;
        modContext->Log("[TalentPresets] live injector deactivated");
    }

    void Update(ModContext* modContext) override
    {
        const bool logHoveredNodes = modContext->config.GetBool(g_metaData.name.c_str(), "log_hovered_nodes", true);
        const int logIntervalMs = modContext->config.GetInt(g_metaData.name.c_str(), "log_interval_ms", 1000);
        const bool canLogHoveredNodes = logHoveredNodes;
        const DWORD now = GetTickCount();
        if (canLogHoveredNodes && now - g_lastLogTick >= static_cast<DWORD>(logIntervalMs))
        {
            const std::uint32_t rawNodeId = CurrentNodeId();
            const LONG hitCount = g_nodeHitCount;
            if (rawNodeId != g_lastLoggedNodeId || hitCount != g_lastLoggedHitCount)
            {
                LogCurrentNode(false);
                g_lastLoggedNodeId = rawNodeId;
                g_lastLoggedHitCount = hitCount;
            }
            g_lastLogTick = now;
        }

        if ((GetAsyncKeyState(VK_F7) & 1) != 0)
        {
            LogCurrentNode(true);
            LogTalentContext();
            DumpLiveTreeNodes();
            DumpSamples();
            LogKnownTalentLookup(TALENT_ID_TALAR_RIGHT_3PT);
            LogKnownTalentLookup(TALENT_ID_GRAPPLE);
            LogKnownTalentLookup(TALENT_ID_TALAR_LEFT_3PT);
        }

        ProcessPresetJob(now);

        if ((GetAsyncKeyState(VK_F4) & 1) != 0)
        {
            if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0)
            {
                Log("[TalentPresets] Alt+F4 ignored by preset panel");
            }
            else
            {
                Log("[TalentPresets] F4 pressed");
                TogglePresetOverlay();
            }
        }

        if ((GetAsyncKeyState(VK_F8) & 1) != 0)
        {
            Log("[TalentPresets] F8 pressed");
            QueueHoveredNodeToggle();
        }

        if ((GetAsyncKeyState(VK_F9) & 1) != 0)
        {
            Log("[TalentPresets] F9 pressed");
            QueueLastHoveredNode(TALENT_MESSAGE_LEARN, "force-learn");
        }

        if ((GetAsyncKeyState(VK_F10) & 1) != 0)
        {
            Log("[TalentPresets] F10 pressed");
            QueueLastHoveredNode(TALENT_MESSAGE_UNLEARN, "force-unlearn");
        }

        if ((GetAsyncKeyState(VK_F11) & 1) != 0)
        {
            Log("[TalentPresets] F11 pressed");
            ArmPendingCurrentTalentAction(1, "pending-current-learn");
        }

        if ((GetAsyncKeyState(VK_F6) & 1) != 0)
        {
            Log("[TalentPresets] F6 pressed");
            LogKnownTalentLookup(TALENT_ID_TALAR_RIGHT_3PT);
            LogKnownTalentLookup(TALENT_ID_GRAPPLE);
            LogKnownTalentLookup(TALENT_ID_TALAR_LEFT_3PT);
        }

        if ((GetAsyncKeyState(VK_F12) & 1) != 0)
        {
            Log("[TalentPresets] F12 pressed");
            ArmPendingCurrentTalentAction(4, "pending-current-unlearn", true);
        }
    }

    ModMetaData GetMetaData() override
    {
        return g_metaData;
    }
};

extern "C" __declspec(dllexport) Mod* CreateModInstance()
{
    return new TalentPresetsMod();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_moduleHandle = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}
