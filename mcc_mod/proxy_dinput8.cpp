#include <windows.h>
#include <unknwn.h>

// Typedef for the real DirectInput8Create from C:\Windows\System32\dinput8.dll
typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
static DirectInput8Create_t g_realDirectInput8Create = nullptr;
static HMODULE g_realDinput8 = nullptr;

bool LoadRealDInput8() {
    if (g_realDinput8) return true;

    wchar_t systemPath[MAX_PATH];
    if (GetSystemDirectoryW(systemPath, MAX_PATH) == 0) return false;

    wchar_t dinput8Path[MAX_PATH];
    wsprintfW(dinput8Path, L"%s\\dinput8.dll", systemPath);

    g_realDinput8 = LoadLibraryW(dinput8Path);
    if (!g_realDinput8) return false;

    g_realDirectInput8Create = (DirectInput8Create_t)GetProcAddress(g_realDinput8, "DirectInput8Create");
    return (g_realDirectInput8Create != nullptr);
}

void FreeRealDInput8() {
    if (g_realDinput8) {
        FreeLibrary(g_realDinput8);
        g_realDinput8 = nullptr;
        g_realDirectInput8Create = nullptr;
    }
}

// Exported DirectInput8Create forwarding call directly to system DLL
extern "C" __declspec(dllexport) HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst,
    DWORD dwVersion,
    REFIID riidltf,
    LPVOID* ppvOut,
    LPUNKNOWN punkOuter
) {
    if (!LoadRealDInput8()) {
        return E_FAIL;
    }
    return g_realDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
}
