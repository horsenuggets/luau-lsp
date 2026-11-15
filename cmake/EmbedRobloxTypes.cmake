# Embed definitions as concatenated raw string literals to stay within MSVC's
# 16380 character limit per literal. Track source edits so incremental builds refresh them.
function(embed_global_types SOURCE_FILE SYMBOL HEADER_NAME)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SOURCE_FILE}")
    file(READ "${SOURCE_FILE}" TYPES_CONTENT)
    string(REGEX REPLACE "\n$" "" TYPES_CONTENT "${TYPES_CONTENT}")
    string(REPLACE ";" "\\;" TYPES_CONTENT "${TYPES_CONTENT}")
    string(REPLACE "\n" ";" LINES "${TYPES_CONTENT}")

    set(HEADER_CONTENT "#pragma once\n#include <string>\n\n")
    string(APPEND HEADER_CONTENT "// Auto-generated from the global type definitions.\n")
    string(APPEND HEADER_CONTENT "inline const char* ${SYMBOL}_INIT()\n{\n")
    string(APPEND HEADER_CONTENT "    static const std::string result = [] {\n")
    string(APPEND HEADER_CONTENT "        std::string result;\n")

    set(CHUNK "")
    set(CHUNK_SIZE 0)
    set(MAX_CHUNK_SIZE 14000)
    foreach(LINE IN LISTS LINES)
        string(LENGTH "${LINE}" LINE_LEN)
        math(EXPR NEW_SIZE "${CHUNK_SIZE} + ${LINE_LEN} + 1")
        if(NEW_SIZE GREATER MAX_CHUNK_SIZE AND CHUNK_SIZE GREATER 0)
            string(APPEND HEADER_CONTENT "        result += R\"LUAU_DEFS(${CHUNK})LUAU_DEFS\";\n")
            set(CHUNK "")
            set(CHUNK_SIZE 0)
        endif()

        # Keep metadata on the first line by appending, not prepending, each newline.
        string(APPEND CHUNK "${LINE}\n")
        math(EXPR CHUNK_SIZE "${CHUNK_SIZE} + ${LINE_LEN} + 1")
    endforeach()
    if(CHUNK_SIZE GREATER 0)
        string(APPEND HEADER_CONTENT "        result += R\"LUAU_DEFS(${CHUNK})LUAU_DEFS\";\n")
    endif()

    string(APPEND HEADER_CONTENT "        return result;\n    }();\n    return result.c_str();\n}\n\n")
    string(APPEND HEADER_CONTENT "inline const char* ${SYMBOL} = ${SYMBOL}_INIT();\n")
    set(EMBEDDED_HEADER "${CMAKE_BINARY_DIR}/${HEADER_NAME}")
    file(CONFIGURE OUTPUT "${EMBEDDED_HEADER}" CONTENT "${HEADER_CONTENT}" @ONLY)
endfunction()

set(ROBLOX_TYPES_FILE "${CMAKE_SOURCE_DIR}/scripts/globalTypes.PluginSecurity.d.luau")
if(EXISTS "${ROBLOX_TYPES_FILE}")
    embed_global_types("${ROBLOX_TYPES_FILE}" ROBLOX_GLOBAL_TYPES RobloxTypes.hpp)
else()
    file(WRITE "${CMAKE_BINARY_DIR}/RobloxTypes.hpp" "#pragma once\nstatic const char* ROBLOX_GLOBAL_TYPES = nullptr;\n")
endif()

embed_global_types("${CMAKE_SOURCE_DIR}/scripts/luneGlobalTypes.d.lua" LUNE_GLOBAL_TYPES LuneTypes.hpp)
