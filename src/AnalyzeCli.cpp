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
#include "RobloxTypes.hpp"
#include <iostream>
#include <memory>
#include <vector>

LUAU_FASTFLAG(DebugLuauTimeTracing)
LUAU_FASTFLAG(LuauSolverV2)

static const char* LUNE_GLOBAL_TYPES = R"LUAU(
declare extern type Color3 with
	B: number
	G: number
	R: number
	b: number
	function Lerp(self, color: Color3, alpha: number): Color3
	function ToHSV(self): (number, number, number)
	function ToHex(self): string
	function lerp(self, color: Color3, alpha: number): Color3
	g: number
	r: number
end

declare extern type Vector2 with
	Magnitude: number
	Unit: Vector2
	X: number
	Y: number
	function Abs(self): Vector2
	function Angle(self, other: Vector2, isSigned: boolean): number
	function Ceil(self): Vector2
	function Cross(self, other: Vector2): number
	function Dot(self, v: Vector2): number
	function Floor(self): Vector2
	function FuzzyEq(self, other: Vector2, epsilon: number?): boolean
	function Lerp(self, v: Vector2, alpha: number): Vector2
	function Max(self, ...: (Vector2)): Vector2
	function Min(self, ...: (Vector2)): Vector2
	function Sign(self): Vector2
	function __add(self, other: Vector2): Vector2
	function __div(self, other: Vector2 | number): Vector2
	function __mul(self, other: Vector2 | number): Vector2
	function __sub(self, other: Vector2): Vector2
	function __unm(self): Vector2
	function lerp(self, v: Vector2, alpha: number): Vector2
	magnitude: number
	unit: Vector2
	x: number
	y: number
end

declare extern type Vector3 with
	Magnitude: number
	Unit: Vector3
	X: number
	Y: number
	Z: number
	function Abs(self): Vector3
	function Angle(self, other: Vector3, axis: Vector3?): number
	function Ceil(self): Vector3
	function Cross(self, other: Vector3): Vector3
	function Dot(self, other: Vector3): number
	function Floor(self): Vector3
	function FuzzyEq(self, other: Vector3, epsilon: number?): boolean
	function Lerp(self, goal: Vector3, alpha: number): Vector3
	function Max(self, ...: (Vector3)): Vector3
	function Min(self, ...: (Vector3)): Vector3
	function Sign(self): Vector3
	function __add(self, other: Vector3): Vector3
	function __div(self, other: Vector3 | number): Vector3
	function __mul(self, other: Vector3 | number): Vector3
	function __sub(self, other: Vector3): Vector3
	function __unm(self): Vector3
	function lerp(self, goal: Vector3, alpha: number): Vector3
	magnitude: number
	unit: Vector3
	x: number
	y: number
	z: number
end

declare extern type CFrame with
	LookVector: Vector3
	Position: Vector3
	RightVector: Vector3
	Rotation: CFrame
	UpVector: Vector3
	X: number
	XVector: Vector3
	Y: number
	YVector: Vector3
	Z: number
	ZVector: Vector3
	function FuzzyEq(self, other: CFrame, epsilon: number?): boolean
	function GetComponents(self): (number, number, number, number, number, number, number, number, number, number, number, number)
	function Inverse(self): CFrame
	function Lerp(self, goal: CFrame, alpha: number): CFrame
	function Orthonormalize(self): CFrame
	function PointToObjectSpace(self, v3: Vector3): Vector3
	function PointToWorldSpace(self, v3: Vector3): Vector3
	function ToAxisAngle(self): (Vector3, number)
	function ToEulerAnglesXYZ(self): (number, number, number)
	function ToEulerAnglesYXZ(self): (number, number, number)
	function ToObjectSpace(self, cf: CFrame): CFrame
	function ToOrientation(self): (number, number, number)
	function ToWorldSpace(self, cf: CFrame): CFrame
	function VectorToObjectSpace(self, v3: Vector3): Vector3
	function VectorToWorldSpace(self, v3: Vector3): Vector3
	function __add(self, other: Vector3): CFrame
	function __mul(self, other: CFrame): CFrame
	function __mul(self, other: Vector3): Vector3
	function __sub(self, other: Vector3): CFrame
	function components(self): (number, number, number, number, number, number, number, number, number, number, number, number)
	function inverse(self): CFrame
	function lerp(self, goal: CFrame, alpha: number): CFrame
	lookVector: Vector3
	p: Vector3
	rightVector: Vector3
	upVector: Vector3
	x: number
	y: number
	z: number
end

declare extern type NumberRange with
	Max: number
	Min: number
end

declare Color3: {
	fromRGB: ((red: number?, green: number?, blue: number?) -> Color3),
	fromHSV: ((hue: number, saturation: number, value: number) -> Color3),
	toHSV: ((color: Color3) -> (number, number, number)),
	new: ((red: number?, green: number?, blue: number?) -> Color3),
	fromHex: ((hex: string) -> Color3),
}

declare Vector2: {
	zero: Vector2,
	one: Vector2,
	xAxis: Vector2,
	yAxis: Vector2,
	new: ((x: number?, y: number?) -> Vector2),
	min: ((...Vector2) -> Vector2),
	max: ((...Vector2) -> Vector2),
}

declare Vector3: {
	zero: Vector3,
	one: Vector3,
	xAxis: Vector3,
	yAxis: Vector3,
	zAxis: Vector3,
	fromNormalId: ((normal: any) -> Vector3),
	fromAxis: ((axis: any) -> Vector3),
	new: ((x: number?, y: number?, z: number?) -> Vector3),
	min: ((...Vector3) -> Vector3),
	max: ((...Vector3) -> Vector3),
}

declare CFrame: {
	identity: CFrame,
	fromEulerAnglesYXZ: ((rx: number, ry: number, rz: number) -> CFrame),
	fromEulerAngles: ((rx: number, ry: number, rz: number) -> CFrame),
	Angles: ((rx: number, ry: number, rz: number) -> CFrame),
	fromMatrix: ((pos: Vector3, vX: Vector3, vY: Vector3, vZ: Vector3?) -> CFrame),
	fromAxisAngle: ((v: Vector3, r: number) -> CFrame),
	fromOrientation: ((rx: number, ry: number, rz: number) -> CFrame),
	fromEulerAnglesXYZ: ((rx: number, ry: number, rz: number) -> CFrame),
	lookAt: ((at: Vector3, target: Vector3, up: Vector3?) -> CFrame),
	lookAlong: ((at: Vector3, direction: Vector3, up: Vector3?) -> CFrame),
	new: (() -> CFrame) & ((pos: Vector3) -> CFrame) & ((pos: Vector3, lookAt: Vector3) -> CFrame) & ((x: number, y: number, z: number) -> CFrame) & ((x: number, y: number, z: number, qX: number, qY: number, qZ: number, qW: number) -> CFrame),
}

declare NumberRange: {
	new: ((value: number) -> NumberRange) & ((min: number, max: number) -> NumberRange),
}

declare script: {
	Name: string,
	Parent: any,
	GetFullName: (self: any) -> string,
	[string]: any,
}

declare executable: string?

declare function warn<T...>(...: T...)

declare task: {
	cancel: (thread: thread) -> (),
	defer: <A..., R...>(threadOrFunction: thread | (A...) -> R..., A...) -> thread,
	delay: <A..., R...>(duration: number?, threadOrFunction: thread | (A...) -> R..., A...) -> thread,
	spawn: <A..., R...>(threadOrFunction: thread | (A...) -> R..., A...) -> thread,
	wait: (duration: number?) -> number,
}
)LUAU";

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
            std::cerr << "Cannot get " << uri.fsPath() << ": path does not exist\n";
            exit(1);
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
    if (reportFormatter == "default")
        format = ReportFormat::Default;
    else if (reportFormatter == "plain")
        format = ReportFormat::Luacheck;
    else if (reportFormatter == "gnu")
        format = ReportFormat::Gnu;

#if !defined(LUAU_ENABLE_TIME_TRACE)
    if (FFlag::DebugLuauTimeTracing)
    {
        printf("To run with --timetrace, Luau has to be built with LUAU_ENABLE_TIME_TRACE enabled\n");
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
        fprintf(stderr, "WARNING: --platform is set to 'roblox' but no definitions files are provided and no built-in definitions are "
                        "available; use `--platform=standard` to silence\n");

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

    auto rootUri = Uri::file(*currentWorkingDirectory);
    WorkspaceFolder workspace(&client, "CLI", rootUri, defaultConfig);
    workspace.setupWithConfiguration(client.globalConfig);
    workspace.isReady = true;

    auto files = getFilesToAnalyze(*filesArg, &workspace);

    if (files.empty())
    {
        fprintf(stderr, "error: no files provided\n");
        return 1;
    }

    int failed = 0;

    for (const auto& path : files)
        failed += !analyzeFile(workspace, path, format, annotate);

    if (!client.diagnostics.empty())
    {
        failed += int(client.diagnostics.size());

        for (const auto& [path, err] : client.diagnostics)
            fprintf(stderr, "%s: %s\n", path.fsPath().c_str(), err.c_str());
    }

    if (format == ReportFormat::Luacheck)
        return 0;
    else
        return failed ? 1 : 0;
}
