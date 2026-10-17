#pragma once

#include <windows.h>
#include <vector>
#include <string>

namespace Halo3AP {

    // Parse a pattern string like "48 89 5C 24 08 ? ? ?? 48 81 EC"
    // where "?" or "??" are wildcards.
    struct ParsedPattern {
        std::vector<uint8_t> bytes;
        std::vector<bool> mask;
    };
    ParsedPattern ParsePattern(const std::string& pattern);

    // Find a pattern in a module's loaded memory.
    // Returns the address of the first match, or nullptr if not found.
    void* FindPatternInModule(HMODULE module, const std::string& pattern);

} // namespace Halo3AP
