#pragma once

inline constexpr char LUNE_SCRIPT_TYPES[] = R"LUAU(
type ScriptReference = {
    Name: string,
    Parent: any,
    --- Returns the absolute source-file path, or the path of an ordinary directory.
    GetFullName: (self: ScriptReference) -> string,
    [string]: any,
}

declare script: ScriptReference

declare project: ScriptReference
)LUAU";
