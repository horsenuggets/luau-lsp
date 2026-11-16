#include "Platform/LSPPlatform.hpp"

#include "LuauFileUtils.hpp"
#include "LSP/ClientConfiguration.hpp"
#include "LSP/Workspace.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "Platform/StringRequireSuggester.hpp"
#include "Platform/StringRequireAutoImporter.hpp"

#include "Luau/StringUtils.h"
#include "Luau/TimeTrace.h"
#include <filesystem>
#include <memory>
#include <unordered_set>

LSPPlatform::LSPPlatform(WorkspaceFileResolver* fileResolver, WorkspaceFolder* workspaceFolder)
    : fileResolver(fileResolver)
    , workspaceFolder(workspaceFolder)
{
}

std::unique_ptr<LSPPlatform> LSPPlatform::getPlatform(
    const ClientConfiguration& config, WorkspaceFileResolver* fileResolver, WorkspaceFolder* workspaceFolder)
{
    if (config.types.roblox && config.platform.type == LSPPlatformConfig::Roblox)
        return std::make_unique<RobloxPlatform>(fileResolver, workspaceFolder);

    return std::make_unique<LSPPlatform>(fileResolver, workspaceFolder);
}

std::unique_ptr<Luau::RequireSuggester> LSPPlatform::getRequireSuggester()
{
    return std::make_unique<StringRequireSuggester>(workspaceFolder, fileResolver, this);
}

std::optional<Uri> LSPPlatform::resolveToRealPath(const Luau::ModuleName& name) const
{
    // Caution: if `isVirtualPath` returns true, then this would infinite loop
    // But, we want to use the default WorkspaceFileResolver::getUri() function in case that changes in the future
    return fileResolver->getUri(name);
}

std::optional<std::string> LSPPlatform::readSourceCode(const Luau::ModuleName& name, const Uri& path) const
{
    LUAU_TIMETRACE_SCOPE("LSPPlatform::readSourceCode", "LSP");

    if (path.extension() == ".lua" || path.extension() == ".luau")
        return Luau::FileUtils::readFile(path.fsPath());

    return std::nullopt;
}

Uri resolveAliasLocation(const Luau::Config::AliasInfo& aliasInfo)
{
    return Uri::file(aliasInfo.configLocation).resolvePath(resolvePath(aliasInfo.value));
}

static std::optional<Uri> resolveAliasWithCycleCheck(
    const std::string& path, const Luau::Config& config, const Uri& from, std::unordered_set<std::string>& visited)
{
    if (path.size() < 1 || path[0] != '@')
        return std::nullopt;

    // To ignore the '@' alias prefix when processing the alias
    const size_t aliasStartPos = 1;

    // If a directory separator was found, the length of the alias is the
    // distance between the start of the alias and the separator. Otherwise,
    // the whole string after the alias symbol is the alias.
    size_t aliasLen = path.find_first_of("\\/");
    if (aliasLen != std::string::npos)
        aliasLen -= aliasStartPos;

    std::string potentialAlias = path.substr(aliasStartPos, aliasLen);

    // Not worth searching when potentialAlias cannot be an alias
    if (!Luau::isValidAlias(potentialAlias))
    {
        // TODO: report error: "@" + potentialAlias + " is not a valid alias");
        return std::nullopt;
    }

    // Luau aliases are case insensitive
    std::transform(potentialAlias.begin(), potentialAlias.end(), potentialAlias.begin(),
        [](unsigned char c)
        {
            return ('A' <= c && c <= 'Z') ? (c + ('a' - 'A')) : c;
        });

    Uri resolvedUri;
    if (auto aliasInfo = config.aliases.find(potentialAlias))
    {
        if (Luau::startsWith(aliasInfo->value, "@"))
        {
            if (!visited.insert(potentialAlias).second)
                return std::nullopt;

            auto chainedResolved = resolveAliasWithCycleCheck(aliasInfo->value, config, from, visited);
            if (!chainedResolved)
                return std::nullopt;
            resolvedUri = *chainedResolved;
        }
        else
        {
            resolvedUri = resolveAliasLocation(*aliasInfo);
        }
    }
    else if (potentialAlias == "self")
        resolvedUri = from;
    else
        // TODO: report error: "@" + potentialAlias + " is not a valid alias"
        return std::nullopt;

    auto remainder = path.substr(potentialAlias.size() + 1);

    // If remainder begins with a '/' character, we need to trim it off before it gets mistaken for an
    // absolute path
    remainder.erase(0, remainder.find_first_not_of("/\\"));

    if (remainder.empty())
        return resolvedUri;
    else
        return resolvedUri.resolvePath(remainder);
}

std::optional<Uri> resolveAlias(const std::string& path, const Luau::Config& config, const Uri& from)
{
    std::unordered_set<std::string> visited;
    return resolveAliasWithCycleCheck(path, config, from, visited);
}

std::optional<Luau::ModuleInfo> LSPPlatform::resolveStringRequire(
    const Luau::ModuleInfo* context, const std::string& requiredString, const Luau::TypeCheckLimits& limits)
{
    if (!context)
        return std::nullopt;

    auto contextPath = resolveToRealPath(context->name);
    if (!contextPath)
        return std::nullopt;

    auto baseUri = contextPath->parent();
    if (!baseUri)
        return std::nullopt;

    ClientConfiguration clientConfig;
    if (fileResolver->client)
        clientConfig = fileResolver->client->getConfiguration(fileResolver->rootUri);

    if (isInitLuauFile(*contextPath) && !clientConfig.require.useOriginalRequireByStringSemantics)
    {
        baseUri = baseUri->parent();
        if (!baseUri)
            return std::nullopt;
    }

    auto fileUri = baseUri->resolvePath(requiredString);

    auto luauConfig = fileResolver->getConfig(context->name, limits);
    if (auto aliasedPath = resolveAlias(requiredString, luauConfig, *contextPath->parent()))
    {
        fileUri = aliasedPath.value();
    }

    // Handle "init.luau" files in a directory
    if (fileUri.isDirectory())
    {
        if (auto redirect = resolveDirectoryRedirect(fileUri))
            return Luau::ModuleInfo{fileResolver->getModuleName(*redirect)};
        fileUri = fileUri.resolvePath("init");
    }

    // Add file endings
    if (fileUri.extension() != ".luau" && fileUri.extension() != ".lua")
    {
        auto fileUriWithExtension = fileUri;
        fileUriWithExtension.path = fileUri.path + ".luau";
        if (!fileUriWithExtension.exists())
            // fall back to .lua if a module with .luau doesn't exist
            fileUri.path += ".lua";
        else
            fileUri.path = fileUriWithExtension.path;
    }

    return Luau::ModuleInfo{fileResolver->getModuleName(fileUri)};
}

std::optional<Luau::ModuleInfo> LSPPlatform::resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node, const Luau::TypeCheckLimits& limits)
{
    // Handle require("path") for compatibility
    if (auto* expr = node->as<Luau::AstExprConstantString>())
    {
        std::string requiredString(expr->value.data, expr->value.size);
        return resolveStringRequire(context, requiredString, limits);
    }

    return std::nullopt;
}

namespace
{
// ScriptReference resolves child mappings from the nearest project file, independently of
// the Git root used by the project global.
std::optional<json> readNavigationProject(const Uri& directory)
{
    auto source = Luau::FileUtils::readFile(directory.resolvePath("default.project.json").fsPath());
    if (!source)
        return std::nullopt;
    auto project = json::parse(*source, nullptr, false);
    if (project.is_discarded() || !project.contains("tree") || !project["tree"].is_object())
        return std::nullopt;
    return project["tree"];
}

std::optional<Uri> navigationProjectChild(const Uri& base, const std::string& name)
{
    for (std::optional<Uri> directory = base; directory; directory = directory->parent())
    {
        if (!directory->resolvePath("default.project.json").exists())
            continue;
        auto tree = readNavigationProject(*directory);
        if (!tree)
            return std::nullopt;
        std::function<std::optional<Uri>(const json&, const Uri&)> findChild = [&](const json& node, const Uri& physicalPath) -> std::optional<Uri>
        {
            if (physicalPath == base)
            {
                if (node.contains(name) && node[name].is_object())
                {
                    const auto& child = node[name];
                    if (child.contains("$path") && child["$path"].is_string())
                        return directory->resolvePath(child["$path"].get<std::string>());
                }
                return std::nullopt;
            }
            for (const auto& [childName, child] : node.items())
            {
                if (childName.empty() || childName[0] == '$' || !child.is_object())
                    continue;
                auto childPath = child.contains("$path") && child["$path"].is_string() ? directory->resolvePath(child["$path"].get<std::string>())
                                                                                       : physicalPath.resolvePath(childName);
                if (auto result = findChild(child, childPath))
                    return result;
            }
            return std::nullopt;
        };
        if (auto result = findChild(*tree, *directory))
            return result;
        if (tree->contains("$path") && (*tree)["$path"].is_string())
            return findChild(*tree, directory->resolvePath((*tree)["$path"].get<std::string>()));
        return std::nullopt;
    }
    return std::nullopt;
}

// Lune rejects ambiguous file/directory entry points instead of choosing .luau first.
// Project redirects take precedence over init files and are followed only once, matching
// LuauFilePath::resolve_inner in the runtime.
std::optional<Uri> luneNavigationSource(const WorkspaceFileResolver& fileResolver, const Uri& path, bool checkProject = true)
{
    if ((path.extension() == ".luau" || path.extension() == ".lua") && (fileResolver.getTextDocument(path) || (path.exists() && !path.isDirectory())))
        return path;
    if (path.isDirectory() && checkProject)
    {
        if (auto tree = readNavigationProject(path); tree && tree->contains("$path") && (*tree)["$path"].is_string())
            return luneNavigationSource(fileResolver, path.resolvePath((*tree)["$path"].get<std::string>()), false);
    }
    std::optional<Uri> found;
    for (const auto& suffix : {".luau", ".lua", "/init.luau", "/init.lua"})
    {
        auto candidate = path;
        candidate.path += suffix;
        if (fileResolver.getTextDocument(candidate) || (candidate.exists() && !candidate.isDirectory()))
        {
            if (found)
                return std::nullopt;
            found = candidate;
        }
    }
    return found;
}
} // namespace

Uri toWorkspacePath(const Uri& workspaceRoot, const Uri& resolvedPath)
{
    std::error_code error;
    auto canonical = std::filesystem::weakly_canonical(workspaceRoot.fsPath(), error);
    if (error)
        return resolvedPath;
    auto canonicalRoot = Uri::file(canonical.generic_string());
    if (resolvedPath == canonicalRoot)
        return workspaceRoot;
    if (!canonicalRoot.isAncestorOf(resolvedPath))
        return resolvedPath;
    return workspaceRoot.resolvePath(resolvedPath.lexicallyRelative(canonicalRoot));
}

std::optional<Luau::ModuleInfo> LSPPlatform::resolveNavigationModule(
    const Luau::ModuleInfo& context, Luau::AstExpr* expression, const Luau::TypeCheckLimits& limits)
{
    if (!fileResolver->client || fileResolver->client->getConfiguration(fileResolver->rootUri).platform.type != LSPPlatformConfig::Lune)
        return std::nullopt;
    auto path = resolveToRealPath(context.name);
    if (!path)
        return std::nullopt;
    if (auto global = expression->as<Luau::AstExprGlobal>())
    {
        if (global->name == "script")
            return context;
        if (global->name == "project")
        {
            if (auto directory = path->parent())
            {
                std::error_code error;
                auto canonical = std::filesystem::weakly_canonical(directory->fsPath(), error);
                if (!error)
                    if (auto root = findGitRoot(Uri::file(canonical.generic_string())))
                        return Luau::ModuleInfo{fileResolver->getModuleName(toWorkspacePath(fileResolver->rootUri, *root))};
            }
        }
        return std::nullopt;
    }

    std::string name;
    if (auto index = expression->as<Luau::AstExprIndexName>())
        name = index->index.value;
    else if (auto index = expression->as<Luau::AstExprIndexExpr>())
    {
        auto string = index->index->as<Luau::AstExprConstantString>();
        if (!string)
            return std::nullopt;
        name.assign(string->value.data, string->value.size);
    }
    else
        // ScriptReference has no child-lookup methods.
        return std::nullopt;

    if (name == "Parent")
    {
        if (!path->exists() && !fileResolver->getTextDocument(*path) && !luneNavigationSource(*fileResolver, *path))
            return std::nullopt;
        if (auto parent = path->parent())
            return Luau::ModuleInfo{fileResolver->getModuleName(*parent)};
        return std::nullopt;
    }
    if (name == "Name" || name == "GetFullName")
        return std::nullopt;
    auto base = (path->exists() && !path->isDirectory()) || fileResolver->getTextDocument(*path) ? path->parent() : path;
    if (!base)
        return std::nullopt;
    auto child = navigationProjectChild(*base, name).value_or(base->resolvePath(name));
    return Luau::ModuleInfo{fileResolver->getModuleName(child)};
}

std::optional<Uri> LSPPlatform::navigationSource(const Luau::ModuleInfo& target, const Luau::TypeCheckLimits& limits)
{
    auto path = resolveToRealPath(target.name);
    return path ? luneNavigationSource(*fileResolver, *path) : std::nullopt;
}

std::optional<Uri> LSPPlatform::navigationDirectory(const Luau::ModuleInfo& target)
{
    auto path = resolveToRealPath(target.name);
    if (!path || !path->isDirectory())
        return std::nullopt;
    // Like Lune's GetFullName, only ordinary directories count. Any entry-point candidate
    // makes the path a module, even when it's ambiguous or broken.
    auto sibling = *path;
    for (const auto& suffix : {".luau", ".lua"})
    {
        sibling.path = path->path + suffix;
        if (sibling.exists())
            return std::nullopt;
    }
    for (const auto& entry : {"init.luau", "init.lua", "default.project.json"})
        if (path->resolvePath(entry).exists())
            return std::nullopt;
    return path;
}

std::optional<Luau::ModuleInfo> LSPPlatform::resolveExpressionToModule(
    const Luau::ModuleName& from, Luau::AstExpr* expression, const Luau::TypeCheckLimits& limits)
{
    // The frontend module resolver only traces require arguments. Walk explicit receivers
    // here instead, retaining the originating module until a real global root is found.
    std::function<std::optional<Luau::ModuleInfo>(Luau::AstExpr*)> resolve = [&](Luau::AstExpr* expr) -> std::optional<Luau::ModuleInfo>
    {
        if (auto group = expr->as<Luau::AstExprGroup>())
            return resolve(group->expr);
        if (auto global = expr->as<Luau::AstExprGlobal>())
        {
            if (global->name != "script" && global->name != "project" && global->name != "game")
                return std::nullopt;
            return resolveNavigationModule(Luau::ModuleInfo{from}, expr, limits);
        }
        Luau::AstExpr* receiver = nullptr;
        if (auto index = expr->as<Luau::AstExprIndexName>(); index && index->op == '.')
            receiver = index->expr;
        else if (auto index = expr->as<Luau::AstExprIndexExpr>(); index && index->index->is<Luau::AstExprConstantString>())
            receiver = index->expr;
        else if (auto call = expr->as<Luau::AstExprCall>();
            call && call->self && call->args.size >= 1 && call->args.size <= 2 && call->args.data[0]->is<Luau::AstExprConstantString>())
        {
            auto method = call->func->as<Luau::AstExprIndexName>();
            if (!method)
                return std::nullopt;
            if (method->index != "WaitForChild" && !(call->args.size == 1 && (method->index == "GetService" || method->index == "FindFirstChild" ||
                                                                                 method->index == "FindFirstAncestor")))
                return std::nullopt;
            receiver = method->expr;
        }
        if (!receiver)
            return std::nullopt;
        if (auto context = resolve(receiver))
            return resolveNavigationModule(*context, expr, limits);
        return std::nullopt;
    };
    return resolve(expression);
}

std::optional<Uri> LSPPlatform::resolveExpressionToSource(
    const Luau::ModuleName& from, Luau::AstExpr* expression, const Luau::TypeCheckLimits& limits)
{
    if (auto target = resolveExpressionToModule(from, expression, limits))
        if (auto source = navigationSource(*target, limits))
            return toWorkspacePath(fileResolver->rootUri, *source);
    return std::nullopt;
}

std::optional<Uri> LSPPlatform::resolveExpressionToDirectory(
    const Luau::ModuleName& from, Luau::AstExpr* expression, const Luau::TypeCheckLimits& limits)
{
    auto target = resolveExpressionToModule(from, expression, limits);
    if (!target || navigationSource(*target, limits))
        return std::nullopt;
    // The project root can resolve outside the workspace's spelling when the workspace is
    // nested inside the repository, so final targets are mapped as well.
    if (auto directory = navigationDirectory(*target))
        return toWorkspacePath(fileResolver->rootUri, *directory);
    return std::nullopt;
}

std::optional<Luau::AutocompleteEntryMap> LSPPlatform::completionCallback(
    const std::string& tag, std::optional<const Luau::ExternType*> ctx, std::optional<std::string> contents, const Luau::ModuleName& moduleName)
{
    return std::nullopt;
}

Luau::LanguageServer::AutoImports::ModuleVisitor LSPPlatform::getAutoImportsModuleVisitor(const Luau::ModuleName& /*from*/)
{
    return Luau::LanguageServer::AutoImports::defaultModuleVisitor(workspaceFolder->frontend);
}

void LSPPlatform::handleSuggestImports(const TextDocument& textDocument, const Luau::SourceModule& module, const ClientConfiguration& config,
    size_t hotCommentsLineNumber, bool completingTypeReferencePrefix, std::vector<lsp::CompletionItem>& items)
{
    if (!config.completion.imports.suggestRequires)
        return;

    LUAU_ASSERT(module.root);
    Luau::LanguageServer::AutoImports::FindImportsVisitor importsVisitor;
    importsVisitor.visit(module.root);

    Luau::LanguageServer::AutoImports::StringRequireAutoImporterContext ctx{
        module.name,
        Luau::NotNull(&textDocument),
        getAutoImportsModuleVisitor(module.name),
        Luau::NotNull(workspaceFolder),
        Luau::NotNull(&config.completion.imports),
        hotCommentsLineNumber,
        Luau::NotNull(&importsVisitor),
        getAutoImportsRequirePathComputer(module.name, config.completion.imports.requireStyle),
    };

    return Luau::LanguageServer::AutoImports::suggestStringRequires(ctx, items);
}

void LSPPlatform::handleUnknownSymbolFix(const UnknownSymbolFixContext& ctx, const Luau::UnknownSymbol& unknownSymbol,
    const std::optional<lsp::Diagnostic>& diagnostic, std::vector<lsp::CodeAction>& result)
{
    if (unknownSymbol.context != Luau::UnknownSymbol::Binding)
        return;

    LUAU_ASSERT(ctx.sourceModule->root);
    Luau::LanguageServer::AutoImports::FindImportsVisitor importsVisitor;
    importsVisitor.visit(ctx.sourceModule->root);

    ClientConfiguration config = workspaceFolder->fileResolver.client->getConfiguration(workspaceFolder->rootUri);
    auto hotCommentsLineNumber = Luau::LanguageServer::AutoImports::computeHotCommentsLineNumber(*ctx.sourceModule);

    Luau::LanguageServer::AutoImports::StringRequireAutoImporterContext importCtx{
        ctx.sourceModule->name,
        Luau::NotNull(ctx.textDocument),
        getAutoImportsModuleVisitor(ctx.sourceModule->name),
        ctx.workspaceFolder,
        Luau::NotNull(&config.completion.imports),
        hotCommentsLineNumber,
        Luau::NotNull(&importsVisitor),
        getAutoImportsRequirePathComputer(ctx.sourceModule->name, config.completion.imports.requireStyle),
        [&unknownSymbol](const std::string& requireName)
        {
            return requireName == unknownSymbol.name;
        },
    };

    const auto results = Luau::LanguageServer::AutoImports::computeAllStringRequires(importCtx);
    for (const auto& stringRequire : results)
    {
        lsp::CodeAction action;
        action.title = "Add require for '" + stringRequire.variableName + "' from \"" + stringRequire.requirePath + "\"";
        action.kind = lsp::CodeActionKind::QuickFix;
        action.isPreferred = results.size() == 1;

        if (diagnostic)
            action.diagnostics.push_back(*diagnostic);

        lsp::WorkspaceEdit workspaceEdit;
        workspaceEdit.changes.emplace(ctx.uri, std::vector{stringRequire.edit});
        action.edit = workspaceEdit;

        result.push_back(action);
    }
}

std::vector<lsp::TextEdit> LSPPlatform::computeAddAllMissingImportsEdits(
    const UnknownSymbolFixContext& ctx, const std::vector<Luau::TypeError>& errors)
{
    std::vector<lsp::TextEdit> edits;

    Luau::LanguageServer::AutoImports::FindImportsVisitor importsVisitor;
    importsVisitor.visit(ctx.sourceModule->root);

    ClientConfiguration config = workspaceFolder->fileResolver.client->getConfiguration(workspaceFolder->rootUri);
    auto hotCommentsLineNumber = Luau::LanguageServer::AutoImports::computeHotCommentsLineNumber(*ctx.sourceModule);

    std::vector<std::string> unknownSymbols;
    std::unordered_set<std::string> addedRequires;

    for (const auto& error : errors)
    {
        const auto* unknownSymbol = Luau::get_if<Luau::UnknownSymbol>(&error.data);
        if (!unknownSymbol || unknownSymbol->context != Luau::UnknownSymbol::Binding)
            continue;

        unknownSymbols.emplace_back(unknownSymbol->name);
    }

    Luau::LanguageServer::AutoImports::StringRequireAutoImporterContext importCtx{
        ctx.sourceModule->name,
        Luau::NotNull(ctx.textDocument),
        getAutoImportsModuleVisitor(ctx.sourceModule->name),
        ctx.workspaceFolder,
        Luau::NotNull(&config.completion.imports),
        hotCommentsLineNumber,
        Luau::NotNull(&importsVisitor),
        getAutoImportsRequirePathComputer(ctx.sourceModule->name, config.completion.imports.requireStyle),
        [&unknownSymbols](const std::string& requireName)
        {
            return contains(unknownSymbols, requireName);
        },
    };

    const auto results = computeAllStringRequires(importCtx);
    for (const auto& stringRequire : results)
    {
        if (addedRequires.find(stringRequire.variableName) != addedRequires.end())
            continue;

        edits.push_back(stringRequire.edit);
        addedRequires.insert(stringRequire.variableName);
    }

    std::sort(edits.begin(), edits.end(),
        [](const lsp::TextEdit& a, const lsp::TextEdit& b)
        {
            return a.range.start.line == b.range.start.line ? a.newText < b.newText : a.range.start.line < b.range.start.line;
        });

    return edits;
}
