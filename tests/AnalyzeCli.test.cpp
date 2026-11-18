#include "doctest.h"
#include "TempDir.h"
#include "Fixture.h"
#include "Analyze/AnalyzeCli.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/WorkspaceFileResolver.hpp"
#include "Analyze/CliConfigurationParser.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Luau/TypeAttach.h"
#include "Luau/PrettyPrinter.h"
#include "LSP/LuauExt.hpp"
#include "RobloxTypes.hpp"
#include "LuneTypes.hpp"

#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

TEST_SUITE_BEGIN("AnalyzeCli");

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

/// Sets up default client config and definitions. Call this before customizing
/// globalConfig fields, then call setupCliWorkspace() to finalize.
static void initCliClient(CliClient& client)
{
    client.globalConfig = Luau::LanguageServer::defaultTestClientConfiguration();
    client.definitionsFiles.emplace("@roblox", "./tests/testdata/standard_definitions.d.luau");
}

static void setupCliWorkspace(CliClient& client, WorkspaceFolder& workspace)
{
    workspace.setupWithConfiguration(client.globalConfig);
    workspace.isReady = true;
}

static void initRobloxCliClient(CliClient& client)
{
    initCliClient(client);
    client.globalConfig.platform.type = LSPPlatformConfig::Roblox;
    client.globalConfig.sourcemap.enabled = true;
    client.globalConfig.sourcemap.sourcemapFile = "sourcemap.json";
}

TEST_CASE("getFilesToAnalyze")
{
    TempDir t("analyze_cli_get_files_to_analyze");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();
    auto fileB = Uri::file(t.write_child("src/b.luau", "")).fsPath();

    auto allResults = getFilesToAnalyze({t.path()});
    std::sort(allResults.begin(), allResults.end());
    CHECK_EQ(allResults, std::vector<std::string>{fileA, fileB});

    CliClient client;
    initCliClient(client);
    client.globalConfig.ignoreGlobs = {"b.luau"};
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto ignoredFile = getFilesToAnalyze({t.path()}, &workspace);
    std::sort(ignoredFile.begin(), ignoredFile.end());
    CHECK_EQ(ignoredFile, std::vector<std::string>{fileA});
}

TEST_CASE("getFilesToAnalyze_handles_ignore_globs_within_directories")
{
    TempDir t("analyze_cli_ignored_files");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();
    auto fileB = Uri::file(t.write_child("src/b.luau", "")).fsPath();

    CliClient client;
    initCliClient(client);
    client.globalConfig.ignoreGlobs = {"b.luau"};
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto ignoredFile = getFilesToAnalyze({t.path()}, &workspace);
    std::sort(ignoredFile.begin(), ignoredFile.end());
    CHECK_EQ(ignoredFile, std::vector<std::string>{fileA});
}

TEST_CASE("getFilesToAnalyze_still_matches_file_if_it_was_explicitly_provided")
{
    TempDir t("analyze_cli_ignored_files_explicitly_provided");
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();

    CliClient client;
    initCliClient(client);
    client.globalConfig.ignoreGlobs = {"a.luau"};
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    CHECK_EQ(getFilesToAnalyze({fileA}, &workspace), std::vector<std::string>{fileA});
}

TEST_CASE("cli_setup_errors_are_distinguished_from_warnings")
{
    CliClient client;
    client.sendLogMessage(lsp::MessageType::Warning, "A nonfatal warning.");
    CHECK_FALSE(client.hasErrorLog);
    client.sendWindowMessage(lsp::MessageType::Error, "A failed setup operation.");
    CHECK(client.hasErrorLog);
}

TEST_CASE("getFilesToAnalyze_sorts_and_deduplicates_overlapping_inputs")
{
    TempDir t("analyze_cli_ordered_files");
    auto fileZ = Uri::file(t.write_child("src/z.luau", "")).fsPath();
    auto fileA = Uri::file(t.write_child("src/a.luau", "")).fsPath();
    auto fileNested = Uri::file(t.write_child("src/nested/b.lua", "")).fsPath();
    auto fileAfterNested = Uri::file(t.write_child("src/nestedZ.lua", "")).fsPath();
    t.write_child("src/not_source.txt", "");
    const std::vector<std::string> expected{fileA, fileNested, fileAfterNested, fileZ};
    CHECK_EQ(
        getFilesToAnalyze({fileZ, t.path() + "/src", fileA, t.path() + "/src/nested", t.path() + "/src/nested/../a.luau"}), expected);
    CHECK_EQ(getFilesToAnalyze({t.path() + "/src/nested", fileA, t.path() + "/src", fileZ}), expected);
    CHECK_THROWS_AS(getFilesToAnalyze({t.path() + "/missing.luau"}), std::runtime_error);
}

TEST_CASE("getFilesToAnalyze_uses_platform_uri_equality_for_case_variants")
{
    TempDir t("analyze_cli_case_variants");
    const auto upper = Uri::file(t.write_child("Source/A.luau", "return 1")).fsPath();
    const auto other = Uri::file(t.write_child("Source/B.luau", "return 2")).fsPath();
    const auto lower = Uri::file(t.write_child("source/a.luau", "return 1")).fsPath();
#if defined(_WIN32) || defined(__APPLE__)
    const std::vector<std::string> expected{upper, other};
#else
    const std::vector<std::string> expected{upper, other, lower};
#endif
    CHECK_EQ(getFilesToAnalyze({lower, t.path() + "/Source", other, t.path() + "/source", upper}), expected);
    CHECK_EQ(getFilesToAnalyze({upper, t.path() + "/source", t.path() + "/Source", lower, other}), expected);
}

static std::vector<json> jsonlEvents(const std::ostringstream& output)
{
    std::istringstream input(output.str());
    std::vector<json> events;
    for (std::string line; std::getline(input, line);)
    {
        auto event = json::parse(line);
        REQUIRE_EQ(event.at("schemaVersion"), 1);
        events.push_back(std::move(event));
    }
    return events;
}

TEST_CASE("jsonl_flushes_each_record_and_only_completes_finished_analysis")
{
    struct FlushedBuffer : std::stringbuf
    {
        int flushes = 0;
        int sync() override
        {
            flushes += 1;
            return 0;
        }
    } buffer;
    std::ostream output(&buffer);
    AnalysisJsonlReporter reporter(output);
    reporter.start("test-version", "standard", "new");
    reporter.progress("discovery", "started", 0, std::nullopt);
    CHECK_EQ(buffer.flushes, 2);
    CHECK_THROWS_AS(reporter.finish(0, 1), std::logic_error);
    CHECK_EQ(buffer.flushes, 2);
    reporter.finish(0, 0);
    CHECK_EQ(buffer.flushes, 3);
    CHECK(buffer.str().find("\"complete\":true") != std::string::npos);
}

TEST_CASE("jsonl_deduplicates_exact_diagnostics_without_collapsing_different_findings")
{
    std::ostringstream output;
    AnalysisJsonlReporter reporter(output);
    lsp::Diagnostic diagnostic;
    diagnostic.code = 1000;
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.range = {{0, 0}, {0, 1}};
    diagnostic.message = "First message\nwith a second line";
    CHECK(reporter.diagnostic("Source/Example.luau", "TypeError", diagnostic));
    CHECK_FALSE(reporter.diagnostic("Source/Example.luau", "TypeError", diagnostic));
    diagnostic.message = "Another error at the same location";
    CHECK(reporter.diagnostic("Source/Example.luau", "TypeError", diagnostic));
    diagnostic.code = 1001;
    CHECK(reporter.diagnostic("Source/Example.luau", "TypeError", diagnostic));
    diagnostic.severity = lsp::DiagnosticSeverity::Warning;
    CHECK(reporter.diagnostic("Source/Example.luau", "TypeError", diagnostic));
    diagnostic.range.end.character = 2;
    CHECK(reporter.diagnostic("Source/Example.luau", "TypeError", diagnostic));
    CHECK_EQ(reporter.errorCount(), 3);
    CHECK_EQ(reporter.warningCount(), 2);
    reporter.finish(1, 1);
    const auto events = jsonlEvents(output);
    REQUIRE_EQ(events.size(), 6);
    CHECK_EQ(events.front().at("message"), "First message\nwith a second line");
    CHECK_EQ(events.back().at("diagnosticCount"), 5);
    CHECK_EQ(events.back().at("errors"), 3);
    CHECK_EQ(events.back().at("warnings"), 2);
}

TEST_CASE("jsonl_reports_progress_and_deduplicated_import_errors")
{
    TempDir t("analyze_cli_jsonl_imports");
    const auto dependency = t.write_child("shared.luau", "--!strict\nreturn ('text' :: number)");
    const auto fileA = t.write_child("a.luau", "--!strict\nreturn require('./shared')");
    const auto fileB = t.write_child("b.luau", "--!strict\nreturn require('./shared')");
    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);
    std::ostringstream output;
    AnalysisJsonlReporter reporter(output);
    reporter.start("test-version", "standard", FFlag::LuauSolverV2 ? "new" : "old");
    CHECK(analyzeFilesJsonl(workspace, {fileA, fileB, dependency}, reporter));
    reporter.finish(3, 3);
    const auto events = jsonlEvents(output);
    REQUIRE_EQ(events.front().at("event"), "start");
    CHECK_EQ(events.front().at("positionEncoding"), "utf-16");
    size_t started = 0;
    size_t completed = 0;
    size_t diagnostics = 0;
    for (const auto& event : events)
    {
        if (event.at("event") == "progress")
        {
            CHECK_EQ(event.at("phase"), "checking");
            CHECK_EQ(event.at("total"), 3);
            CHECK(event.at("path").get<std::string>().find(t.path()) == std::string::npos);
            if (event.at("state") == "started")
            {
                CHECK_EQ(event.at("completed"), completed);
                started += 1;
            }
            else
            {
                completed += 1;
                CHECK_EQ(event.at("completed"), completed);
            }
        }
        else if (event.at("event") == "diagnostic")
        {
            diagnostics += 1;
            CHECK_EQ(event.at("path"), "shared.luau");
            CHECK_EQ(event.at("severity"), "error");
            CHECK(event.at("code").is_number_integer());
        }
    }
    CHECK_EQ(started, 3);
    CHECK_EQ(completed, 3);
    CHECK_EQ(diagnostics, 1);
    CHECK_EQ(events.back().at("event"), "summary");
    CHECK_EQ(events.back().at("filesCompleted"), 3);
    CHECK_EQ(events.back().at("errors"), 1);
}

TEST_CASE("jsonl_module_names_are_relative_without_rewriting_type_literals")
{
    TempDir t("analyze_cli_jsonl_module_names");
    const auto fileA = t.write_child("a.luau", "--!strict\nreturn require('./b')");
    const auto fileB = t.write_child("b.luau", "--!strict\nreturn require('./a')");
    const auto literalPath = Uri::file(t.path()).path;
    auto displayRoot = Uri::file(t.path()).fsPath();
    std::replace(displayRoot.begin(), displayRoot.end(), '\\', '/');
    const auto literalFile = t.write_child("literal.luau", "--!strict\nlocal value: 'expected' = " + json(literalPath).dump() + "\nreturn value");
    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);
    std::ostringstream cycles;
    AnalysisJsonlReporter cycleReporter(cycles);
    const Luau::TypeError cycleError{{{0, 0}, {0, 1}}, fileA, Luau::ModuleHasCyclicDependency{{fileA, fileB}}};
    CHECK(reportTypeErrorJsonl(workspace, cycleError, cycleReporter));
    const auto cycleEvents = jsonlEvents(cycles);
    REQUIRE_EQ(cycleEvents.size(), 1);
    const auto cycleMessage = cycleEvents.front().at("message").get<std::string>();
    CHECK(cycleMessage.find(displayRoot) == std::string::npos);
    CHECK(cycleMessage.find("a.luau") != std::string::npos);
    CHECK(cycleMessage.find("b.luau") != std::string::npos);
    std::ostringstream literals;
    AnalysisJsonlReporter literalReporter(literals);
    CHECK(analyzeFileJsonl(workspace, literalFile, literalReporter));
    const auto literalEvents = jsonlEvents(literals);
    REQUIRE_EQ(literalEvents.size(), 1);
    CHECK(literalEvents.front().at("message").get<std::string>().find(literalPath) != std::string::npos);
}

TEST_CASE("jsonl_propagates_internal_compiler_errors_without_completing_targets")
{
    TempDir t("analyze_cli_jsonl_internal_failure");
    const auto path = t.write_child("test.luau", "return 1");
    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);
    workspace.frontend.prepareModuleScope = [](const auto&, const auto&, bool)
    {
        throw Luau::InternalCompilerError("Injected analysis failure.");
    };
    std::ostringstream output;
    AnalysisJsonlReporter reporter(output);
    CHECK_THROWS_AS(analyzeFilesJsonl(workspace, {path}, reporter), Luau::InternalCompilerError);
    CHECK(workspace.frontend.getSourceModule(workspace.fileResolver.getModuleName(Uri::file(path))) != nullptr);
    const auto events = jsonlEvents(output);
    REQUIRE_EQ(events.size(), 1);
    CHECK_EQ(events.front().at("event"), "progress");
    CHECK_EQ(events.front().at("state"), "started");
    CHECK_EQ(events.front().at("completed"), 0);
}

TEST_CASE("jsonl_maps_transformed_type_syntax_and_lint_ranges_to_original_source")
{
    TempDir t("analyze_cli_jsonl_source_mapping");
    const auto plugin = t.write_child("header.luau", R"(
return {
    transformSource = function(source, context)
        return {{ startLine = 1, startColumn = 1, endLine = 1, endColumn = 1, newText = "-- Header\n" }}
    end
}
)");
    const auto typed = t.write_child("typed.luau", "--!strict\nlocal unused = 1\nlocal value: number = 'invalid'\nreturn value\n");
    const auto syntax = t.write_child("syntax.luau", "--!strict\nlocal value = (\n");
    for (const auto& path : {typed, syntax})
    {
        std::vector<json> original;
        for (const bool transformed : {false, true})
        {
            CliClient client;
            initCliClient(client);
            client.globalConfig.plugins.enabled = transformed;
            client.globalConfig.plugins.paths = {plugin};
            Luau::Config config;
            config.enabledLint.enableWarning(Luau::LintWarning::Code_LocalUnused);
            WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), config);
            setupCliWorkspace(client, workspace);
            std::ostringstream output;
            AnalysisJsonlReporter reporter(output);
            CHECK(analyzeFileJsonl(workspace, path, reporter));
            const auto events = jsonlEvents(output);
            REQUIRE_FALSE(events.empty());
            if (transformed)
                CHECK_EQ(events, original);
            else
                original = events;
        }
    }
}

TEST_CASE("jsonl_preserves_effective_lint_severity")
{
    for (const bool fatal : {false, true})
    {
        CAPTURE(fatal);
        TempDir t("analyze_cli_jsonl_lint");
        const auto path = t.write_child("test.luau", "local unused = 1");
        CliClient client;
        initCliClient(client);
        Luau::Config config;
        config.enabledLint.enableWarning(Luau::LintWarning::Code_LocalUnused);
        if (fatal)
            config.fatalLint.enableWarning(Luau::LintWarning::Code_LocalUnused);
        WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), config);
        setupCliWorkspace(client, workspace);
        std::ostringstream output;
        AnalysisJsonlReporter reporter(output);
        CHECK(analyzeFileJsonl(workspace, path, reporter));
        const auto events = jsonlEvents(output);
        REQUIRE_EQ(events.size(), 1);
        CHECK_EQ(events.front().at("category"), "LocalUnused");
        CHECK_EQ(events.front().at("severity"), fatal ? "error" : "warning");
        CHECK_EQ(reporter.errorCount(), fatal ? 1 : 0);
        CHECK_EQ(reporter.warningCount(), fatal ? 0 : 1);
    }
}

TEST_CASE("jsonl_uses_utf16_source_ranges")
{
    TempDir t("analyze_cli_jsonl_unicode");
    const std::string source = "--!strict\nlocal _ = '\xf0\x9f\x98\x80'; local _value: number = 'wrong'";
    const auto path = t.write_child("test.luau", source);
    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);
    std::ostringstream output;
    AnalysisJsonlReporter reporter(output);
    CHECK(analyzeFileJsonl(workspace, path, reporter));
    const auto events = jsonlEvents(output);
    REQUIRE_EQ(events.size(), 1);
    CHECK_EQ(events.front().at("range").at("start").at("line"), 1);
    const auto checked = workspace.checkSimple(path, nullptr);
    REQUIRE_EQ(checked.errors.size(), 1);
    const auto byteOffset = checked.errors.front().location.begin.column;
    REQUIRE_GT(byteOffset, 14);
    CHECK_EQ(events.front().at("range").at("start").at("character"), byteOffset - 2);
}

TEST_CASE("jsonl_syntax_errors_are_completed_source_diagnostics")
{
    TempDir t("analyze_cli_jsonl_syntax");
    const auto path = t.write_child("test.luau", "local value = {");
    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);
    std::ostringstream output;
    AnalysisJsonlReporter reporter(output);
    CHECK(analyzeFilesJsonl(workspace, {path}, reporter));
    CHECK_GT(reporter.errorCount(), 0);
    reporter.finish(1, 1);
    const auto events = jsonlEvents(output);
    CHECK_EQ(events[1].at("category"), "SyntaxError");
    CHECK_EQ(events.back().at("complete"), true);
}

TEST_CASE("jsonl_missing_source_does_not_complete")
{
    TempDir t("analyze_cli_jsonl_missing");
    const auto path = t.write_child("missing.luau", "return 1");
    const auto files = getFilesToAnalyze({path});
    REQUIRE_EQ(files.size(), 1);
    REQUIRE(std::filesystem::remove(path));
    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);
    std::ostringstream output;
    AnalysisJsonlReporter reporter(output);
    CHECK_FALSE(analyzeFilesJsonl(workspace, files, reporter));
    const auto events = jsonlEvents(output);
    REQUIRE_EQ(events.size(), 1);
    CHECK_EQ(events.front().at("state"), "started");
}

TEST_CASE("ignore_globs_from_settings_file_applied")
{
    CliClient client;

    auto configFile = R"({ "luau-lsp.ignoreGlobs": [ "/ignored/**" ] })";

    applySettings(configFile, client);

    REQUIRE_EQ(client.globalConfig.ignoreGlobs.size(), 1);
    CHECK_EQ(client.globalConfig.ignoreGlobs[0], "/ignored/**");
}

TEST_CASE("definition_files_from_settings_file_applied")
{
    CliClient client;

    auto configFile = R"({ "luau-lsp.types.definitionFiles": [ "global_types/types.d.luau" ] })";

    applySettings(configFile, client);

    REQUIRE_EQ(client.definitionsFiles.size(), 1);
    REQUIRE(client.definitionsFiles.find("@roblox1") != client.definitionsFiles.end());
    CHECK_EQ(client.definitionsFiles["@roblox1"], "global_types/types.d.luau");
}

TEST_CASE("enable_new_solver_fflag_from_settings_file_applied")
{
    ScopedFastFlag sff{FFlag::LuauSolverV2, false};

    CliClient client;

    auto configFile = R"({ "luau-lsp.fflags.enableNewSolver": true })";

    applySettings(configFile, client);

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
    program.add_argument("--enable-new-solver").default_value(false).implicit_value(true);

    std::vector<std::string> arguments{"", "--enable-new-solver"};
    program.parse_args(arguments);

    CHECK(program.get<bool>("--enable-new-solver"));
}

TEST_CASE("enable_new_solver_cli_flag_default_false")
{
    argparse::ArgumentParser program("test");
    program.set_assign_chars(":=");
    program.add_argument("--enable-new-solver").default_value(false).implicit_value(true);

    std::vector<std::string> arguments{""};
    program.parse_args(arguments);

    CHECK_FALSE(program.get<bool>("--enable-new-solver"));
}

TEST_CASE("enable_new_solver_cli_flag_overrides_settings_file")
{
    ScopedFastFlag sff{FFlag::LuauSolverV2, false};

    CliClient client;

    // Settings file does not enable new solver
    auto configFile = R"({ "luau-lsp.fflags.enableNewSolver": false })";
    applySettings(configFile, client);
    CHECK_FALSE(FFlag::LuauSolverV2);
    CHECK_FALSE(client.globalConfig.fflags.enableNewSolver);
}

TEST_CASE("builtin_definitions_follow_platform_and_user_definitions")
{
    // Walk every combination of platform and whether the user provided definitions
    for (auto platform : {LSPPlatformConfig::Standard, LSPPlatformConfig::Roblox, LSPPlatformConfig::Lune})
    {
        for (bool userDefinitions : {false, true})
        {
            CAPTURE(int(platform));
            CAPTURE(userDefinitions);

            CliClient client;
            client.globalConfig.platform.type = platform;
            if (userDefinitions)
                client.definitionsFiles.emplace("@roblox", "types.d.luau");

            CHECK(addBuiltinDefinitions(client));

            std::vector<std::string> packages;
            for (const auto& [packageName, _] : client.builtinDefinitions)
                packages.push_back(packageName);

            if (platform == LSPPlatformConfig::Roblox && !userDefinitions)
                CHECK_EQ(packages, std::vector<std::string>{"@roblox"});
            else if (platform == LSPPlatformConfig::Lune)
                CHECK_EQ(packages, std::vector<std::string>{"@lune"});
            else
                CHECK(packages.empty());
        }
    }
}

TEST_CASE("embedded_roblox_definitions_keep_metadata_on_first_line")
{
    REQUIRE(ROBLOX_GLOBAL_TYPES != nullptr);
    CHECK(Luau::startsWith(ROBLOX_GLOBAL_TYPES, "--#METADATA#"));
    CHECK(types::parseDefinitionsFileMetadata(ROBLOX_GLOBAL_TYPES).has_value());
}

TEST_CASE("lune_builtin_definitions_are_registered_in_workspace")
{
    TempDir t("analyze_cli_lune_builtins");
    auto filePath = t.write_child("test.luau", R"(
        --!strict
        warn("hello")
        task.wait(1)
        local position: Vector3 = Vector3.new(1, 2, 3)
        local _ = position.Magnitude
        local _ = script.Name
        local _ = project.Name
        local _ = project.Source.Parent
        local _ = project:GetFullName()
    )");

    CliClient client;
    client.globalConfig = Luau::LanguageServer::defaultTestClientConfiguration();
    client.globalConfig.platform.type = LSPPlatformConfig::Lune;
    REQUIRE(addBuiltinDefinitions(client));
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto cr = workspace.checkSimple(filePath, nullptr);
    CHECK_MESSAGE(cr.errors.empty(), (cr.errors.empty() ? "" : Luau::toString(cr.errors[0])));
}

// Explicit companion contract for @lune/roblox, with no dependency on a local Lune checkout.
static const std::vector<std::string> luneRobloxExports = {
    "Axes",
    "BrickColor",
    "CFrame",
    "Color3",
    "ColorSequence",
    "ColorSequenceKeypoint",
    "Content",
    "Enum",
    "Faces",
    "Font",
    "Instance",
    "NumberRange",
    "NumberSequence",
    "NumberSequenceKeypoint",
    "PhysicalProperties",
    "Ray",
    "Rect",
    "Region3",
    "Region3int16",
    "UDim",
    "UDim2",
    "UniqueId",
    "Vector2",
    "Vector2int16",
    "Vector3",
    "Vector3int16",
    "deserializeModel",
    "deserializePlace",
    "getAuthCookie",
    "getReflectionDatabase",
    "implementMethod",
    "implementProperty",
    "registerClass",
    "registerService",
    "serializeModel",
    "serializePlace",
    "studioApplicationPath",
    "studioBuiltinPluginPath",
    "studioContentPath",
    "studioPluginPath",
};

TEST_CASE("lune_roblox_globals_export_contract_is_complete_and_typed")
{
    REQUIRE_EQ(luneRobloxExports.size(), 40);
    TempDir t("lune_roblox_export_contract");
    CliClient client;
    client.globalConfig = Luau::LanguageServer::defaultTestClientConfiguration();
    client.globalConfig.platform.type = LSPPlatformConfig::Lune;
    REQUIRE(addBuiltinDefinitions(client));
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    for (const auto& name : luneRobloxExports)
    {
        CAPTURE(name);
        auto path = t.write_child(name + ".luau", "--!strict\nlocal value = " + name + "\nreturn value\n");
        auto result = workspace.checkSimple(path, nullptr);
        REQUIRE_MESSAGE(result.errors.empty(), (result.errors.empty() ? "" : Luau::toString(result.errors.front())));
        // Also pin the type boundary. Merely resolving a global typed as any is not enough.
        auto invalid = t.write_child(name + "_invalid.luau", "--!strict\nlocal value: number = " + name + "\nreturn value\n");
        CHECK_FALSE(workspace.checkSimple(invalid, nullptr).errors.empty());
    }
}

TEST_CASE_FIXTURE(Fixture, "lune_roblox_definitions_load_without_errors")
{
    std::ifstream source("scripts/luneGlobalTypes.d.lua");
    REQUIRE(source.is_open());
    const std::string definitions{std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
    CHECK_EQ(std::string(LUNE_GLOBAL_TYPES), definitions);

    switchToStandardPlatform();
    Luau::unfreeze(workspace.frontend.globals.globalTypes);
    if (!FFlag::LuauSolverV2)
        Luau::unfreeze(workspace.frontend.globalsForAutocomplete.globalTypes);
    auto result = workspace.loadDefinitionFile("@lune", LUNE_GLOBAL_TYPES);
    Luau::freeze(workspace.frontend.globals.globalTypes);
    if (!FFlag::LuauSolverV2)
        Luau::freeze(workspace.frontend.globalsForAutocomplete.globalTypes);
    for (const auto& error : result.parseResult.errors)
        CHECK_MESSAGE(false, std::string(error.what()));
    if (result.module)
        for (const auto& error : result.module->errors)
            CHECK_MESSAGE(false, Luau::toString(error));
    CHECK(result.success);
}

TEST_CASE_FIXTURE(Fixture, "lune_roblox_datatypes_have_supported_signatures")
{
    switchToStandardPlatform();
    loadDefinition("@lune", LUNE_GLOBAL_TYPES);
    auto result = check(R"(
        --!strict
        local axes: Axes = Axes.new(Enum.Axis.X, Enum.NormalId.Top)
        local face: boolean = axes.Top
        local faces: Faces = Faces.new(Enum.NormalId.Front)
        local back: boolean = faces.Back
        local brick: BrickColor = BrickColor.new(Color3.fromRGB(1, 2, 3))
        local color: Color3 = BrickColor.Red().Color:Lerp(brick.Color, 0.5)
        local hue: number, saturation: number, value: number = color:ToHSV()
        local sequence: ColorSequence = ColorSequence.new({ColorSequenceKeypoint.new(0, color), ColorSequenceKeypoint.new(1, color)})
        local sample: Color3 = sequence.Keypoints[1].Value
        local numeric: NumberSequence = NumberSequence.new({NumberSequenceKeypoint.new(0, 2, 0.5), NumberSequenceKeypoint.new(1, 3)})
        local envelope: number = numeric.Keypoints[1].Envelope
        local range: NumberRange = NumberRange.new(1, 2)
        local minimum: number = range.Min
        local font: Font = Font.fromName("BuilderSans", Enum.FontWeight.Bold, Enum.FontStyle.Normal)
        local bold: boolean = font.Bold
        local content: Content = Content.fromAssetId(123)
        local uri: string? = content.Uri
        local object: Instance? = Content.none.Object
        local source: EnumItem = content.SourceType
        local physics: PhysicalProperties = PhysicalProperties.new(1, 0.5, 0.5, 1, 1, 0.2)
        local absorption: number = physics.AcousticAbsorption
        local material: PhysicalProperties = PhysicalProperties.new(Enum.Material.Plastic)
        local ray: Ray = Ray.new(Vector3.zero, Vector3.yAxis)
        local nearest: Vector3 = ray:ClosestPoint(Vector3.one)
        local distance: number = ray.Unit:Distance(nearest)
        local rect: Rect = Rect.new(Vector2.zero, Vector2.one)
        local width: number = Rect.new(0, 0, 10, 20).Width
        local region: Region3 = Region3.new(Vector3.zero, Vector3.one):ExpandToGrid(4)
        local size: Vector3 = region.Size
        local integer: Vector3int16 = Vector3int16.new(1, 2, 3) * 2
        local integerRegion: Region3int16 = Region3int16.new(Vector3int16.new(), integer)
        local integerMin: Vector3int16 = integerRegion.Min
        local vector: Vector2int16 = -Vector2int16.new(1, 2)
        local x: number = vector.X
        local dim: UDim = UDim.new(0.5, 10) + UDim.new()
        local dims: UDim2 = UDim2.new(dim, dim):Lerp(UDim2.fromScale(1, 1), 0.5)
        local function optionalDims(x: UDim?, y: UDim?): UDim2
            return UDim2.new(x, y)
        end
        local emptyDims: UDim2 = UDim2.new()
        local nilDims: UDim2 = UDim2.new(nil, nil)
        local xDim: UDim2 = UDim2.new(dim)
        local yDim: UDim2 = UDim2.new(nil, dim)
        local xOffset: UDim2 = UDim2.new(nil, 10)
        local yScale: UDim2 = UDim2.new(nil, nil, 0.5)
        local yOffset: UDim2 = UDim2.new(nil, nil, nil, 10)
        local offset: number = dims.X.Offset
        local id: UniqueId = UniqueId.fromString(tostring(UniqueId.new()))
        local null: UniqueId = UniqueId.null
        local angle: number = Vector2.xAxis:Angle(Vector2.yAxis)
        local normal: Vector3 = Vector3.fromNormalId(Enum.NormalId.Top)
        local axis: Vector3 = Vector3.fromAxis(Enum.Axis.X)
        local rotation: CFrame = CFrame.fromEulerAngles(1, 2, 3, Enum.RotationOrder.YXZ)
        local between: CFrame = CFrame.fromRotationBetweenVectors(axis, normal)
        local matrix: CFrame = CFrame.new(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1)
        local position: Vector3 = matrix * Vector3.one
        local first: Vector3, second: Vector3 = matrix:PointToWorldSpace(Vector3.zero, Vector3.one)
        local rx: number, ry: number, rz: number = rotation:ToEulerAngles(Enum.RotationOrder.XYZ)
        local delta: number = rotation:AngleBetween(between)
    )");
    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "lune_roblox_rect_optional_bounds_and_arithmetic")
{
    switchToStandardPlatform();
    loadDefinition("@lune", LUNE_GLOBAL_TYPES);
    auto result = check(R"(
        --!strict
        local function optionalBounds(min: Vector2?, max: Vector2?): Rect
            return Rect.new(min, max)
        end
        local empty: Rect = Rect.new()
        local minimum: Rect = Rect.new(Vector2.one)
        local maximum: Rect = Rect.new(nil, Vector2.one)
        local explicitNil: Rect = Rect.new(Vector2.one, nil)
        local nilBounds: Rect = Rect.new(nil, nil)
        local negated: Rect = -minimum
        local added: Rect = minimum + maximum
        local subtracted: Rect = maximum - minimum
        local width: number = (added + negated - subtracted).Width
        local y0: Rect = Rect.new(nil, 1)
        local x1: Rect = Rect.new(nil, nil, 1)
        local y1: Rect = Rect.new(nil, nil, nil, 1)
    )");
    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE_FIXTURE(Fixture, "lune_roblox_instances_enums_and_helpers_have_typed_results")
{
    switchToStandardPlatform();
    loadDefinition("@lune", LUNE_GLOBAL_TYPES);
    auto result = check(R"(
        --!strict
        local instance: Instance = Instance.new("Folder")
        instance.Parent = Instance.new("Folder")
        instance.Name = "Root"
        local clone: Instance = instance:Clone()
        local descendants: {Instance} = clone:GetDescendants()
        local found: Instance? = clone:FindFirstChild("Child", true)
        local matches: {Instance} = clone:QueryDescendants("Folder")
        instance:AddTag("Test")
        local tagged: boolean = instance:HasTag("Test")
        local tags: {string} = instance:GetTags()
        local fullName: string = instance:GetFullName()
        local enums: {Enum} = Enum:GetEnums()
        local items: {EnumItem} = Enum.Material:GetEnumItems()
        local item: EnumItem? = Enum.Material:FromName("Plastic")
        local fromValue: EnumItem? = Enum.Material:FromValue(256)
        local enumType: Enum = items[1].EnumType
        local name: string = items[1].Name
        local value: number = items[1].Value
        local model: DataModel = deserializePlace("contents")
        local created: DataModel = Instance.new("DataModel")
        local service: Instance = model:GetService("Workspace")
        local maybeService: Instance? = created:FindService("Workspace")
        local instances: {Instance} = deserializeModel("contents")
        local binary: string = serializePlace(model)
        local xml: string = serializeModel(instances, true)
        local cookie: string? = getAuthCookie(true)
        local application: string = studioApplicationPath()
        local builtin: string = studioBuiltinPluginPath()
        local content: string = studioContentPath()
        local plugin: string = studioPluginPath()
        registerClass("Custom", "Folder")
        registerService("CustomService")
        implementProperty("Folder", "Count", function(target: Instance): number
            return #target:GetChildren()
        end, function(target: Instance, value: number)
            target:SetAttribute("Count", value)
        end)
        implementMethod("Folder", "Echo", function(target: Instance, message: string): (string, number)
            return target.Name .. message, 1
        end)
        local database = getReflectionDatabase()
        local version: string = database.Version
        local names: {string} = database:GetClassNames()
        local enumNames: {string} = database:GetEnumNames()
        local class = database:GetClass("Part")
        if class then
            local superclass: string? = class.Superclass
            local datatype: string = class.Properties.Name.Datatype
            local scriptability: DatabaseScriptability = class.Properties.Name.Scriptability
        end
        local descriptor = database:FindEnum("Material")
        if descriptor then
            local materialId: number = descriptor.Items.Plastic
        end
    )");
    LUAU_LSP_REQUIRE_NO_ERRORS(result);
}

TEST_CASE("lune_roblox_globals_reject_invalid_arguments_and_unsupported_members")
{
    TempDir t("lune_roblox_invalid_arguments");
    CliClient client;
    client.globalConfig = Luau::LanguageServer::defaultTestClientConfiguration();
    client.globalConfig.platform.type = LSPPlatformConfig::Lune;
    REQUIRE(addBuiltinDefinitions(client));
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);
    size_t index = 0;
    for (const auto& source : {
             "Axes.new(1)",
             "BrickColor.new(false)",
             "CFrame.new(1, 2)",
             "Color3.fromHex(123)",
             "ColorSequence.new({NumberSequenceKeypoint.new(0, 1)})",
             "ColorSequenceKeypoint.new(0, 1)",
             "Content.fromObject(1)",
             "Faces.new(true)",
             "Font.fromId(false)",
             "Instance.new(1)",
             "NumberRange.new(false)",
             "NumberSequence.new(Color3.new())",
             "NumberSequenceKeypoint.new(0, false)",
             "PhysicalProperties.new(true)",
             "Ray.new(Vector2.zero, Vector3.zero)",
             "Rect.new(Vector3.zero, Vector3.one)",
             "local value = Rect.new() + Vector2.one",
             "local value = Rect.new() - 1",
             "Region3.new(Vector2.zero, Vector2.one)",
             "Region3int16.new(Vector3.zero, Vector3.one)",
             "UDim.new(false)",
             "UDim2.fromOffset(false)",
             "UniqueId.fromString(false)",
             "Vector2.new(false)",
             "Vector2int16.new(false)",
             "Vector3.fromAxis(1)",
             "Vector3int16.new(false)",
             "Enum.Material:FromValue(false)",
             "deserializeModel(1)",
             "deserializePlace(false)",
             "getAuthCookie(1)",
             "getReflectionDatabase():GetClass(1)",
             "implementMethod(1, 'Method', function() end)",
             "implementMethod('Folder', 'Method', 1)",
             "implementProperty('Folder', 'Value', function(target: string) return target end)",
             "registerClass('Custom', false)",
             "registerService(1)",
             "serializeModel({1})",
             "serializePlace(Instance.new('Folder'))",
             "serializeModel({}, 'xml')",
             "studioApplicationPath(1)",
             "studioBuiltinPluginPath(1)",
             "studioContentPath(1)",
             "studioPluginPath(1)",
             "Instance.new('Folder'):WaitForChild('Child')",
             "Vector3.min(Vector3.zero)",
             "Color3.toHSV(Color3.new())",
             "Random.new()",
             "game:GetService('Workspace')",
             "workspace:GetChildren()",
         })
    {
        CAPTURE(source);
        auto path = t.write_child(std::to_string(index++) + ".luau", std::string("--!strict\n") + source);
        auto result = workspace.checkSimple(path, nullptr);
        LUAU_LSP_REQUIRE_ERRORS(result);
    }
}

TEST_CASE("lune_roblox_helper_results_preserve_types_and_optionality")
{
    TempDir t("lune_roblox_invalid_results");
    CliClient client;
    client.globalConfig = Luau::LanguageServer::defaultTestClientConfiguration();
    client.globalConfig.platform.type = LSPPlatformConfig::Lune;
    REQUIRE(addBuiltinDefinitions(client));
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);
    size_t index = 0;
    for (const auto& source : {
             "local value: string = deserializeModel('contents')",
             "local value: string = deserializePlace('contents')",
             "local value: number = serializeModel({})",
             "local value: number = serializePlace(Instance.new('DataModel'))",
             "local value: string = getAuthCookie()",
             "local value: number = getReflectionDatabase().Version",
             "local value: number = studioApplicationPath()",
             "local value: number = studioBuiltinPluginPath()",
             "local value: number = studioContentPath()",
             "local value: number = studioPluginPath()",
             "local value: string = Instance.new('Folder'):FindFirstChild('Child').Name",
             "local value: number = Enum.Material.Plastic.Name",
             "local value: number = getReflectionDatabase():GetClass('Part').Name",
             "local value: string = Instance.new('Folder'):GetAttribute('Dynamic')",
             "implementProperty('Folder', 'Count', function(): number return 1 end, function(target: Instance, value: string) end)",
         })
    {
        CAPTURE(source);
        auto path = t.write_child(std::to_string(index++) + ".luau", std::string("--!strict\n") + source);
        auto result = workspace.checkSimple(path, nullptr);
        LUAU_LSP_REQUIRE_ERRORS(result);
    }
}

TEST_CASE("lune_roblox_helpers_do_not_leak_to_other_platforms")
{
    for (auto platform : {LSPPlatformConfig::Standard, LSPPlatformConfig::Roblox})
    {
        CAPTURE(int(platform));
        TempDir t("lune_roblox_platform_isolation");
        CliClient client;
        client.globalConfig = Luau::LanguageServer::defaultTestClientConfiguration();
        client.globalConfig.platform.type = platform;
        REQUIRE(addBuiltinDefinitions(client));
        WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
        setupCliWorkspace(client, workspace);
        for (const auto& name : luneRobloxExports)
        {
            // Roblox legitimately has the datatype globals, but never Lune's helpers.
            if (platform == LSPPlatformConfig::Roblox && std::isupper(static_cast<unsigned char>(name.front())))
                continue;
            CAPTURE(name);
            auto path = t.write_child(name + ".luau", "--!strict\nreturn " + name);
            auto result = workspace.checkSimple(path, nullptr);
            REQUIRE_FALSE(result.errors.empty());
            CHECK(Luau::get<Luau::UnknownSymbol>(result.errors.front()) != nullptr);
        }
    }
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

TEST_CASE("analyze_file_reports_type_errors")
{
    TempDir t("analyze_cli_type_errors");
    auto filePath = t.write_child("test.luau", R"(
        --!strict
        local x: number = "not a number"
    )");

    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto cr = workspace.checkSimple(filePath, nullptr);
    CHECK(!cr.errors.empty());
}

TEST_CASE("analyze_file_reports_no_errors_for_valid_code")
{
    TempDir t("analyze_cli_no_errors");
    auto filePath = t.write_child("test.luau", R"(
        --!strict
        local x: number = 42
    )");

    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto cr = workspace.checkSimple(filePath, nullptr);
    CHECK(cr.errors.empty());
}

TEST_CASE("definitions_loaded_through_workspace_via_client")
{
    TempDir t("analyze_cli_definitions");
    auto filePath = t.write_child("test.luau", R"(
        --!strict
        local x: Instance
    )");

    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto cr = workspace.checkSimple(filePath, nullptr);
    CHECK(cr.errors.empty());
}

TEST_CASE("sourcemap_loaded_through_workspace_configuration")
{
    TempDir t("analyze_cli_sourcemap");

    auto sourcemapContents = R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "Module",
                        "className": "ModuleScript",
                        "filePaths": ["src/Module.luau"]
                    }
                ]
            }
        ]
    })";

    t.write_child("sourcemap.json", sourcemapContents);
    t.write_child("src/Module.luau", "return {}");

    CliClient client;
    initRobloxCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto* robloxPlatform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    REQUIRE(robloxPlatform);
    CHECK(robloxPlatform->rootSourceNode != nullptr);
}

TEST_CASE("annotate_retains_type_graphs")
{
    TempDir t("analyze_cli_annotate");
    auto filePath = t.write_child("test.luau", R"(
local function add(a: number, b: number): number
    return a + b
end
    )");

    CliClient client;
    initCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    Luau::ModuleName name = filePath;
    workspace.checkStrict(name, nullptr, /* forAutocomplete= */ false);

    Luau::SourceModule* sm = workspace.frontend.getSourceModule(name);
    Luau::ModulePtr m = workspace.getModule(name);
    REQUIRE(sm);
    REQUIRE(m);

    // attachTypeData requires retained type graphs — would produce empty output
    // if checkSimple (which discards graphs) was used instead
    Luau::attachTypeData(*sm, *m);
    std::string annotated = Luau::prettyPrintWithTypes(*sm->root);
    CHECK(annotated.find("number") != std::string::npos);
}

TEST_CASE("analyze_resolves_game_requires_with_sourcemap")
{
    // Regression test for https://github.com/JohnnyMorganz/luau-lsp/issues/1473
    TempDir t("analyze_game_requires");

    // ServerModule and Util are at completely different filesystem locations but are
    // DataModel siblings — only the sourcemap knows they are related.
    t.write_child("sourcemap.json", R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "Util",
                        "className": "ModuleScript",
                        "filePaths": ["packages/util/Util.luau"]
                    }
                ]
            },
            {
                "name": "ServerScriptService",
                "className": "ServerScriptService",
                "children": [
                    {
                        "name": "ServerModule",
                        "className": "ModuleScript",
                        "filePaths": ["src/server/ServerModule.luau"]
                    }
                ]
            }
        ]
    })");
    t.write_child("packages/util/Util.luau", "return { value = 42 }");
    t.write_child("src/server/ServerModule.luau", "local _ = require('@game/ReplicatedStorage/Util')");

    CliClient client;
    initRobloxCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto serverModulePath = Uri::file(t.path() + "/src/server/ServerModule.luau").fsPath();
    CHECK(analyzeFile(workspace, serverModulePath, ReportFormat::Default, false));
}

TEST_CASE("analyze_resolves_non_mirrored_relative_requires_with_sourcemap")
{
    // Variant of the above: relative require between DataModel siblings whose
    // filesystem paths are non-mirrored (packages/core/ vs packages/combat/).
    TempDir t("analyze_non_mirrored_relative");

    t.write_child("sourcemap.json", R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "ModuleA",
                        "className": "ModuleScript",
                        "filePaths": ["packages/core/ModuleA.luau"]
                    },
                    {
                        "name": "ModuleB",
                        "className": "ModuleScript",
                        "filePaths": ["packages/combat/ModuleB.luau"]
                    }
                ]
            }
        ]
    })");
    t.write_child("packages/core/ModuleA.luau", "local _ = require('./ModuleB')");
    t.write_child("packages/combat/ModuleB.luau", "return {}");

    CliClient client;
    initRobloxCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto moduleAPath = Uri::file(t.path() + "/packages/core/ModuleA.luau").fsPath();
    CHECK(analyzeFile(workspace, moduleAPath, ReportFormat::Default, false));
}

TEST_CASE("analyze_resolves_game_requires_from_file_outside_sourcemap")
{
    // `@game` is absolute, so it must resolve from build output that the sourcemap does not cover.
    TempDir t("analyze_game_requires_outside_sourcemap");

    t.write_child("sourcemap.json", R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [
                    {
                        "name": "Util",
                        "className": "ModuleScript",
                        "filePaths": ["packages/util/Util.luau"]
                    }
                ]
            }
        ]
    })");
    t.write_child("packages/util/Util.luau", "return { value = 42 }");
    // dist/ has no sourcemap node.
    t.write_child("dist/main.luau", "local _ = require('@game/ReplicatedStorage/Util')");

    CliClient client;
    initRobloxCliClient(client);
    WorkspaceFolder workspace(&client, "CLI", Uri::file(t.path()), std::nullopt);
    setupCliWorkspace(client, workspace);

    auto mainPath = Uri::file(t.path() + "/dist/main.luau").fsPath();
    CHECK(analyzeFile(workspace, mainPath, ReportFormat::Default, false));
}

TEST_SUITE_END();
