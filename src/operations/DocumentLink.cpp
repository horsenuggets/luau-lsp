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

// Collects each child segment of an instance reference, matching the segments that Go to
// Definition navigates from. Bare roots like "script" and "project" are skipped, since they
// appear so often that underlining every use would be noise.
struct FindNavigationVisitor : public Luau::AstVisitor
{
    std::vector<std::pair<Luau::AstExpr*, Luau::Location>> segments{};

    bool visit(Luau::AstExprIndexName* index) override
    {
        if (index->op == '.')
            segments.emplace_back(index, index->indexLocation);
        return true;
    }

    bool visit(Luau::AstExprIndexExpr* index) override
    {
        if (index->index->is<Luau::AstExprConstantString>())
            segments.emplace_back(index, index->index->location);
        return true;
    }

    bool visit(Luau::AstExprCall* call) override
    {
        if (call->self && call->args.size >= 1 && call->args.data[0]->is<Luau::AstExprConstantString>())
            segments.emplace_back(call, call->args.data[0]->location);
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

    for (auto& require : visitor.requireInfos)
    {
        // Instance paths like require(script.Child) get a link per segment below, which
        // would overlap a link over the whole argument.
        if (platform->resolveExpressionToSource(moduleName, require.require, limits))
            continue;
        if (auto moduleInfo = frontend.moduleResolver.resolveModuleInfo(moduleName, *require.require))
        {
            // Resolve the module info to a URI
            if (auto uri = platform->resolveToRealPath(moduleInfo->name))
            {
                lsp::DocumentLink link;
                link.target = *uri;
                Luau::Position endPos{require.location.end.line, require.location.end.column - 1};
                link.range = {textDocument->convertPosition(require.location.begin), textDocument->convertPosition(endPos)};
                result.push_back(link);
            }
        }
    }

    // Every navigable segment is linked, so all of them are underlined consistently. Files
    // open directly, while folders are revealed in the explorer.
    FindNavigationVisitor navigationVisitor;
    sourceModule->root->visit(&navigationVisitor);
    for (const auto& [expression, location] : navigationVisitor.segments)
    {
        lsp::DocumentLink link;
        link.range = {textDocument->convertPosition(location.begin), textDocument->convertPosition(location.end)};
        if (auto source = platform->resolveExpressionToSource(moduleName, expression, limits))
            link.target = *source;
        else if (auto directory = platform->resolveExpressionToDirectory(moduleName, expression, limits))
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
