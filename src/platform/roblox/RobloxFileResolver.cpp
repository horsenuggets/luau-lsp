#include "Platform/RobloxPlatform.hpp"
#include "LSP/ClientConfiguration.hpp"
#include "LSP/JsonTomlSyntaxParser.hpp"
#include "LSP/Workspace.hpp"

#include "Luau/TimeTrace.h"
#include "LuauFileUtils.hpp"

std::optional<Luau::ModuleName> RobloxPlatform::resolveToVirtualPath(const Uri& uri) const
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::resolveToVirtualPath", "LSP");
    if (const auto sourceNode = getSourceNodeFromRealPath(uri))
        return getVirtualPathFromSourceNode(*sourceNode);
    return std::nullopt;
}

std::optional<Uri> RobloxPlatform::resolveToRealPath(const Luau::ModuleName& name) const
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::resolveToRealPath", "LSP");
    if (isVirtualPath(name))
    {
        if (auto sourceNode = getSourceNodeFromVirtualPath(name))
        {
            return getRealPathFromSourceNode(*sourceNode);
        }
    }
    else
    {
        return fileResolver->getUri(name);
    }

    return std::nullopt;
}

Luau::SourceCode::Type RobloxPlatform::sourceCodeTypeFromPath(const Uri& path) const
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::sourceCodeTypeFromPath", "LSP");
    if (auto sourceNode = getSourceNodeFromRealPath(path))
        return (*sourceNode)->sourceCodeType();

    auto filename = path.filename();

    if (endsWith(filename, ".server.lua") || endsWith(filename, ".server.luau") || endsWith(filename, ".client.lua") ||
        endsWith(filename, ".client.luau"))
    {
        return Luau::SourceCode::Type::Script;
    }

    return Luau::SourceCode::Type::Module;
}

std::optional<std::string> RobloxPlatform::readSourceCode(const Luau::ModuleName& name, const Uri& path) const
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::readSourceCode", "LSP");
    if (auto parentResult = LSPPlatform::readSourceCode(name, path))
        return parentResult;

    auto source = Luau::FileUtils::readFile(path.fsPath());
    if (!source)
        return std::nullopt;

    if (path.extension() == ".json")
    {
        try
        {
            source = "--!strict\nreturn " + jsonValueToLuau(json::parse(*source));
        }
        catch (const std::exception& e)
        {
            // TODO: display diagnostic?
            std::cerr << "Failed to load JSON module: " << path.toString() << " - " << e.what() << '\n';
            return std::nullopt;
        }
    }
    else if (path.extension() == ".toml")
    {
        try
        {
            std::string tomlSource(*source);
            std::istringstream tomlSourceStream(tomlSource, std::ios_base::binary | std::ios_base::in);
            source = "--!strict\nreturn " + tomlValueToLuau(toml::parse(tomlSourceStream, path.fsPath()));
        }
        catch (const std::exception& e)
        {
            // TODO: display diagnostic?
            std::cerr << "Failed to load TOML module: " << path.toString() << " - " << e.what() << '\n';
            return std::nullopt;
        }
    }
    else if (path.extension() == ".yaml" || path.extension() == ".yml")
    {
        try
        {
            ryml::Tree tree = ryml::parse_in_arena(ryml::to_csubstr(*source));
            source = "--!strict\nreturn " + yamlValueToLuau(tree.rootref());
        }
        catch (const std::exception& e)
        {
            // TODO: display diagnostic?
            std::cerr << "Failed to load YAML module: " << path.toString() << " - " << e.what() << '\n';
            return std::nullopt;
        }
    }

    return source;
}

/// Modify the context so that game/Players/LocalPlayer items point to the correct place
static std::string mapContext(const std::string& context)
{
    if (context == "game/Players/LocalPlayer/PlayerScripts")
        return "game/StarterPlayer/StarterPlayerScripts";
    else if (context == "game/Players/LocalPlayer/PlayerGui")
        return "game/StarterGui";
    else if (context == "game/Players/LocalPlayer/StarterGear")
        return "game/StarterPack";
    return context;
}

std::optional<Luau::ModuleInfo> RobloxPlatform::resolveStringRequire(
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
        auto initLuau = fileUri.resolvePath("init.luau");
        auto initLua = fileUri.resolvePath("init.lua");

        if (initLuau.exists() || initLua.exists())
        {
            fileUri = fileUri.resolvePath("init");
        }
        else if (auto it = directoryToSourceNodes.find(fileUri); it != directoryToSourceNodes.end())
        {
            // Use the sourcemap to resolve through project.json $path redirects
            if (auto scriptPath = it->second->getScriptFilePath())
            {
                fileUri = fileResolver->rootUri.resolvePath(*scriptPath);
                return Luau::ModuleInfo{fileResolver->getModuleName(fileUri)};
            }
            else
            {
                fileUri = fileUri.resolvePath("init");
            }
        }
        else
        {
            fileUri = fileUri.resolvePath("init");
        }
    }

    // Add file endings
    if (fileUri.extension() != ".luau" && fileUri.extension() != ".lua")
    {
        auto fileUriWithExtension = fileUri;
        fileUriWithExtension.path = fileUri.path + ".luau";
        if (!fileUriWithExtension.exists())
            fileUri.path += ".lua";
        else
            fileUri.path = fileUriWithExtension.path;
    }

    return Luau::ModuleInfo{fileResolver->getModuleName(fileUri)};
}

std::optional<Luau::ModuleInfo> RobloxPlatform::resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node, const Luau::TypeCheckLimits& limits)
{

    if (auto parentResult = LSPPlatform::resolveModule(context, node, limits))
        return parentResult;

    if (auto* g = node->as<Luau::AstExprGlobal>())
    {
        if (g->name == "game")
            return Luau::ModuleInfo{"game"};

        if (g->name == "script")
        {
            if (isVirtualPath(context->name))
            {
                return Luau::ModuleInfo{context->name};
            }
            else if (auto virtualPath = resolveToVirtualPath(fileResolver->getUri(context->name)))
            {
                return Luau::ModuleInfo{virtualPath.value()};
            }
        }
    }
    else if (auto* i = node->as<Luau::AstExprIndexName>())
    {
        if (context)
        {
            if (strcmp(i->index.value, "Parent") == 0)
            {
                // Pop the name instead
                auto parentPath = getParentPath(context->name);
                if (parentPath.has_value())
                    return Luau::ModuleInfo{parentPath.value(), context->optional};
            }

            return Luau::ModuleInfo{mapContext(context->name) + '/' + i->index.value, context->optional};
        }
    }
    else if (auto* i_expr = node->as<Luau::AstExprIndexExpr>())
    {
        if (auto* index = i_expr->index->as<Luau::AstExprConstantString>())
        {
            if (context)
                return Luau::ModuleInfo{mapContext(context->name) + '/' + std::string(index->value.data, index->value.size), context->optional};
        }
    }
    else if (auto* call = node->as<Luau::AstExprCall>(); call && call->self && call->args.size >= 1 && context)
    {
        if (auto* index = call->args.data[0]->as<Luau::AstExprConstantString>())
        {
            Luau::AstName func = call->func->as<Luau::AstExprIndexName>()->index;

            if (func == "GetService" && context->name == "game")
            {
                return Luau::ModuleInfo{"game/" + std::string(index->value.data, index->value.size)};
            }
            else if (func == "WaitForChild" || (func == "FindFirstChild" && call->args.size == 1)) // Don't allow recursive FFC
            {
                return Luau::ModuleInfo{mapContext(context->name) + '/' + std::string(index->value.data, index->value.size), context->optional};
            }
            else if (func == "FindFirstAncestor")
            {
                auto ancestorName = getAncestorPath(context->name, std::string(index->value.data, index->value.size), rootSourceNode);
                if (ancestorName)
                    return Luau::ModuleInfo{*ancestorName, context->optional};
            }
        }
    }

    return std::nullopt;
}
