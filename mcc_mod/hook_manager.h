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

} // namespace Halo3AP
