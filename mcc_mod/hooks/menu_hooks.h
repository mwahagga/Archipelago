#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Halo3AP {

    // Hooks Scaleform UIWidgetArray::AddItem to filter locked campaign mission cards
    bool InstallMenuHooks();

    // Removes Menu hooks
    void RemoveMenuHooks();

} // namespace Halo3AP
