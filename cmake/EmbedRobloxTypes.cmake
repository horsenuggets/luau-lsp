# Read the Roblox definitions file and generate a C++ header with the content
# embedded as a raw string literal

set(ROBLOX_TYPES_FILE "${CMAKE_SOURCE_DIR}/scripts/globalTypes.PluginSecurity.d.luau")

if(EXISTS "${ROBLOX_TYPES_FILE}")
    file(READ "${ROBLOX_TYPES_FILE}" ROBLOX_TYPES_CONTENT)
    file(WRITE "${CMAKE_BINARY_DIR}/RobloxTypes.hpp"
        "#pragma once\n"
        "// Auto-generated from scripts/globalTypes.PluginSecurity.d.luau\n"
        "// Do not edit manually\n\n"
        "static const char* ROBLOX_GLOBAL_TYPES = R\"LUAU_DEFS(\n"
        "${ROBLOX_TYPES_CONTENT}"
        ")LUAU_DEFS\";\n"
    )
else()
    file(WRITE "${CMAKE_BINARY_DIR}/RobloxTypes.hpp"
        "#pragma once\n"
        "// Roblox definitions not found at build time\n"
        "static const char* ROBLOX_GLOBAL_TYPES = nullptr;\n"
    )
endif()
