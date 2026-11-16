#include "LSP/Client.hpp"
#include "LSP/LanguageServer.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/Workspace.hpp"

struct RequireInfo
{
    Luau::AstExpr* require = nullptr;
    Luau::Location location;
};

struct FindRequireVisitor : public Luau::AstVisitor
{
    std::vector<RequireInfo> requireInfos{};

    bool visit(Luau::AstExprCall* call) override
    {
        if (auto maybeRequire = types::matchRequire(*call))
            requireInfos.emplace_back(RequireInfo{*maybeRequire, call->argLocation});
        return true;
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            stat->visit(this);
        }

        return false;
    }
};

// Returns where to link a child segment of an instance reference, matching the segments
// that Go to Definition navigates from. Bare roots like "script" and "project" aren't
// segments, since they appear so often that underlining every use would be noise.
static std::optional<Luau::Location> segmentLocation(Luau::AstExpr* expression)
{
    if (auto index = expression->as<Luau::AstExprIndexName>(); index && index->op == '.')
        return index->indexLocation;
    if (auto index = expression->as<Luau::AstExprIndexExpr>(); index && index->index->is<Luau::AstExprConstantString>())
        return index->index->location;
    if (auto call = expression->as<Luau::AstExprCall>();
        call && call->self && call->args.size >= 1 && call->args.data[0]->is<Luau::AstExprConstantString>())
        return call->args.data[0]->location;
    return std::nullopt;
}

struct FindNavigationVisitor : public Luau::AstVisitor
{
    std::vector<std::pair<Luau::AstExpr*, Luau::Location>> segments{};

    bool visit(Luau::AstExpr* expression) override
    {
        if (auto location = segmentLocation(expression))
            segments.emplace_back(expression, *location);
        return true;
    }
};

// VS Code runs command URIs from document links. The argument must be a marshalled URI
// object ("$mid" 1), since revealInExplorer falls back to the active editor otherwise.
static Uri revealInExplorerUri(const Uri& directory)
{
    json argument = {{"$mid", 1}, {"scheme", directory.scheme}, {"authority", directory.authority}, {"path", directory.path}};
    return Uri{"command", "", "revealInExplorer", json::array({argument}).dump()};
}

std::optional<Uri> WorkspaceFolder::resolveRequireLink(const Luau::ModuleName& moduleName, Luau::AstExpr* argument)
{
    if (auto moduleInfo = frontend.moduleResolver.resolveModuleInfo(moduleName, *argument))
        if (auto uri = platform->resolveToRealPath(moduleInfo->name))
            return uri;
    // Standalone references like project.Source.Foo resolve through the platform when the
    // module resolver doesn't know them.
    return platform->resolveExpressionToSource(moduleName, argument, limits);
}

std::vector<lsp::DocumentLink> WorkspaceFolder::documentLink(const lsp::DocumentLinkParams& params)
{
    if (!client->getConfiguration(rootUri).documentLinks.enabled)
        return {};

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    std::vector<lsp::DocumentLink> result{};

    frontend.parse(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule || !sourceModule->root)
        return {};

    FindRequireVisitor visitor;
    visitor.visit(sourceModule->root);

    // A require links its whole argument to the required module, so the segments inside it
    // aren't linked separately, which would overlap that link.
    std::vector<Luau::Location> linkedRequires;
    for (auto& require : visitor.requireInfos)
    {
        auto uri = resolveRequireLink(moduleName, require.require);
        if (!uri)
            continue;
        lsp::DocumentLink link;
        link.target = *uri;
        Luau::Position endPos{require.location.end.line, require.location.end.column - 1};
        link.range = {textDocument->convertPosition(require.location.begin), textDocument->convertPosition(endPos)};
        result.push_back(link);
        linkedRequires.push_back(require.location);
    }

    // Every other navigable segment is linked, so all of them are underlined consistently.
    // Files open directly, while folders are revealed in the explorer for clients that support it.
    FindNavigationVisitor navigationVisitor;
    sourceModule->root->visit(&navigationVisitor);
    for (const auto& [expression, location] : navigationVisitor.segments)
    {
        if (std::any_of(linkedRequires.begin(), linkedRequires.end(),
                [&location = location](const auto& require)
                {
                    return require.encloses(location);
                }))
            continue;
        lsp::DocumentLink link;
        link.range = {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)};
        if (auto source = platform->resolveExpressionToSource(moduleName, expression, limits))
            link.target = *source;
        else if (auto directory = client->folderNavigation ? platform->resolveExpressionToDirectory(moduleName, expression, limits) : std::nullopt)
        {
            link.target = revealInExplorerUri(*directory);
            link.tooltip = "Reveal in Explorer";
        }
        else
            continue;
        result.push_back(link);
    }

    return result;
}

std::vector<lsp::DocumentLink> LanguageServer::documentLink(const lsp::DocumentLinkParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->documentLink(params);
}
