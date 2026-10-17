#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <string>

namespace Halo3AP {

// Installs in-engine hooks on MCC-Win64-Shipping.exe
bool InstallHooks();

// Removes in-engine hooks
void RemoveHooks();

// Signals the background monitor thread to terminate immediately (used during process detach)
void StopMonitor();

} // namespace Halo3AP
