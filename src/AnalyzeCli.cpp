// Source: https://github.com/Roblox/luau/blob/master/CLI/Analyze.cpp; licensed under MIT License
#include "Analyze/AnalyzeCli.hpp"
#include "Analyze/CliConfigurationParser.hpp"
#include "Analyze/CliClient.hpp"
#include "Flags.hpp"

#include "LSP/Workspace.hpp"
#include "LSP/ClientConfiguration.hpp"
#include "Luau/TypeAttach.h"
#include "Luau/PrettyPrinter.h"
#include "LuauFileUtils.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/Utils.hpp"
#include "RobloxTypes.hpp"
#include "LuneTypes.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

LUAU_FASTFLAG(DebugLuauTimeTracing)
LUAU_FASTFLAG(LuauSolverV2)

static void report(ReportFormat format, const char* name, const Luau::Location& loc, const char* type, const char* message)
{
    switch (format)
    {
    case ReportFormat::Default:
        fprintf(stderr, "%s(%d,%d): %s: %s\n", name, loc.begin.line + 1, loc.begin.column + 1, type, message);
        break;

    case ReportFormat::Luacheck:
    {
        // Note: luacheck's end column is inclusive but our end column is exclusive
        // In addition, luacheck doesn't support multi-line messages, so if the error is multiline we'll fake end column as 100 and hope for the best
        unsigned int columnEnd = (loc.begin.line == loc.end.line) ? loc.end.column : 100;

        // Use stdout to match luacheck behavior
        fprintf(stdout, "%s:%d:%d-%d: (W0) %s: %s\n", name, loc.begin.line + 1, loc.begin.column + 1, columnEnd, type, message);
        break;
    }

    case ReportFormat::Gnu:
        // Note: GNU end column is inclusive but our end column is exclusive
        fprintf(stderr, "%s:%d.%d-%d.%d: %s: %s\n", name, loc.begin.line + 1, loc.begin.column + 1, loc.end.line + 1, loc.end.column, type, message);
        break;
    }
}

FilePathInformation getFilePath(const WorkspaceFileResolver* fileResolver, const Luau::ModuleName& moduleName)
{
    auto path = fileResolver->platform->resolveToRealPath(moduleName);
    LUAU_ASSERT(path);

    // For consistency, we want to map the error.moduleName to a relative path (if it is a real path)
    Luau::ModuleName errorFriendlyName = moduleName;
    if (!fileResolver->platform->isVirtualPath(moduleName))
        errorFriendlyName = path->lexicallyRelative(fileResolver->rootUri);

    return {*path, fileResolver->getHumanReadableModuleName(errorFriendlyName)};
}

static bool reportError(WorkspaceFolder& workspace, ReportFormat format, const Luau::TypeError& error)
{
    // The type checker can produce errors with empty module names for internal type artifacts.
    // Skip these since they don't correspond to any real source file.
    if (error.moduleName.empty())
        return false;

    auto [uri, relativePath] = getFilePath(&workspace.fileResolver, error.moduleName);

    if (workspace.isIgnoredFile(uri))
        return false;

    if (const auto* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
        report(format, relativePath.c_str(), error.location, "SyntaxError", syntaxError->message.c_str());
    else
        report(format, relativePath.c_str(), error.location, "TypeError",
            Luau::toString(error, Luau::TypeErrorToStringOptions{workspace.frontend.fileResolver}).c_str());

    return true;
}

static void reportWarning(ReportFormat format, const char* name, const Luau::LintWarning& warning)
{
    report(format, name, warning.location, Luau::LintWarning::getName(warning.code), warning.text.c_str());
}

AnalysisJsonlReporter::AnalysisJsonlReporter(std::ostream& output)
    : output(output)
    , started(std::chrono::steady_clock::now())
{
}

void AnalysisJsonlReporter::emit(json event)
{
    event["schemaVersion"] = 1;
    output << event.dump() << '\n' << std::flush;
}

void AnalysisJsonlReporter::start(const std::string& version, const std::string& platform, const std::string& solver)
{
    emit({{"event", "start"}, {"analyzerVersion", version}, {"platform", platform}, {"solver", solver}, {"positionEncoding", "utf-16"}});
}

void AnalysisJsonlReporter::progress(
    const std::string& phase, const std::string& state, size_t completed, std::optional<size_t> total, const std::string& path)
{
    json event = {{"event", "progress"}, {"phase", phase}, {"state", state}, {"completed", completed}, {"total", nullptr}};
    if (total)
        event["total"] = *total;
    if (!path.empty())
        event["path"] = path;
    emit(std::move(event));
}

bool AnalysisJsonlReporter::diagnostic(const std::string& path, const std::string& category, const lsp::Diagnostic& diagnostic)
{
    const bool isError = diagnostic.severity == lsp::DiagnosticSeverity::Error;
    json event = {{"event", "diagnostic"}, {"path", path}, {"category", category}, {"severity", isError ? "error" : "warning"},
        {"code", std::get<int>(diagnostic.code.value())}, {"range", diagnostic.range}, {"message", diagnostic.message}};
    if (!diagnostics.insert(event.dump()).second)
        return false;
    if (isError)
        errors += 1;
    else
        warnings += 1;
    emit(std::move(event));
    return true;
}

void AnalysisJsonlReporter::finish(size_t completed, size_t total)
{
    if (completed != total)
        throw std::logic_error("Cannot complete an analysis with unfinished files.");
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
    emit({{"event", "summary"}, {"complete", true}, {"filesCompleted", completed}, {"filesTotal", total},
        {"diagnosticCount", diagnostics.size()}, {"errors", errors}, {"warnings", warnings}, {"elapsedMilliseconds", elapsed}});
}

size_t AnalysisJsonlReporter::errorCount() const
{
    return errors;
}

size_t AnalysisJsonlReporter::warningCount() const
{
    return warnings;
}

static std::string structuredPath(const WorkspaceFolder& workspace, const Uri& uri)
{
    const auto path = uri.lexicallyRelative(workspace.rootUri);
    if (path.empty() || path == ".." || Luau::startsWith(path, "../"))
        throw std::runtime_error("Cannot report a source file outside the analysis workspace.");
    return path;
}

// Normalize referenced module names without rewriting user-authored type literals.
class AnalysisModuleNames : public Luau::NullFileResolver
{
public:
    explicit AnalysisModuleNames(const WorkspaceFolder& workspace)
        : workspace(workspace)
    {
    }

    std::string getHumanReadableModuleName(const Luau::ModuleName& name) const override
    {
        auto label = workspace.fileResolver.getHumanReadableModuleName(name);
        auto prefix = workspace.rootUri.fsPath();
        if (!prefix.empty() && prefix.back() != '/' && prefix.back() != '\\')
            prefix += '/';
        std::replace(prefix.begin(), prefix.end(), '\\', '/');
        std::replace(label.begin(), label.end(), '\\', '/');
        if (Luau::startsWith(label, prefix))
            label.erase(0, prefix.size());
        return label;
    }

private:
    const WorkspaceFolder& workspace;
};

bool reportTypeErrorJsonl(WorkspaceFolder& workspace, const Luau::TypeError& error, AnalysisJsonlReporter& reporter)
{
    if (error.moduleName.empty())
        return true;
    const auto [uri, _] = getFilePath(&workspace.fileResolver, error.moduleName);
    if (workspace.isIgnoredFile(uri))
        return true;
    const auto textDocument = workspace.fileResolver.getOrCreateTextDocumentFromModuleName(error.moduleName);
    if (!textDocument)
    {
        std::cerr << "Cannot read the source for an analysis diagnostic.\n";
        return false;
    }
    AnalysisModuleNames moduleNames(workspace);
    const auto category = Luau::get_if<Luau::SyntaxError>(&error.data) ? "SyntaxError" : "TypeError";
    reporter.diagnostic(structuredPath(workspace, uri), category, createTypeErrorDiagnostic(error, &moduleNames, *textDocument));
    return true;
}

bool analyzeFileJsonl(WorkspaceFolder& workspace, const std::string& path, AnalysisJsonlReporter& reporter)
{
    const auto name = workspace.fileResolver.getModuleName(Uri::file(path));
    workspace.frontend.parse(name);
    if (!workspace.frontend.getSourceModule(name))
    {
        std::cerr << "Error opening " << name << "\n";
        return false;
    }
    Luau::FrontendOptions options{/* retainFullTypeGraphs: */ false, /* forAutocomplete: */ false, /* runLintChecks: */ true};
    auto result = workspace.frontend.check(name, options);

    for (const auto& error : result.errors)
    {
        if (!reportTypeErrorJsonl(workspace, error, reporter))
            return false;
    }

    const auto textDocument = workspace.fileResolver.getOrCreateTextDocumentFromModuleName(name);
    if (!textDocument)
    {
        std::cerr << "Cannot read the source for analysis lint results.\n";
        return false;
    }
    const auto sourcePath = structuredPath(workspace, Uri::file(path));
    for (const auto& error : result.lintResult.errors)
    {
        auto diagnostic = createLintDiagnostic(error, *textDocument);
        diagnostic.severity = lsp::DiagnosticSeverity::Error;
        reporter.diagnostic(sourcePath, Luau::LintWarning::getName(error.code), diagnostic);
    }
    for (const auto& warning : result.lintResult.warnings)
        reporter.diagnostic(sourcePath, Luau::LintWarning::getName(warning.code), createLintDiagnostic(warning, *textDocument));
    return true;
}

bool analyzeFilesJsonl(WorkspaceFolder& workspace, const std::vector<std::string>& paths, AnalysisJsonlReporter& reporter)
{
    size_t completed = 0;
    for (const auto& path : paths)
    {
        const auto relativePath = structuredPath(workspace, Uri::file(path));
        reporter.progress("checking", "started", completed, paths.size(), relativePath);
        if (!analyzeFileJsonl(workspace, path, reporter))
            return false;
        completed += 1;
        reporter.progress("checking", "completed", completed, paths.size(), relativePath);
    }
    return true;
}

bool analyzeFile(WorkspaceFolder& workspace, const std::string& path, ReportFormat format, bool annotate)
{
    Luau::ModuleName name = workspace.fileResolver.getModuleName(Uri::file(path));

    // Use checkStrict when annotating to retain type graphs needed by attachTypeData
    Luau::CheckResult cr = annotate ? workspace.checkStrict(name, /* cancellationToken= */ nullptr, /* forAutocomplete= */ false)
                                    : workspace.checkSimple(name, /* cancellationToken= */ nullptr);

    if (!workspace.frontend.getSourceModule(name))
    {
        std::cerr << "Error opening " << name << "\n";
        return false;
    }

    unsigned int reportedErrors = 0;
    for (auto& error : cr.errors)
        reportedErrors += reportError(workspace, format, error);

    // For the human readable module name, we use a relative version
    auto [_, relativePath] = getFilePath(&workspace.fileResolver, path);
    for (auto& error : cr.lintResult.errors)
        reportWarning(format, relativePath.c_str(), error);
    for (auto& warning : cr.lintResult.warnings)
        reportWarning(format, relativePath.c_str(), warning);

    if (annotate)
    {
        Luau::SourceModule* sm = workspace.frontend.getSourceModule(name);
        Luau::ModulePtr m = workspace.getModule(name);

        Luau::attachTypeData(*sm, *m);

        std::string annotated = Luau::prettyPrintWithTypes(*sm->root);

        printf("%s", annotated.c_str());
    }

    return reportedErrors == 0 && cr.lintResult.errors.empty();
}

std::vector<std::string> getFilesToAnalyze(const std::vector<std::string>& paths, WorkspaceFolder* workspace)
{
    auto cwd = Luau::FileUtils::getCurrentWorkingDirectory();
    LUAU_ASSERT(cwd);
    auto cwdUri = Uri::file(*cwd);

    std::vector<std::string> files;
    for (const auto& pathString : paths)
    {
        Uri uri = cwdUri.resolvePath(pathString);
        if (!uri.exists())
        {
            throw std::runtime_error("Cannot get " + uri.fsPath() + ": path does not exist");
        }

        if (uri.isDirectory())
        {
            Luau::FileUtils::traverseDirectoryRecursive(uri.fsPath(),
                [&](const auto& path)
                {
                    auto uri = Uri::file(path);
                    auto ext = uri.extension();
                    if (ext == ".lua" || ext == ".luau")
                    {
                        if (!workspace || !workspace->isIgnoredFile(uri))
                            files.push_back(uri.fsPath());
                    }
                });
        }
        else
        {
            files.push_back(uri.fsPath());
        }
    }
    std::sort(files.begin(), files.end(), [](const auto& left, const auto& right)
        {
            const auto leftPath = Uri::file(left).path;
            const auto rightPath = Uri::file(right).path;
#if defined(_WIN32) || defined(__APPLE__)
            const auto leftKey = toLower(leftPath);
            const auto rightKey = toLower(rightPath);
            if (leftKey != rightKey)
                return leftKey < rightKey;
#endif
            return leftPath < rightPath;
        });
    files.erase(std::unique(files.begin(), files.end(), [](const auto& left, const auto& right)
                    { return Uri::file(left) == Uri::file(right); }),
        files.end());
    return files;
}

void applySettings(const std::string& settingsContents, CliClient& client)
{
    client.globalConfig = dottedToClientConfiguration(settingsContents);

    // Merge definitions from settings into client.definitionsFiles
    for (const auto& pair : client.globalConfig.types.definitionFiles)
        client.definitionsFiles.insert(pair);

    // Process any fflags
    if (client.globalConfig.fflags.enableNewSolver)
        FFlag::LuauSolverV2.value = true;
    registerFastFlagsCLI(client.globalConfig.fflags.override);
    if (!client.globalConfig.fflags.enableByDefault)
        std::cerr << "warning: `luau-lsp.fflags.enableByDefault` is not respected in CLI Analyze mode. Please instead use the CLI option "
                     "`--no-flags-enabled` to configure this.\n";
    if (client.globalConfig.fflags.sync)
        std::cerr << "warning: `luau-lsp.fflags.sync` is not supported in CLI Analyze mode. Instead, all FFlags are enabled by default. "
                     "Please manually configure necessary FFlags\n";
}

std::unordered_map<std::string, std::string> processDefinitionsFilePaths(const argparse::ArgumentParser& program)
{
    std::unordered_map<std::string, std::string> definitionsFiles{};
    size_t backwardsCompatibilityNameSuffix = 0;
    for (const auto& definition : program.get<std::vector<std::string>>("--definitions"))
    {
        std::string packageName = definition;
        std::string filePath = definition;

        size_t eqIndex = definition.find('=');
        if (eqIndex == std::string::npos)
        {
            // TODO: Remove Me - backwards compatibility
            packageName = "@roblox";
            if (backwardsCompatibilityNameSuffix > 0)
                packageName += std::to_string(backwardsCompatibilityNameSuffix);
            backwardsCompatibilityNameSuffix += 1;
        }
        else
        {
            packageName = definition.substr(0, eqIndex);
            filePath = definition.substr(eqIndex + 1, definition.length());
        }

        if (!Luau::startsWith(packageName, "@"))
            packageName = "@" + packageName;

        definitionsFiles.emplace(packageName, filePath);
    }

    return definitionsFiles;
}

bool addBuiltinDefinitions(CliClient& client)
{
    // Fall back to the built-in Roblox definitions when no definitions files are provided
    if (client.globalConfig.platform.type == LSPPlatformConfig::Roblox && client.definitionsFiles.empty())
    {
        if (ROBLOX_GLOBAL_TYPES == nullptr)
            return false;
        client.builtinDefinitions.emplace_back("@roblox", ROBLOX_GLOBAL_TYPES);
    }

    if (client.globalConfig.platform.type == LSPPlatformConfig::Lune)
        client.builtinDefinitions.emplace_back("@lune", LUNE_GLOBAL_TYPES);

    return true;
}

int startAnalyze(const argparse::ArgumentParser& program)
{
    ReportFormat format = ReportFormat::Default;
    bool annotate = program.is_used("--annotate");
    auto sourcemapPath = program.present<std::string>("--sourcemap");
    auto baseLuaurc = program.present<std::string>("--base-luaurc");
    auto settingsPath = program.present<std::string>("--settings");
    FFlag::DebugLuauTimeTracing.value = program.is_used("--timetrace");

    CliClient client;

    auto currentWorkingDirectory = Luau::FileUtils::getCurrentWorkingDirectory();
    if (!currentWorkingDirectory)
    {
        fprintf(stderr, "Failed to determine current working directory\n");
        return 1;
    }

    if (settingsPath)
    {
        if (std::optional<std::string> contents = Luau::FileUtils::readFile(*settingsPath))
        {
            applySettings(contents.value(), client);
        }
        else
        {
            fprintf(stderr, "Failed to read settings at '%s'\n", settingsPath->c_str());
            return 1;
        }
    }

    // Apply CLI args after settings so they take precedence
    auto cliIgnoreGlobs = program.get<std::vector<std::string>>("--ignore");
    client.globalConfig.ignoreGlobs.insert(client.globalConfig.ignoreGlobs.end(), cliIgnoreGlobs.begin(), cliIgnoreGlobs.end());
    for (const auto& [key, value] : processDefinitionsFilePaths(program))
        client.definitionsFiles.insert_or_assign(key, value);
    if (program.get<bool>("--enable-new-solver"))
    {
        client.globalConfig.fflags.enableNewSolver = true;
        FFlag::LuauSolverV2.value = true;
    }

    auto filesArg = program.present<std::vector<std::string>>("files");
    if (!filesArg || filesArg->empty())
    {
        fprintf(stderr, "error: no files provided\n");
        return 1;
    }

    auto reportFormatter = program.get<std::string>("--formatter");
    const bool jsonl = reportFormatter == "jsonl";
    if (jsonl && annotate)
    {
        fprintf(stderr, "Cannot combine --formatter=jsonl with --annotate.\n");
        return 1;
    }
    if (reportFormatter == "default")
        format = ReportFormat::Default;
    else if (reportFormatter == "plain")
        format = ReportFormat::Luacheck;
    else if (reportFormatter == "gnu")
        format = ReportFormat::Gnu;

#if !defined(LUAU_ENABLE_TIME_TRACE)
    if (FFlag::DebugLuauTimeTracing)
    {
        fprintf(jsonl ? stderr : stdout, "To run with --timetrace, Luau has to be built with LUAU_ENABLE_TIME_TRACE enabled\n");
        return 1;
    }
#endif

    if (auto platformArg = program.present("--platform"))
    {
        if (platformArg == "standard")
            client.globalConfig.platform.type = LSPPlatformConfig::Standard;
        else if (platformArg == "roblox")
            client.globalConfig.platform.type = LSPPlatformConfig::Roblox;
        else if (platformArg == "lune")
            client.globalConfig.platform.type = LSPPlatformConfig::Lune;
    }

    if (!addBuiltinDefinitions(client))
    {
        fprintf(stderr, "WARNING: --platform is set to 'roblox' but no definitions files are provided and no built-in definitions are "
                        "available; use `--platform=standard` to silence\n");
        if (jsonl)
            return 1;
    }

    // Configure sourcemap via configuration (handled by RobloxPlatform::setupWithConfiguration)
    if (sourcemapPath)
    {
        if (client.globalConfig.platform.type == LSPPlatformConfig::Roblox)
        {
            client.globalConfig.sourcemap.sourcemapFile = *sourcemapPath;
            client.globalConfig.sourcemap.enabled = true;
        }
        else
        {
            std::cerr << "warning: a sourcemap was provided, but the current platform is not `roblox`. Use `--platform roblox` to ensure the "
                         "sourcemap option is respected.\n";
        }
    }
    else
    {
        client.globalConfig.sourcemap.enabled = false;
    }

    // Handle deprecated --no-strict-dm-types flag via configuration
    if (program.is_used("--no-strict-dm-types"))
        client.globalConfig.diagnostics.strictDatamodelTypes = false;

    client.globalConfig.index.enabled = false;

    std::optional<Luau::Config> defaultConfig;
    if (baseLuaurc)
    {
        Luau::Config result;
        if (std::optional<std::string> contents = Luau::FileUtils::readFile(*baseLuaurc))
        {
            std::optional<std::string> error = WorkspaceFileResolver::parseConfig(Uri::file(*baseLuaurc), *contents, result);
            if (error)
            {
                fprintf(stderr, "%s: %s\n", baseLuaurc->c_str(), error->c_str());
                return 1;
            }
            defaultConfig = std::move(result);
        }
        else
        {
            fprintf(stderr, "Failed to read base .luaurc configuration at '%s'\n", baseLuaurc->c_str());
            return 1;
        }
    }

    std::unique_ptr<AnalysisJsonlReporter> reporter;
    if (jsonl)
    {
        reporter = std::make_unique<AnalysisJsonlReporter>(std::cout);
        const auto platform = client.globalConfig.platform.type == LSPPlatformConfig::Roblox ? "roblox"
            : client.globalConfig.platform.type == LSPPlatformConfig::Lune                   ? "lune"
                                                                                          : "standard";
        reporter->start(LSP_VERSION, platform, FFlag::LuauSolverV2 ? "new" : "old");
        reporter->progress("setup", "started", 0, std::nullopt);
    }

    auto rootUri = Uri::file(*currentWorkingDirectory);
    WorkspaceFolder workspace(&client, "CLI", rootUri, defaultConfig);
    workspace.setupWithConfiguration(client.globalConfig);
    workspace.isReady = true;
    if (reporter)
    {
        if (client.hasErrorLog)
            return 1;
        reporter->progress("setup", "completed", 0, std::nullopt);
        reporter->progress("discovery", "started", 0, std::nullopt);
    }

    std::vector<std::string> files;
    try
    {
        files = getFilesToAnalyze(*filesArg, &workspace);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }

    if (files.empty())
    {
        fprintf(stderr, "error: no files provided\n");
        return 1;
    }

    int failed = 0;
    if (reporter)
    {
        reporter->progress("discovery", "completed", 0, files.size());
        try
        {
            if (!analyzeFilesJsonl(workspace, files, *reporter))
                return 1;
        }
        catch (const std::exception& error)
        {
            std::cerr << error.what() << '\n';
            return 1;
        }
        failed = reporter->errorCount() > 0 ? 1 : 0;
    }
    else
    {
        for (const auto& path : files)
            failed += !analyzeFile(workspace, path, format, annotate);
    }

    if (!client.diagnostics.empty())
    {
        failed += int(client.diagnostics.size());

        for (const auto& [path, err] : client.diagnostics)
            fprintf(stderr, "%s: %s\n", path.fsPath().c_str(), err.c_str());
    }

    if (reporter && client.hasErrorLog)
        return 1;
    if (reporter && client.diagnostics.empty())
        reporter->finish(files.size(), files.size());

    if (format == ReportFormat::Luacheck)
        return 0;
    else
        return failed ? 1 : 0;
}
