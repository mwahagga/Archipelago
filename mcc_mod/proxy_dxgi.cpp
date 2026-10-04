#include <windows.h>
#include <dxgi.h>

// Typedefs for the real DXGI exports from C:\Windows\System32\dxgi.dll
typedef HRESULT(WINAPI* CreateDXGIFactory_t)(REFIID, void**);
typedef HRESULT(WINAPI* CreateDXGIFactory1_t)(REFIID, void**);
typedef HRESULT(WINAPI* CreateDXGIFactory2_t)(UINT, REFIID, void**);
typedef HRESULT(WINAPI* DXGIDeclareAdapterRemovalSupport_t)();

static HMODULE g_realDxgi = nullptr;
static CreateDXGIFactory_t g_realCreateDXGIFactory = nullptr;
static CreateDXGIFactory1_t g_realCreateDXGIFactory1 = nullptr;
static CreateDXGIFactory2_t g_realCreateDXGIFactory2 = nullptr;
static DXGIDeclareAdapterRemovalSupport_t g_realDXGIDeclareAdapterRemovalSupport = nullptr;

bool LoadRealDXGI() {
    if (g_realDxgi) return true;

    wchar_t systemPath[MAX_PATH];
    if (GetSystemDirectoryW(systemPath, MAX_PATH) == 0) return false;

    wchar_t dxgiPath[MAX_PATH];
    wsprintfW(dxgiPath, L"%s\\dxgi.dll", systemPath);

    g_realDxgi = LoadLibraryW(dxgiPath);
    if (!g_realDxgi) return false;

    g_realCreateDXGIFactory = (CreateDXGIFactory_t)GetProcAddress(g_realDxgi, "CreateDXGIFactory");
    g_realCreateDXGIFactory1 = (CreateDXGIFactory1_t)GetProcAddress(g_realDxgi, "CreateDXGIFactory1");
    g_realCreateDXGIFactory2 = (CreateDXGIFactory2_t)GetProcAddress(g_realDxgi, "CreateDXGIFactory2");
    g_realDXGIDeclareAdapterRemovalSupport = (DXGIDeclareAdapterRemovalSupport_t)GetProcAddress(g_realDxgi, "DXGIDeclareAdapterRemovalSupport");

    return (g_realCreateDXGIFactory1 != nullptr);
}

void FreeRealDXGI() {
    if (g_realDxgi) {
        FreeLibrary(g_realDxgi);
        g_realDxgi = nullptr;
        g_realCreateDXGIFactory = nullptr;
        g_realCreateDXGIFactory1 = nullptr;
        g_realCreateDXGIFactory2 = nullptr;
        g_realDXGIDeclareAdapterRemovalSupport = nullptr;
    }
}

// Exported forwarders
extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory(REFIID riid, void** ppFactory) {
    if (!LoadRealDXGI() || !g_realCreateDXGIFactory) return E_FAIL;
    return g_realCreateDXGIFactory(riid, ppFactory);
}

extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory) {
    if (!LoadRealDXGI() || !g_realCreateDXGIFactory1) return E_FAIL;
    return g_realCreateDXGIFactory1(riid, ppFactory);
}

extern "C" __declspec(dllexport) HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory) {
    if (!LoadRealDXGI() || !g_realCreateDXGIFactory2) return E_FAIL;
    return g_realCreateDXGIFactory2(Flags, riid, ppFactory);
}

extern "C" __declspec(dllexport) HRESULT WINAPI DXGIDeclareAdapterRemovalSupport() {
    if (!LoadRealDXGI() || !g_realDXGIDeclareAdapterRemovalSupport) return E_FAIL;
    return g_realDXGIDeclareAdapterRemovalSupport();
}
