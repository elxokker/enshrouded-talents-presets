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

constexpr unsigned char kOriginalJump = 0x75;
constexpr unsigned char kPatchedJump = 0xEB;
constexpr std::size_t kFreeUnlearnJumpOffset = 7;

struct SignatureByte {
    unsigned char value;
    bool wildcard;
};

constexpr SignatureByte kFreeUnlearnSignature[] = {
    {0xE8, false}, {0x00, true}, {0x00, true}, {0x00, true}, {0x00, true}, // call admin-mode check
    {0x84, false}, {0xC0, false},                                       // test al, al
    {0x75, false}, {0x00, true},                                        // jnz success
    {0x0F, false}, {0xB7, false}, {0xD3, false},                        // movzx edx, bx
    {0x48, false}, {0x8D, false}, {0x4D, false}, {0x00, true}           // lea rcx, [rbp+disp8]
};

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

std::uintptr_t GetModuleImageSize(HMODULE module)
{
    auto* base = reinterpret_cast<const unsigned char*>(module);
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }

    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }

    return static_cast<std::uintptr_t>(nt->OptionalHeader.SizeOfImage);
}

unsigned char* FindSignature(unsigned char* base, std::uintptr_t size, const SignatureByte* signature, std::size_t length)
{
    if (!base || size < length) {
        return nullptr;
    }

    for (std::uintptr_t offset = 0; offset <= size - length; ++offset) {
        bool matched = true;
        for (std::size_t index = 0; index < length; ++index) {
            if (!signature[index].wildcard && base[offset + index] != signature[index].value) {
                matched = false;
                break;
            }
        }

        if (matched) {
            return base + offset;
        }
    }

    return nullptr;
}

bool PatchFreeUnlearnCost()
{
    HMODULE exe = GetModuleHandleW(nullptr);
    if (!exe) {
        WriteLog("ERROR: GetModuleHandle(nullptr) failed.");
        return false;
    }

    auto* base = reinterpret_cast<unsigned char*>(exe);
    const std::uintptr_t imageSize = GetModuleImageSize(exe);
    auto* signature = FindSignature(base, imageSize, kFreeUnlearnSignature, sizeof(kFreeUnlearnSignature) / sizeof(kFreeUnlearnSignature[0]));
    if (!signature) {
        WriteLog("ERROR: free-unlearn signature not found. Patch not applied.");
        return false;
    }

    auto* target = signature + kFreeUnlearnJumpOffset;
    const std::uintptr_t targetRva = reinterpret_cast<std::uintptr_t>(target) - reinterpret_cast<std::uintptr_t>(exe);

    if (*target == kPatchedJump) {
        char msg[160]{};
        std::snprintf(
            msg,
            sizeof(msg),
            "OK: free-unlearn patch was already active at RVA 0x%llX.",
            static_cast<unsigned long long>(targetRva));
        WriteLog(msg);
        return true;
    }

    if (*target != kOriginalJump) {
        char msg[160]{};
        std::snprintf(
            msg,
            sizeof(msg),
            "ERROR: unexpected byte at signature RVA 0x%llX. Expected 0x%02X, found 0x%02X. Patch not applied.",
            static_cast<unsigned long long>(targetRva),
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

    char msg[180]{};
    std::snprintf(
        msg,
        sizeof(msg),
        "OK: free-unlearn patch applied at RVA 0x%llX. Individual talent unlearn should not consume runes.",
        static_cast<unsigned long long>(targetRva));
    WriteLog(msg);
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
