# Read the Roblox definitions file and generate a C++ header with the content
# embedded as concatenated raw string literals (MSVC has a 16380 char limit per literal)

set(ROBLOX_TYPES_FILE "${CMAKE_SOURCE_DIR}/scripts/globalTypes.PluginSecurity.d.luau")

if(EXISTS "${ROBLOX_TYPES_FILE}")
    file(READ "${ROBLOX_TYPES_FILE}" ROBLOX_TYPES_CONTENT)

    # Split into lines
    string(REPLACE "\n" ";" LINES "${ROBLOX_TYPES_CONTENT}")

    set(HEADER_CONTENT "#pragma once\n")
    string(APPEND HEADER_CONTENT "#include <string>\n\n")
    string(APPEND HEADER_CONTENT "// Auto-generated from scripts/globalTypes.PluginSecurity.d.luau\n")
    string(APPEND HEADER_CONTENT "// Split into chunks to stay within MSVC string literal limits\n\n")
    string(APPEND HEADER_CONTENT "inline const char* ROBLOX_GLOBAL_TYPES_INIT()\n{\n")
    string(APPEND HEADER_CONTENT "    static std::string result;\n")
    string(APPEND HEADER_CONTENT "    if (!result.empty()) return result.c_str();\n")

    set(CHUNK "")
    set(CHUNK_SIZE 0)
    set(MAX_CHUNK_SIZE 14000)

    foreach(LINE IN LISTS LINES)
        string(LENGTH "${LINE}" LINE_LEN)
        math(EXPR NEW_SIZE "${CHUNK_SIZE} + ${LINE_LEN} + 1")

        if(NEW_SIZE GREATER MAX_CHUNK_SIZE AND CHUNK_SIZE GREATER 0)
            # Flush current chunk
            string(APPEND HEADER_CONTENT "    result += R\"LUAU_DEFS(${CHUNK}\n)LUAU_DEFS\";\n")
            set(CHUNK "")
            set(CHUNK_SIZE 0)
        endif()

        string(APPEND CHUNK "\n${LINE}")
        math(EXPR CHUNK_SIZE "${CHUNK_SIZE} + ${LINE_LEN} + 1")
    endforeach()

    # Flush remaining chunk
    if(CHUNK_SIZE GREATER 0)
        string(APPEND HEADER_CONTENT "    result += R\"LUAU_DEFS(${CHUNK}\n)LUAU_DEFS\";\n")
    endif()

    string(APPEND HEADER_CONTENT "    return result.c_str();\n}\n\n")
    string(APPEND HEADER_CONTENT "static const char* ROBLOX_GLOBAL_TYPES = ROBLOX_GLOBAL_TYPES_INIT();\n")

    file(WRITE "${CMAKE_BINARY_DIR}/RobloxTypes.hpp" "${HEADER_CONTENT}")
else()
    file(WRITE "${CMAKE_BINARY_DIR}/RobloxTypes.hpp"
        "#pragma once\n"
        "// Roblox definitions not found at build time\n"
        "static const char* ROBLOX_GLOBAL_TYPES = nullptr;\n"
    )
endif()
