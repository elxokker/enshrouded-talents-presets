#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cwchar>

using MiniDumpType = int;
using MiniDumpExceptionInformation = void*;
using MiniDumpUserStreamInformation = void*;
using MiniDumpCallbackInformation = void*;

namespace {

constexpr std::uintptr_t kFreeUnlearnRva = 0xA5F64;
constexpr unsigned char kOriginalJump = 0x75;
constexpr unsigned char kPatchedJump = 0xEB;

HMODULE g_realDbgHelp = nullptr;

void WriteLog(const char* message)
{
    char modulePath[MAX_PATH]{};
    if (!GetModuleFileNameA(nullptr, modulePath, MAX_PATH)) {
        return;
    }

    char* lastSlash = nullptr;
    for (char* p = modulePath; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            lastSlash = p;
        }
    }
    if (lastSlash) {
        *(lastSlash + 1) = '\0';
    }

    char logPath[MAX_PATH]{};
    std::snprintf(logPath, sizeof(logPath), "%sfree_talent_unlearn_proxy.log", modulePath);

    SYSTEMTIME now{};
    GetLocalTime(&now);

    FILE* file = nullptr;
    if (fopen_s(&file, logPath, "ab") != 0 || !file) {
        return;
    }

    std::fprintf(
        file,
        "[%04u-%02u-%02u %02u:%02u:%02u] %s\r\n",
        now.wYear,
        now.wMonth,
        now.wDay,
        now.wHour,
        now.wMinute,
        now.wSecond,
        message);
    std::fclose(file);
}

bool PatchFreeUnlearnCost()
{
    HMODULE exe = GetModuleHandleW(nullptr);
    if (!exe) {
        WriteLog("ERROR: GetModuleHandle(nullptr) failed.");
        return false;
    }

    auto* target = reinterpret_cast<unsigned char*>(reinterpret_cast<std::uintptr_t>(exe) + kFreeUnlearnRva);

    if (*target == kPatchedJump) {
        WriteLog("OK: free-unlearn patch was already active.");
        return true;
    }

    if (*target != kOriginalJump) {
        char msg[160]{};
        std::snprintf(
            msg,
            sizeof(msg),
            "ERROR: unexpected byte at RVA 0x%llX. Expected 0x%02X, found 0x%02X. Patch not applied.",
            static_cast<unsigned long long>(kFreeUnlearnRva),
            kOriginalJump,
            *target);
        WriteLog(msg);
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        WriteLog("ERROR: VirtualProtect failed.");
        return false;
    }

    *target = kPatchedJump;
    FlushInstructionCache(GetCurrentProcess(), target, 1);

    DWORD ignored = 0;
    VirtualProtect(target, 1, oldProtect, &ignored);

    WriteLog("OK: free-unlearn patch applied in memory. Individual talent unlearn should not consume runes.");
    return true;
}

DWORD WINAPI PatchThread(LPVOID)
{
    PatchFreeUnlearnCost();
    return 0;
}

HMODULE LoadRealDbgHelp()
{
    if (g_realDbgHelp) {
        return g_realDbgHelp;
    }

    wchar_t systemDir[MAX_PATH]{};
    if (!GetSystemDirectoryW(systemDir, MAX_PATH)) {
        WriteLog("ERROR: GetSystemDirectoryW failed while loading real dbghelp.dll.");
        return nullptr;
    }

    wchar_t realPath[MAX_PATH]{};
    std::swprintf(realPath, MAX_PATH, L"%s\\dbghelp.dll", systemDir);

    g_realDbgHelp = LoadLibraryW(realPath);
    if (!g_realDbgHelp) {
        WriteLog("ERROR: could not load real System32 dbghelp.dll.");
    }

    return g_realDbgHelp;
}

} // namespace

extern "C" __declspec(dllexport) BOOL WINAPI MiniDumpWriteDump(
    HANDLE hProcess,
    DWORD processId,
    HANDLE hFile,
    MiniDumpType dumpType,
    MiniDumpExceptionInformation exceptionParam,
    MiniDumpUserStreamInformation userStreamParam,
    MiniDumpCallbackInformation callbackParam)
{
    using Fn = BOOL(WINAPI*)(
        HANDLE,
        DWORD,
        HANDLE,
        MiniDumpType,
        MiniDumpExceptionInformation,
        MiniDumpUserStreamInformation,
        MiniDumpCallbackInformation);

    HMODULE realDbgHelp = LoadRealDbgHelp();
    if (!realDbgHelp) {
        return FALSE;
    }

    auto realMiniDumpWriteDump = reinterpret_cast<Fn>(GetProcAddress(realDbgHelp, "MiniDumpWriteDump"));
    if (!realMiniDumpWriteDump) {
        WriteLog("ERROR: real MiniDumpWriteDump export not found.");
        return FALSE;
    }

    return realMiniDumpWriteDump(
        hProcess,
        processId,
        hFile,
        dumpType,
        exceptionParam,
        userStreamParam,
        callbackParam);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);

        HANDLE thread = CreateThread(nullptr, 0, PatchThread, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        } else {
            PatchFreeUnlearnCost();
        }
    }

    return TRUE;
}
