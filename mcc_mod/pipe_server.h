#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <atomic>

namespace Halo3AP {

struct ModState {
    std::mutex mtx;
    std::set<std::string> unlockedMissions;
    std::set<std::string> unlockedWeapons;
    int weaponLicenseMode = 1; // 0 = disabled, 1 = individual, 2 = tiers
    std::string startingWeapon = "assault_rifle";
    bool pipeConnected = false;
    bool tcpConnected = false;
    std::string lastBlockedMission = "";

    // Events detected by in-game hooks (rally points, terminals, skulls).
    // Written by hook detours, consumed by pipe server when client polls.
    std::set<std::string> pendingEvents;
};

extern ModState g_modState;
extern std::atomic<float> g_maxShieldFraction;
extern std::atomic<int> g_pendingDeathLinkEffect; // 0=None, 1=Kill, 2=ShieldBreak, 3=Mortal

// Thread entry point for the Named Pipe IPC server (Windows)
DWORD WINAPI PipeServerThread(LPVOID lpParam);

// Thread entry point for the Localhost TCP IPC server (Universal Windows / Linux Proton)
DWORD WINAPI TcpServerThread(LPVOID lpParam);

// Helper function to check if an Archipelago client is currently connected
bool IsClientConnected();

// Helper function to check if the player has at least one Halo 3 level unlocked
bool HasHalo3Unlocked();

// Helper function to check if a scenario is allowed to load
bool IsMissionUnlocked(const std::string& scenario);

// Helper function to check if a weapon is unlocked by canonical name
bool IsWeaponUnlocked(const std::string& weaponName);

// Log message to Halo3AP_mod.log
void Log(const char* format, ...);

} // namespace Halo3AP
