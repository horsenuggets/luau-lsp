#include "doctest.h"
#include "TempDir.h"
#include "Fixture.h"
#include "Analyze/AnalyzeCli.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
#include "Analyze/CliConfigurationParser.hpp"

namespace std
{
template<typename T>
ostream& operator<<(ostream& os, const vector<T>& value)
{
    os << "[";
    bool first = true;
    for (const auto& path : value)
    {
        if (!first)
            os << ", ";
        else
            first = false;
        os << path;
    }
    os << "]";
    return os;
}
} // namespace std

TEST_CASE("getFilesToAnalyze")
{
    TempDir t("analyze_cli_get_files_to_analyze");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();
    auto fileB = Uri::file(t.write_child("src/b.luau", "")).fsPath();

    auto allResults = getFilesToAnalyze({t.path()}, {});
    std::sort(allResults.begin(), allResults.end());
    CHECK_EQ(allResults, std::vector<std::string>{fileA, fileB});

    auto ignoredFile = getFilesToAnalyze({t.path()}, {"b.luau"});
    std::sort(ignoredFile.begin(), ignoredFile.end());
    CHECK_EQ(ignoredFile, std::vector<std::string>{fileA});
}

TEST_CASE("getFilesToAnalyze_handles_ignore_globs_within_directories")
{
    TempDir t("analyze_cli_ignored_files");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();
    auto fileB = Uri::file(t.write_child("src/b.luau", "")).fsPath();

    auto ignoredFile = getFilesToAnalyze({t.path()}, {"b.luau"});
    std::sort(ignoredFile.begin(), ignoredFile.end());
    CHECK_EQ(ignoredFile, std::vector<std::string>{fileA});
}

TEST_CASE("getFilesToAnalyze_still_matches_file_if_it_was_explicitly_provided")
{
    TempDir t("analyze_cli_ignored_files_explicitly_provided");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();
    CHECK_EQ(getFilesToAnalyze({fileA}, {"a.luau"}), std::vector<std::string>{fileA});
}

TEST_CASE("ignore_globs_from_settings_file_applied")
{
    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::unordered_map<std::string, std::string> definitionPaths;

    auto configFile = R"({ "luau-lsp.ignoreGlobs": [ "/ignored/**" ] })";

    applySettings(configFile, client, ignoreGlobs, definitionPaths);

    REQUIRE_EQ(ignoreGlobs.size(), 1);
    CHECK_EQ(ignoreGlobs[0], "/ignored/**");
}

TEST_CASE("definition_files_from_settings_file_applied")
{
    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::unordered_map<std::string, std::string> definitionPaths;

    auto configFile = R"({ "luau-lsp.types.definitionFiles": [ "global_types/types.d.luau" ] })";

    applySettings(configFile, client, ignoreGlobs, definitionPaths);

    REQUIRE_EQ(definitionPaths.size(), 1);
    REQUIRE(definitionPaths.find("@roblox1") != definitionPaths.end());
    CHECK_EQ(definitionPaths["@roblox1"], "global_types/types.d.luau");
}

TEST_CASE("enable_new_solver_fflag_from_settings_file_applied")
{
    ScopedFastFlag sff{FFlag::LuauSolverV2, false};

    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::unordered_map<std::string, std::string> definitionPaths;

    auto configFile = R"({ "luau-lsp.fflags.enableNewSolver": true })";

    applySettings(configFile, client, ignoreGlobs, definitionPaths);

    CHECK(FFlag::LuauSolverV2);
}

TEST_CASE_FIXTURE(Fixture, "analysis_relative_file_paths")
{
    CHECK_EQ(getFilePath(&workspace.fileResolver, tempDir.touch_child("test.luau")).relativePath, "test.luau");
    CHECK_EQ(getFilePath(&workspace.fileResolver, tempDir.touch_child("folder/file.luau")).relativePath, "folder/file.luau");
}

TEST_CASE("enable_new_solver_cli_flag")
{
    ScopedFastFlag sff{FFlag::LuauSolverV2, false};

    argparse::ArgumentParser program("test");
    program.set_assign_chars(":=");
    program.add_argument("--enable-new-solver")
        .default_value(false)
        .implicit_value(true);

    std::vector<std::string> arguments{"", "--enable-new-solver"};
    program.parse_args(arguments);

    CHECK(program.get<bool>("--enable-new-solver"));
}

TEST_CASE("enable_new_solver_cli_flag_default_false")
{
    argparse::ArgumentParser program("test");
    program.set_assign_chars(":=");
    program.add_argument("--enable-new-solver")
        .default_value(false)
        .implicit_value(true);

    std::vector<std::string> arguments{""};
    program.parse_args(arguments);

    CHECK_FALSE(program.get<bool>("--enable-new-solver"));
}

TEST_CASE("enable_new_solver_cli_flag_overrides_settings_file")
{
    ScopedFastFlag sff{FFlag::LuauSolverV2, false};

    CliClient client;
    std::vector<std::string> ignoreGlobs;
    std::unordered_map<std::string, std::string> definitionPaths;

    // Settings file does not enable new solver
    auto configFile = R"({ "luau-lsp.fflags.enableNewSolver": false })";
    applySettings(configFile, client, ignoreGlobs, definitionPaths);
    CHECK_FALSE(FFlag::LuauSolverV2);

    // CLI flag override takes precedence
    client.configuration.fflags.enableNewSolver = true;
    FFlag::LuauSolverV2.value = true;

    CHECK(FFlag::LuauSolverV2);
    CHECK(client.configuration.fflags.enableNewSolver);
}

TEST_CASE("parse_definitions_files_handles_new_syntax")
{
    argparse::ArgumentParser program("test");
    program.set_assign_chars(":=");
    program.add_argument("--definitions", "--defs")
        .help("A path to a Luau definitions file to load into the global namespace")
        .default_value<std::vector<std::string>>({})
        .append()
        .metavar("PATH");

    std::vector<std::string> arguments{
        "", "--definitions:@roblox=example_path.d.luau", "--definitions:@lune=lune.d.luau", "--definitions:no_at_sign=path.d.luau"};
    program.parse_args(arguments);

    auto definitionsFiles = processDefinitionsFilePaths(program);

    CHECK_EQ(definitionsFiles, std::unordered_map<std::string, std::string>{
                                   {"@roblox", "example_path.d.luau"},
                                   {"@lune", "lune.d.luau"},
                                   {"@no_at_sign", "path.d.luau"},
                               });
}

TEST_CASE("parse_definitions_files_handles_legacy_syntax")
{
    argparse::ArgumentParser program("test");
    program.set_assign_chars(":=");
    program.add_argument("--definitions", "--defs")
        .help("A path to a Luau definitions file to load into the global namespace")
        .default_value<std::vector<std::string>>({})
        .append()
        .metavar("PATH");

    std::vector<std::string> arguments{"", "--definitions=example_path.d.luau", "--definitions=lune.d.luau"};
    program.parse_args(arguments);

    auto definitionsFiles = processDefinitionsFilePaths(program);

    CHECK_EQ(definitionsFiles, std::unordered_map<std::string, std::string>{
                                   {"@roblox", "example_path.d.luau"},
                                   {"@roblox1", "lune.d.luau"},
                               });
}

TEST_SUITE_END();
