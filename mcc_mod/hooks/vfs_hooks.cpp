#include "vfs_hooks.h"
#include "../pipe_server.h"

#include <MinHook.h>
#include <algorithm>
#include <mutex>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <thread>
#include <chrono>

namespace Halo3AP {

CreateFileW_t g_originalCreateFileW = nullptr;
void*         g_targetCreateFileW   = nullptr;

CreateFileA_t g_originalCreateFileA = nullptr;
void*         g_targetCreateFileA   = nullptr;

ReadFile_t    g_originalReadFile    = nullptr;
void*         g_targetReadFile      = nullptr;

GetOverlappedResult_t  g_originalGetOverlappedResult  = nullptr;
void*                  g_targetGetOverlappedResult    = nullptr;

CreateFileMappingW_t   g_originalCreateFileMappingW   = nullptr;
void*                  g_targetCreateFileMappingW     = nullptr;

MapViewOfFile_t        g_originalMapViewOfFile        = nullptr;
void*                  g_targetMapViewOfFile          = nullptr;

static std::wstring s_modDir;
static std::mutex s_vfsMutex;
static std::unordered_set<std::wstring> s_loggedFiles;

static std::mutex s_pakMutex;
static std::unordered_set<HANDLE> s_pakHandles;

static std::mutex s_mapMutex;
static std::unordered_set<HANDLE> s_pakMappings;

static std::mutex s_mapFileMutex;
static std::unordered_map<HANDLE, std::wstring> s_mapFileHandles;
static std::unordered_map<HANDLE, std::wstring> s_mapFileMappings;

struct PendingRead {
    HANDLE   hFile;
    LPVOID   lpBuffer;
    DWORD    nNumberOfBytesToRead;
    uint64_t fileOffset;
};
static std::mutex s_pendingMutex;
static std::map<LPOVERLAPPED, PendingRead> s_pendingReads;

static void InitModDirectory() {
    if (!s_modDir.empty()) return;
    HMODULE hMod = nullptr;
    GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&InstallVfsHooks,
        &hMod
    );
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(hMod, path, MAX_PATH)) {
        wchar_t* lastSlash = wcsrchr(path, L'\\');
        if (lastSlash) {
            *lastSlash = L'\0';
            s_modDir = path;
        }
    }
}

static std::wstring ToLower(const std::wstring& str) {
    std::wstring lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    return lower;
}

static std::wstring GetBasename(const std::wstring& fullPath) {
    size_t lastSlash = fullPath.find_last_of(L"\\/");
    if (lastSlash == std::wstring::npos) {
        return fullPath;
    }
    return fullPath.substr(lastSlash + 1);
}

static bool IsUiCandidate(const std::wstring& lower) {
    if (lower.find(L"screens") != std::wstring::npos) return true;
    if (lower.find(L"\\ui\\") != std::wstring::npos || lower.find(L"/ui/") != std::wstring::npos) return true;
    if (lower.find(L"mainmenu") != std::wstring::npos) return true;
    if (lower.find(L".gfx") != std::wstring::npos) return true;
    if (lower.find(L".swf") != std::wstring::npos) return true;
    return false;
}

static std::wstring FindOverride(const std::wstring& originalPath) {
    // Disk-based Scaleform overrides disabled to prevent UI desync with UE4 Slate.
    // Menu customizations (skipping submenus, hiding unused modes) are handled cleanly
    // via in-flight memory byte patching in PatchPakBuffer().
    return L"";
}

HANDLE WINAPI Detour_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    if (!lpFileName || !g_originalCreateFileW) {
        return g_originalCreateFileW ? g_originalCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                                                            lpSecurityAttributes, dwCreationDisposition,
                                                            dwFlagsAndAttributes, hTemplateFile)
                                     : INVALID_HANDLE_VALUE;
    }

    std::wstring originalPath(lpFileName);
    std::wstring lowerPath = ToLower(originalPath);

    if (IsUiCandidate(lowerPath)) {
        std::wstring overridePath = FindOverride(originalPath);

        if (!overridePath.empty()) {
            {
                std::lock_guard<std::mutex> lock(s_vfsMutex);
                if (s_loggedFiles.find(lowerPath) == s_loggedFiles.end()) {
                    s_loggedFiles.insert(lowerPath);
                    Log("[VFS] >>> REDIRECTING (W): %ls -> %ls", lpFileName, overridePath.c_str());
                }
            }
            return g_originalCreateFileW(
                overridePath.c_str(),
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile
            );
        } else {
            std::lock_guard<std::mutex> lock(s_vfsMutex);
            if (s_loggedFiles.find(lowerPath) == s_loggedFiles.end()) {
                s_loggedFiles.insert(lowerPath);
                Log("[VFS] Pass-through (W): %ls", lpFileName);
            }
        }
    }

    HANDLE hFile = g_originalCreateFileW(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        if (lowerPath.find(L"mcc-windowsnoeditor.pak") != std::wstring::npos) {
            std::lock_guard<std::mutex> lock(s_pakMutex);
            s_pakHandles.insert(hFile);
            Log("[VFS] Captured MCC-WindowsNoEditor.pak handle (W): %p (access=0x%08X, share=0x%08X, flags=0x%08X)",
                hFile, dwDesiredAccess, dwShareMode, dwFlagsAndAttributes);
        } else if (lowerPath.find(L".map") != std::wstring::npos) {
            std::wstring baseName = GetBasename(lowerPath);
            std::lock_guard<std::mutex> lock(s_mapFileMutex);
            s_mapFileHandles[hFile] = baseName;
            Log("[VFS] Captured map file handle (W): %p (%ls)", hFile, baseName.c_str());
        }
    }

    return hFile;
}

HANDLE WINAPI Detour_CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    if (!lpFileName || !g_originalCreateFileA) {
        return g_originalCreateFileA ? g_originalCreateFileA(lpFileName, dwDesiredAccess, dwShareMode,
                                                            lpSecurityAttributes, dwCreationDisposition,
                                                            dwFlagsAndAttributes, hTemplateFile)
                                     : INVALID_HANDLE_VALUE;
    }

    int len = MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, nullptr, 0);
    std::wstring originalPath(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, lpFileName, -1, &originalPath[0], len);
    if (!originalPath.empty() && originalPath.back() == L'\0') originalPath.pop_back();

    std::wstring lowerPath = ToLower(originalPath);

    if (IsUiCandidate(lowerPath)) {
        std::wstring overridePath = FindOverride(originalPath);

        if (!overridePath.empty() && g_originalCreateFileW) {
            {
                std::lock_guard<std::mutex> lock(s_vfsMutex);
                if (s_loggedFiles.find(lowerPath) == s_loggedFiles.end()) {
                    s_loggedFiles.insert(lowerPath);
                    Log("[VFS] >>> REDIRECTING (A->W): %s -> %ls", lpFileName, overridePath.c_str());
                }
            }
            return g_originalCreateFileW(
                overridePath.c_str(),
                dwDesiredAccess,
                dwShareMode,
                lpSecurityAttributes,
                dwCreationDisposition,
                dwFlagsAndAttributes,
                hTemplateFile
            );
        } else {
            std::lock_guard<std::mutex> lock(s_vfsMutex);
            if (s_loggedFiles.find(lowerPath) == s_loggedFiles.end()) {
                s_loggedFiles.insert(lowerPath);
                Log("[VFS] Pass-through (A): %s", lpFileName);
            }
        }
    }

    HANDLE hFile = g_originalCreateFileA(
        lpFileName,
        dwDesiredAccess,
        dwShareMode,
        lpSecurityAttributes,
        dwCreationDisposition,
        dwFlagsAndAttributes,
        hTemplateFile
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        if (lowerPath.find(L"mcc-windowsnoeditor.pak") != std::wstring::npos) {
            std::lock_guard<std::mutex> lock(s_pakMutex);
            s_pakHandles.insert(hFile);
            Log("[VFS] Captured MCC-WindowsNoEditor.pak handle (A): %p (access=0x%08X, share=0x%08X, flags=0x%08X)",
                hFile, dwDesiredAccess, dwShareMode, dwFlagsAndAttributes);
        } else if (lowerPath.find(L".map") != std::wstring::npos) {
            std::wstring baseName = GetBasename(lowerPath);
            std::lock_guard<std::mutex> lock(s_mapFileMutex);
            s_mapFileHandles[hFile] = baseName;
            Log("[VFS] Captured map file handle (A): %p (%ls)", hFile, baseName.c_str());
        }
    }

    return hFile;
}

static void PatchPakBuffer(const char* sourceName, uint8_t* pBuf, uint64_t fileOffset, DWORD bytesRead) {
    if (!pBuf || bytesRead == 0) return;

    constexpr uint64_t PAK_UEXP_BASE = 0x26193EC19ULL;
    constexpr uint64_t PAK_UEXP_SIZE = 0x360D0ULL;

    uint64_t readEnd = fileOffset + bytesRead;
    if (fileOffset < PAK_UEXP_BASE + PAK_UEXP_SIZE && readEnd > PAK_UEXP_BASE) {
        Log("[VFS] >>> %s: Reading DT_MainMenuData range! [0x%llX .. 0x%llX] (%u bytes)",
            sourceName, (unsigned long long)fileOffset, (unsigned long long)readEnd, bytesRead);
    }

    struct MenuPatch {
        const char* name;
        uint64_t    pakOffset;
        uint8_t     origVal;
        uint8_t     patchVal;
    };

    static const MenuPatch s_patches[] = {
        // Hide unused Main Menu modes (Row3, Row4, Row5: PlatformVisibility = XBOX_ONLY)
        {"Hide_Multiplayer",              PAK_UEXP_BASE + 0x0A8D, 0x58, 0x5A},
        {"Hide_Creative",                 PAK_UEXP_BASE + 0x0E76, 0x58, 0x5A},
        {"Hide_Firefight",                PAK_UEXP_BASE + 0x1268, 0x58, 0x5A},

        // Halo 3 (Row13 / Row39): Bypasses intermediate submenus directly to Mission Select
        {"H3_Carousel_DirectMissions",    PAK_UEXP_BASE + 0x2F61, 0x70, 0x72},
        {"H3_Carousel_FeatureID",         PAK_UEXP_BASE + 0x31EE, 0x2C, 0x17},
        {"H3_Submenu_SwapMenuType",       PAK_UEXP_BASE + 0x9693, 0x72, 0x70},

        // Hide Cross-Game Playlists from Carousel (Row16: PlatformVisibility = XBOX_ONLY)
        {"Hide_CrossGamePlaylists",       PAK_UEXP_BASE + 0x3DB4, 0x58, 0x5A},
    };

    for (const auto& p : s_patches) {
        if (p.pakOffset >= fileOffset && p.pakOffset < readEnd) {
            size_t bufIdx = (size_t)(p.pakOffset - fileOffset);
            uint8_t currentVal = pBuf[bufIdx];

            if (currentVal == p.origVal) {
                pBuf[bufIdx] = p.patchVal;
                Log("[VFS] >>> [%s] In-flight PAK patched: %s at pak+0x%llX (0x%02X -> 0x%02X)!",
                    sourceName, p.name, (unsigned long long)p.pakOffset, p.origVal, p.patchVal);
            } else if (currentVal == p.patchVal) {
                Log("[VFS] [%s] Already patched: %s at pak+0x%llX (0x%02X)",
                    sourceName, p.name, (unsigned long long)p.pakOffset, currentVal);
            } else {
                Log("[VFS] [!] [%s] Unexpected byte for %s at pak+0x%llX: 0x%02X (expected 0x%02X)",
                    sourceName, p.name, (unsigned long long)p.pakOffset, currentVal, p.origVal);
            }
        }
    }
}

struct MapSkullPatch {
    const wchar_t* mapFileName;
    const char*    skullName;
    uint64_t       flagOffset;
    uint32_t       origVal;
    uint32_t       patchVal;
};

static const MapSkullPatch s_mapSkullPatches[] = {
    // Sierra 117 (010_jungle.map)
    { L"010_jungle.map",    "Iron Skull",            0x151C12FCULL, 0x21, 0x20 },
    { L"010_jungle.map",    "Blind Skull",           0x151C13A4ULL, 0x21, 0x20 },

    // Crow's Nest (020_base.map)
    { L"020_base.map",      "Grunt Birthday Skull",  0x1667755CULL, 0x21, 0x20 },
    { L"020_base.map",      "Black Eye Skull",       0x16677604ULL, 0x21, 0x20 },

    // Tsavo Highway (030_outskirts.map)
    { L"030_outskirts.map", "Tough Luck Skull",      0x15FD5544ULL, 0x21, 0x20 },

    // The Storm (040_voi.map)
    { L"040_voi.map",       "Catch Skull",           0x17B9FB58ULL, 0x21, 0x20 },

    // Floodgate (050_floodvoi.map)
    { L"050_floodvoi.map",  "Fog Skull",             0x167FDD7CULL, 0x21, 0x20 },

    // The Ark (070_waste.map)
    { L"070_waste.map",     "Famine Skull",          0x1C0D3C14ULL, 0x21, 0x20 },
    { L"070_waste.map",     "Cowbell Skull",         0x1C0D3CBCULL, 0x21, 0x20 },

    // The Covenant (100_citadel.map)
    { L"100_citadel.map",   "Thunderstorm Skull",    0x1E6D9C5CULL, 0x21, 0x20 },
    { L"100_citadel.map",   "IWHBYD Skull",          0x1E6D9D04ULL, 0x21, 0x20 },

    // Cortana (110_hc.map)
    { L"110_hc.map",        "Tilt Skull",            0x0CFE8308ULL, 0x21, 0x20 },
    { L"110_hc.map",        "Blind Skull",           0x0CFE83B0ULL, 0x61, 0x60 },
};

static void PatchMapBuffer(const wchar_t* mapName, const char* sourceName, uint8_t* pBuf, uint64_t fileOffset, DWORD bytesRead) {
    if (!pBuf || bytesRead == 0 || !mapName) return;

    uint64_t readEnd = fileOffset + bytesRead;

    for (const auto& p : s_mapSkullPatches) {
        if (_wcsicmp(p.mapFileName, mapName) == 0) {
            if (p.flagOffset >= fileOffset && p.flagOffset + sizeof(uint32_t) <= readEnd) {
                size_t bufIdx = (size_t)(p.flagOffset - fileOffset);
                uint32_t* pVal = (uint32_t*)(pBuf + bufIdx);
                uint32_t currentVal = *pVal;

                if (currentVal == p.origVal) {
                    *pVal = p.patchVal;
                    Log("[VFS] >>> [%s] In-flight MAP patched: %ls %s at off=0x%llX (0x%08X -> 0x%08X)!",
                        sourceName, mapName, p.skullName, (unsigned long long)p.flagOffset, p.origVal, p.patchVal);
                } else if (currentVal == p.patchVal) {
                    Log("[VFS] [%s] MAP skull already patched: %ls %s at off=0x%llX (0x%08X)",
                        sourceName, mapName, p.skullName, (unsigned long long)p.flagOffset, currentVal);
                } else {
                    Log("[VFS] [!] [%s] Unexpected byte for %ls %s at off=0x%llX: 0x%08X (expected 0x%08X)",
                        sourceName, mapName, p.skullName, (unsigned long long)p.flagOffset, currentVal, p.origVal);
                }
            }
        }
    }
}

BOOL WINAPI Detour_ReadFile(
    HANDLE       hFile,
    LPVOID       lpBuffer,
    DWORD        nNumberOfBytesToRead,
    LPDWORD      lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
) {
    if (!g_originalReadFile) return FALSE;

    bool isPak = false;
    {
        std::lock_guard<std::mutex> lock(s_pakMutex);
        isPak = (s_pakHandles.find(hFile) != s_pakHandles.end());
    }

    std::wstring mapFileName;
    {
        std::lock_guard<std::mutex> lock(s_mapFileMutex);
        auto it = s_mapFileHandles.find(hFile);
        if (it != s_mapFileHandles.end()) {
            mapFileName = it->second;
        }
    }
    bool isMap = !mapFileName.empty();

    uint64_t fileOffset = 0;
    if (isPak || isMap) {
        if (lpOverlapped) {
            fileOffset = ((uint64_t)lpOverlapped->OffsetHigh << 32) | lpOverlapped->Offset;
        } else {
            LARGE_INTEGER zero;
            zero.QuadPart = 0;
            LARGE_INTEGER curPos;
            curPos.QuadPart = 0;
            if (SetFilePointerEx(hFile, zero, &curPos, FILE_CURRENT)) {
                fileOffset = (uint64_t)curPos.QuadPart;
            }
        }

        if (lpOverlapped && lpBuffer) {
            std::lock_guard<std::mutex> lock(s_pendingMutex);
            if (s_pendingReads.size() > 512) {
                s_pendingReads.clear();
            }
            s_pendingReads[lpOverlapped] = { hFile, lpBuffer, nNumberOfBytesToRead, fileOffset };
        }
    }

    BOOL result = g_originalReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    DWORD dwErr = result ? 0 : GetLastError();

    if (isPak) {
        static int s_logPakReads = 0;
        if (++s_logPakReads <= 20 || (fileOffset >= 0x261900000ULL && fileOffset <= 0x261A00000ULL)) {
            Log("[VFS] PAK Read #%d: off=0x%llX, req=%u, ov=%p, res=%d, err=%u",
                s_logPakReads, (unsigned long long)fileOffset, nNumberOfBytesToRead, lpOverlapped, result, dwErr);
        }

        if (result && lpBuffer) {
            DWORD bytesRead = lpNumberOfBytesRead ? *lpNumberOfBytesRead : nNumberOfBytesToRead;
            PatchPakBuffer("ReadFileSync", (uint8_t*)lpBuffer, fileOffset, bytesRead);
            if (lpOverlapped) {
                std::lock_guard<std::mutex> lock(s_pendingMutex);
                s_pendingReads.erase(lpOverlapped);
            }
        } else if (!result && dwErr != ERROR_IO_PENDING && lpOverlapped) {
            std::lock_guard<std::mutex> lock(s_pendingMutex);
            s_pendingReads.erase(lpOverlapped);
        }
    } else if (isMap) {
        static int s_logMapReads = 0;
        if (++s_logMapReads <= 20) {
            Log("[VFS] MAP Read (%ls) #%d: off=0x%llX, req=%u, ov=%p, res=%d, err=%u",
                mapFileName.c_str(), s_logMapReads, (unsigned long long)fileOffset, nNumberOfBytesToRead, lpOverlapped, result, dwErr);
        }

        if (result && lpBuffer) {
            DWORD bytesRead = lpNumberOfBytesRead ? *lpNumberOfBytesRead : nNumberOfBytesToRead;
            PatchMapBuffer(mapFileName.c_str(), "ReadFileSync", (uint8_t*)lpBuffer, fileOffset, bytesRead);
            if (lpOverlapped) {
                std::lock_guard<std::mutex> lock(s_pendingMutex);
                s_pendingReads.erase(lpOverlapped);
            }
        } else if (!result && dwErr != ERROR_IO_PENDING && lpOverlapped) {
            std::lock_guard<std::mutex> lock(s_pendingMutex);
            s_pendingReads.erase(lpOverlapped);
        }
    }

    if (!result) SetLastError(dwErr);
    return result;
}

BOOL WINAPI Detour_GetOverlappedResult(
    HANDLE       hFile,
    LPOVERLAPPED lpOverlapped,
    LPDWORD      lpNumberOfBytesTransferred,
    BOOL         bWait
) {
    if (!g_originalGetOverlappedResult) return FALSE;

    BOOL result = g_originalGetOverlappedResult(hFile, lpOverlapped, lpNumberOfBytesTransferred, bWait);
    if (result && lpOverlapped) {
        PendingRead pr = {};
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(s_pendingMutex);
            auto it = s_pendingReads.find(lpOverlapped);
            if (it != s_pendingReads.end()) {
                pr = it->second;
                found = true;
                s_pendingReads.erase(it);
            }
        }
        if (found && pr.lpBuffer) {
            DWORD transferred = lpNumberOfBytesTransferred ? *lpNumberOfBytesTransferred : pr.nNumberOfBytesToRead;
            bool isPak = false;
            {
                std::lock_guard<std::mutex> lock(s_pakMutex);
                isPak = (s_pakHandles.find(pr.hFile) != s_pakHandles.end());
            }
            if (isPak) {
                PatchPakBuffer("GetOverlappedResult", (uint8_t*)pr.lpBuffer, pr.fileOffset, transferred);
            } else {
                std::wstring mapName;
                {
                    std::lock_guard<std::mutex> lock(s_mapFileMutex);
                    auto it = s_mapFileHandles.find(pr.hFile);
                    if (it != s_mapFileHandles.end()) {
                        mapName = it->second;
                    }
                }
                if (!mapName.empty()) {
                    PatchMapBuffer(mapName.c_str(), "GetOverlappedResult", (uint8_t*)pr.lpBuffer, pr.fileOffset, transferred);
                }
            }
        }
    }
    return result;
}

HANDLE WINAPI Detour_CreateFileMappingW(
    HANDLE                hFile,
    LPSECURITY_ATTRIBUTES lpFileMappingAttributes,
    DWORD                 flProtect,
    DWORD                 dwMaximumSizeHigh,
    DWORD                 dwMaximumSizeLow,
    LPCWSTR               lpName
) {
    if (!g_originalCreateFileMappingW) return NULL;
    HANDLE hMap = g_originalCreateFileMappingW(hFile, lpFileMappingAttributes, flProtect,
                                               dwMaximumSizeHigh, dwMaximumSizeLow, lpName);
    if (hMap != NULL) {
        bool isPak = false;
        {
            std::lock_guard<std::mutex> lock(s_pakMutex);
            isPak = (s_pakHandles.find(hFile) != s_pakHandles.end());
        }
        if (isPak) {
            std::lock_guard<std::mutex> lock(s_mapMutex);
            s_pakMappings.insert(hMap);
            Log("[VFS] >>> CreateFileMappingW on PAK handle %p -> MapHandle %p (protect=0x%X)",
                hFile, hMap, flProtect);
        }

        std::wstring mapName;
        {
            std::lock_guard<std::mutex> lock(s_mapFileMutex);
            auto it = s_mapFileHandles.find(hFile);
            if (it != s_mapFileHandles.end()) {
                mapName = it->second;
            }
        }
        if (!mapName.empty()) {
            std::lock_guard<std::mutex> lock(s_mapFileMutex);
            s_mapFileMappings[hMap] = mapName;
            Log("[VFS] >>> CreateFileMappingW on MAP handle %p (%ls) -> MapHandle %p (protect=0x%X)",
                hFile, mapName.c_str(), hMap, flProtect);
        }
    }
    return hMap;
}

LPVOID WINAPI Detour_MapViewOfFile(
    HANDLE hFileMappingObject,
    DWORD  dwDesiredAccess,
    DWORD  dwFileOffsetHigh,
    DWORD  dwFileOffsetLow,
    SIZE_T dwNumberOfBytesToMap
) {
    if (!g_originalMapViewOfFile) return NULL;
    LPVOID pView = g_originalMapViewOfFile(hFileMappingObject, dwDesiredAccess,
                                          dwFileOffsetHigh, dwFileOffsetLow, dwNumberOfBytesToMap);
    if (pView != NULL) {
        bool isPakMap = false;
        {
            std::lock_guard<std::mutex> lock(s_mapMutex);
            isPakMap = (s_pakMappings.find(hFileMappingObject) != s_pakMappings.end());
        }
        if (isPakMap) {
            uint64_t fileOffset = ((uint64_t)dwFileOffsetHigh << 32) | dwFileOffsetLow;
            MEMORY_BASIC_INFORMATION mbi = {};
            if (VirtualQuery(pView, &mbi, sizeof(mbi))) {
                DWORD bytesMapped = (dwNumberOfBytesToMap > 0) ? (DWORD)dwNumberOfBytesToMap : (DWORD)mbi.RegionSize;
                Log("[VFS] >>> MapViewOfFile on PAK: ptr=%p, off=0x%llX, bytes=%u (protect=0x%X)",
                    pView, (unsigned long long)fileOffset, bytesMapped, mbi.Protect);

                DWORD oldProtect = 0;
                BOOL canWrite = (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
                if (!canWrite) {
                    canWrite = VirtualProtect(pView, bytesMapped, PAGE_WRITECOPY, &oldProtect);
                }
                if (canWrite) {
                    PatchPakBuffer("MapViewOfFile", (uint8_t*)pView, fileOffset, bytesMapped);
                    if (oldProtect != 0) {
                        VirtualProtect(pView, bytesMapped, oldProtect, &oldProtect);
                    }
                }
            }
        }

        std::wstring mapFileName;
        {
            std::lock_guard<std::mutex> lock(s_mapFileMutex);
            auto it = s_mapFileMappings.find(hFileMappingObject);
            if (it != s_mapFileMappings.end()) {
                mapFileName = it->second;
            }
        }
        if (!mapFileName.empty()) {
            uint64_t fileOffset = ((uint64_t)dwFileOffsetHigh << 32) | dwFileOffsetLow;
            MEMORY_BASIC_INFORMATION mbi = {};
            if (VirtualQuery(pView, &mbi, sizeof(mbi))) {
                DWORD bytesMapped = (dwNumberOfBytesToMap > 0) ? (DWORD)dwNumberOfBytesToMap : (DWORD)mbi.RegionSize;
                Log("[VFS] >>> MapViewOfFile on MAP (%ls): ptr=%p, off=0x%llX, bytes=%u (protect=0x%X)",
                    mapFileName.c_str(), pView, (unsigned long long)fileOffset, bytesMapped, mbi.Protect);

                DWORD oldProtect = 0;
                BOOL canWrite = (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
                if (!canWrite) {
                    canWrite = VirtualProtect(pView, bytesMapped, PAGE_WRITECOPY, &oldProtect);
                }
                if (canWrite) {
                    PatchMapBuffer(mapFileName.c_str(), "MapViewOfFile", (uint8_t*)pView, fileOffset, bytesMapped);
                    if (oldProtect != 0) {
                        VirtualProtect(pView, bytesMapped, oldProtect, &oldProtect);
                    }
                }
            }
        }
    }
    return pView;
}

bool InstallVfsHooks() {
    InitModDirectory();
    Log("[VFS] Initializing VFS file redirection (mod directory: %ls)...", s_modDir.c_str());

    HMODULE hKb = GetModuleHandleA("KernelBase.dll");
    if (!hKb) hKb = LoadLibraryA("KernelBase.dll");
    if (!hKb) {
        Log("[!] [VFS] Failed to locate KernelBase.dll!");
        return false;
    }

    // Hook CreateFileW
    g_targetCreateFileW = (void*)GetProcAddress(hKb, "CreateFileW");
    if (g_targetCreateFileW) {
        MH_STATUS s = MH_CreateHook(g_targetCreateFileW, (LPVOID)&Detour_CreateFileW, (LPVOID*)&g_originalCreateFileW);
        if (s == MH_OK) {
            MH_EnableHook(g_targetCreateFileW);
            Log("[+] [VFS] CreateFileW hook active at KernelBase!CreateFileW (%p)!", g_targetCreateFileW);
        } else {
            Log("[!] [VFS] MH_CreateHook CreateFileW failed: %s", MH_StatusToString(s));
        }
    }

    // Hook CreateFileA
    g_targetCreateFileA = (void*)GetProcAddress(hKb, "CreateFileA");
    if (g_targetCreateFileA) {
        MH_STATUS s = MH_CreateHook(g_targetCreateFileA, (LPVOID)&Detour_CreateFileA, (LPVOID*)&g_originalCreateFileA);
        if (s == MH_OK) {
            MH_EnableHook(g_targetCreateFileA);
            Log("[+] [VFS] CreateFileA hook active at KernelBase!CreateFileA (%p)!", g_targetCreateFileA);
        } else {
            Log("[!] [VFS] MH_CreateHook CreateFileA failed: %s", MH_StatusToString(s));
        }
    }

    // Hook ReadFile
    g_targetReadFile = (void*)GetProcAddress(hKb, "ReadFile");
    if (g_targetReadFile) {
        MH_STATUS s = MH_CreateHook(g_targetReadFile, (LPVOID)&Detour_ReadFile, (LPVOID*)&g_originalReadFile);
        if (s == MH_OK) {
            MH_EnableHook(g_targetReadFile);
            Log("[+] [VFS] ReadFile hook active at KernelBase!ReadFile (%p)!", g_targetReadFile);
        } else {
            Log("[!] [VFS] MH_CreateHook ReadFile failed: %s", MH_StatusToString(s));
        }
    }

    // Hook GetOverlappedResult
    g_targetGetOverlappedResult = (void*)GetProcAddress(hKb, "GetOverlappedResult");
    if (g_targetGetOverlappedResult) {
        MH_STATUS s = MH_CreateHook(g_targetGetOverlappedResult, (LPVOID)&Detour_GetOverlappedResult, (LPVOID*)&g_originalGetOverlappedResult);
        if (s == MH_OK) {
            MH_EnableHook(g_targetGetOverlappedResult);
            Log("[+] [VFS] GetOverlappedResult hook active at KernelBase!GetOverlappedResult (%p)!", g_targetGetOverlappedResult);
        } else {
            Log("[!] [VFS] MH_CreateHook GetOverlappedResult failed: %s", MH_StatusToString(s));
        }
    }

    // Hook CreateFileMappingW
    g_targetCreateFileMappingW = (void*)GetProcAddress(hKb, "CreateFileMappingW");
    if (g_targetCreateFileMappingW) {
        MH_STATUS s = MH_CreateHook(g_targetCreateFileMappingW, (LPVOID)&Detour_CreateFileMappingW, (LPVOID*)&g_originalCreateFileMappingW);
        if (s == MH_OK) {
            MH_EnableHook(g_targetCreateFileMappingW);
            Log("[+] [VFS] CreateFileMappingW hook active at KernelBase!CreateFileMappingW (%p)!", g_targetCreateFileMappingW);
        } else {
            Log("[!] [VFS] MH_CreateHook CreateFileMappingW failed: %s", MH_StatusToString(s));
        }
    }

    // Hook MapViewOfFile
    g_targetMapViewOfFile = (void*)GetProcAddress(hKb, "MapViewOfFile");
    if (g_targetMapViewOfFile) {
        MH_STATUS s = MH_CreateHook(g_targetMapViewOfFile, (LPVOID)&Detour_MapViewOfFile, (LPVOID*)&g_originalMapViewOfFile);
        if (s == MH_OK) {
            MH_EnableHook(g_targetMapViewOfFile);
            Log("[+] [VFS] MapViewOfFile hook active at KernelBase!MapViewOfFile (%p)!", g_targetMapViewOfFile);
        } else {
            Log("[!] [VFS] MH_CreateHook MapViewOfFile failed: %s", MH_StatusToString(s));
        }
    }

    return true;
}

void RemoveVfsHooks() {
    if (g_targetMapViewOfFile) {
        MH_DisableHook(g_targetMapViewOfFile);
        MH_RemoveHook(g_targetMapViewOfFile);
        g_targetMapViewOfFile = nullptr;
    }
    if (g_targetCreateFileMappingW) {
        MH_DisableHook(g_targetCreateFileMappingW);
        MH_RemoveHook(g_targetCreateFileMappingW);
        g_targetCreateFileMappingW = nullptr;
    }
    if (g_targetGetOverlappedResult) {
        MH_DisableHook(g_targetGetOverlappedResult);
        MH_RemoveHook(g_targetGetOverlappedResult);
        g_targetGetOverlappedResult = nullptr;
    }
    if (g_targetReadFile) {
        MH_DisableHook(g_targetReadFile);
        MH_RemoveHook(g_targetReadFile);
        g_targetReadFile = nullptr;
    }
    if (g_targetCreateFileW) {
        MH_DisableHook(g_targetCreateFileW);
        MH_RemoveHook(g_targetCreateFileW);
        g_targetCreateFileW = nullptr;
    }
    if (g_targetCreateFileA) {
        MH_DisableHook(g_targetCreateFileA);
        MH_RemoveHook(g_targetCreateFileA);
        g_targetCreateFileA = nullptr;
    }
    Log("[VFS] VFS hooks removed.");
}

} // namespace Halo3AP
