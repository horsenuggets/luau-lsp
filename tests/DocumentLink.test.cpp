#include "Fixture.h"
#include "LSP/LanguageServer.hpp"
#include "Luau/StringUtils.h"
#include "Platform/LSPPlatform.hpp"
#include "doctest.h"

#include <map>

TEST_CASE("document_links_configuration_defaults_and_round_trips")
{
    CHECK(ClientConfiguration{}.documentLinks.enabled);
    CHECK(json::object().get<ClientConfiguration>().documentLinks.enabled);
    auto emptyLinks = json{{"documentLinks", json::object()}};
    CHECK(emptyLinks.get<ClientConfiguration>().documentLinks.enabled);

    for (bool enabled : {false, true})
    {
        ClientConfiguration configuration;
        configuration.documentLinks.enabled = enabled;
        CHECK_EQ(json(configuration).get<ClientConfiguration>().documentLinks.enabled, enabled);
    }
}

TEST_CASE_FIXTURE(Fixture, "document_links_toggle_preserves_definition_navigation")
{
    switchToStandardPlatform();
    auto target = tempDir.write_child("Target.luau", "return {}\n");
    auto [source, position] = sourceWithMarker("return require(\"./Tar|get\")");
    auto document = newDocument("Main.luau", source);
    lsp::DefinitionParams definitionParams;
    definitionParams.textDocument = lsp::TextDocumentIdentifier{document};
    definitionParams.position = position;
    lsp::DocumentLinkParams linkParams;
    linkParams.textDocument = lsp::TextDocumentIdentifier{document};

    client->configStore.emplace(workspace.rootUri, client->globalConfig);
    for (bool enabled : {true, false, true})
    {
        client->configStore.at(workspace.rootUri).documentLinks.enabled = enabled;
        auto definitions = workspace.gotoDefinition(definitionParams, nullptr);
        REQUIRE_EQ(definitions.size(), 1);
        CHECK_EQ(definitions[0].uri, Uri::file(target));
        auto links = workspace.documentLink(linkParams);
        REQUIRE_EQ(links.size(), enabled ? 1 : 0);
        if (enabled)
            CHECK_EQ(links[0].target, Uri::file(target));
    }
}

namespace
{
struct NavigationLinkFixture : Fixture
{
    // Behaves like the VS Code extension, which opts in to folder navigation.
    NavigationLinkFixture()
    {
        client->folderNavigation = true;
    }

    void open(const std::string& source)
    {
        auto document = workspace.rootUri.resolvePath("main.luau");
        if (workspace.fileResolver.getTextDocument(document))
            updateDocument(document, source);
        else
            newDocument("main.luau", source);
    }

    // Describes each link as "text -> target", sorted by the linked text. Folder targets end
    // in a slash, and file targets are the opened file.
    std::string links(const std::string& source)
    {
        open(source);
        lsp::DocumentLinkParams params;
        params.textDocument = lsp::TextDocumentIdentifier{workspace.rootUri.resolvePath("main.luau")};
        std::map<std::string, std::string> described;
        for (const auto& link : workspace.documentLink(params))
        {
            REQUIRE_EQ(link.range.start.line, link.range.end.line);
            auto line = Luau::split(source, '\n')[link.range.start.line];
            auto text = std::string(line.substr(link.range.start.character, link.range.end.character - link.range.start.character));
            if (link.target.scheme != "command")
            {
                CHECK_FALSE(link.tooltip);
                described.emplace(text, link.target.lexicallyRelative(workspace.rootUri));
                continue;
            }
            CHECK_EQ(link.target.path, "revealInExplorer");
            CHECK_EQ(link.tooltip, "Reveal in Explorer");
            // The client receives an encoded string, so check that it decodes to the same command.
            auto wire = json(link).at("target").get<std::string>();
            CHECK(Luau::startsWith(wire, "command:revealInExplorer?"));
            CHECK_EQ(Uri::parse(wire), link.target);
            auto arguments = json::parse(link.target.query);
            REQUIRE(arguments.is_array());
            REQUIRE_EQ(arguments.size(), 1);
            const auto& argument = arguments[0];
            CHECK_EQ(argument.at("$mid"), 1);
            Uri directory{argument.at("scheme").get<std::string>(), argument.at("authority").get<std::string>(),
                argument.at("path").get<std::string>()};
            CHECK(directory.isDirectory());
            described.emplace(text, directory.lexicallyRelative(workspace.rootUri) + "/");
        }
        std::string result;
        for (const auto& [text, target] : described)
            result += (result.empty() ? "" : ", ") + text + " -> " + target;
        return result;
    }

    // Describes the definition at the "|" marker in the same way as links().
    std::string definition(const std::string& markedSource)
    {
        auto [source, position] = sourceWithMarker(markedSource);
        open(source);
        lsp::DefinitionParams params;
        params.textDocument = lsp::TextDocumentIdentifier{workspace.rootUri.resolvePath("main.luau")};
        params.position = position;
        std::string result;
        for (const auto& location : workspace.gotoDefinition(params, nullptr))
            result += (result.empty() ? "" : ", ") + location.uri.lexicallyRelative(workspace.rootUri) + (location.uri.isDirectory() ? "/" : "");
        return result;
    }

    void lune()
    {
        switchToStandardPlatform();
        client->globalConfig.platform.type = LSPPlatformConfig::Lune;
    }
};
} // namespace

TEST_CASE_FIXTURE(NavigationLinkFixture, "navigation_links_cover_lune_files_and_folders")
{
    lune();
    tempDir.touch_child(".git");
    tempDir.write_child("Source/Foo.luau", "return {}\n");
    tempDir.write_child("Source/Nested/Bar.luau", "return {}\n");
    tempDir.write_child("Package/init.luau", "return {}\n");
    tempDir.write_child("Package/Inner.luau", "return {}\n");

    CHECK_EQ(links("local value = project.Source.Foo"), "Foo -> Source/Foo.luau, Source -> Source/");
    CHECK_EQ(links("local value = project.Source.Nested.Bar"), "Bar -> Source/Nested/Bar.luau, Nested -> Source/Nested/, Source -> Source/");
    CHECK_EQ(links("local value = project[\"Source\"]"), "\"Source\" -> Source/");
    CHECK_EQ(links("local value = script.Parent.Source"), "Parent -> ./, Source -> Source/");

    // Folders with an init file open it instead of revealing the folder.
    CHECK_EQ(links("local value = project.Package"), "Package -> Package/init.luau");
    CHECK_EQ(links("local value = project.Package.Inner"), "Inner -> Package/Inner.luau, Package -> Package/init.luau");

    // Missing paths, local shadows, and bare roots get no link.
    CHECK_EQ(links("local value = project.Missing"), "");
    CHECK_EQ(links("local project = {}\nlocal value = project.Source"), "");
    CHECK_EQ(links("local value = project"), "");
}

TEST_CASE_FIXTURE(NavigationLinkFixture, "navigation_links_split_instance_requires_into_segments")
{
    lune();
    tempDir.touch_child(".git");
    tempDir.write_child("Source/Foo.luau", "return {}\n");

    // Instance paths are linked per segment rather than across the whole argument, so the
    // links never overlap.
    CHECK_EQ(links("local Foo = require(project.Source.Foo)"), "Foo -> Source/Foo.luau, Source -> Source/");
    // Other requires keep the single argument link.
    CHECK_EQ(links("local Foo = require(\"./Source/Foo\")"), "\"./Source/Foo\" -> Source/Foo.luau");
}

TEST_CASE_FIXTURE(NavigationLinkFixture, "navigation_links_keep_whole_argument_links_for_bare_roots")
{
    tempDir.touch_child(".git");
    tempDir.write_child("Source/init.luau", "return {}\n");
    loadSourcemap(R"({"name":"Package","className":"ModuleScript","filePaths":["default.project.json","Source/init.luau"],"children":[
        {"name":"Main","className":"ModuleScript","filePaths":["main.luau"]}
    ]})");

    // Bare roots have no segments to link instead, so the require keeps its own link.
    CHECK_EQ(links("local Root = require(project)"), "project -> Source/init.luau");
    CHECK_EQ(links("local Root = require((project))"), "(project) -> Source/init.luau");
}

TEST_CASE_FIXTURE(NavigationLinkFixture, "navigation_links_use_project_directories_of_packages")
{
    tempDir.touch_child(".git");
    tempDir.write_child("Source/Package/src/Foo.luau", "return {}\n");
    tempDir.write_child("Source/Package/default.project.json", R"({"tree":{"$path":"src"}})");
    // The package maps its scripts into "src", so only its project file shows where it lives.
    loadSourcemap(R"({"name":"Root","className":"Folder","filePaths":["default.project.json"],"children":[
        {"name":"Main","className":"ModuleScript","filePaths":["main.luau"]},
        {"name":"Source","className":"Folder","children":[
            {"name":"Package","className":"Folder","filePaths":["Source/Package/default.project.json"],"children":[
                {"name":"Foo","className":"ModuleScript","filePaths":["Source/Package/src/Foo.luau"]}
            ]}
        ]}
    ]})");

    CHECK_EQ(links("local value = project.Source"), "Source -> Source/");
}

TEST_CASE_FIXTURE(NavigationLinkFixture, "navigation_links_reveal_sourcemap_folders")
{
    tempDir.touch_child(".git");
    tempDir.write_child("Source/Foo.luau", "return {}\n");
    tempDir.write_child("Source/Deep/Inner/init.luau", "return {}\n");
    loadSourcemap(R"({"name":"Package","className":"Folder","filePaths":["default.project.json"],"children":[
        {"name":"Main","className":"ModuleScript","filePaths":["main.luau"]},
        {"name":"Source","className":"Folder","children":[
            {"name":"Foo","className":"ModuleScript","filePaths":["Source/Foo.luau"]},
            {"name":"Deep","className":"Folder","children":[
                {"name":"Inner","className":"ModuleScript","filePaths":["Source/Deep/Inner/init.luau"]}
            ]}
        ]},
        {"name":"Empty","className":"Folder"}
    ]})");

    CHECK_EQ(links("local value = project.Source.Foo"), "Foo -> Source/Foo.luau, Source -> Source/");
    CHECK_EQ(links("local value = project.Source.Deep.Inner"), "Deep -> Source/Deep/, Inner -> Source/Deep/Inner/init.luau, Source -> Source/");
    CHECK_EQ(links("local value = project:FindFirstChild(\"Source\")"), "\"Source\" -> Source/");

    // A folder with no descendant files has nowhere on disk to reveal.
    CHECK_EQ(links("local value = project.Empty"), "");
}

TEST_CASE_FIXTURE(NavigationLinkFixture, "folder_navigation_requires_client_opt_in")
{
    lune();
    tempDir.touch_child(".git");
    tempDir.write_child("Source/Foo.luau", "return {}\n");
    tempDir.write_child("Package/init.luau", "return {}\n");

    client->configStore.emplace(workspace.rootUri, client->globalConfig);
    for (bool folderNavigation : {false, true})
    {
        for (bool enabled : {true, false})
        {
            CAPTURE(folderNavigation);
            CAPTURE(enabled);
            client->folderNavigation = folderNavigation;
            client->configStore.at(workspace.rootUri).documentLinks.enabled = enabled;
            // Other clients would open a folder as a file and can't run VS Code commands, so
            // they never receive folders. Files are unaffected.
            CHECK_EQ(definition("local value = project.Sou|rce.Foo"), folderNavigation ? "Source/" : "");
            CHECK_EQ(definition("local value = project.Source.F|oo"), "Source/Foo.luau");
            CHECK_EQ(definition("local value = project.Pack|age"), "Package/init.luau");
            std::string expected = enabled ? (folderNavigation ? "Foo -> Source/Foo.luau, Source -> Source/" : "Foo -> Source/Foo.luau") : "";
            CHECK_EQ(links("local value = project.Source.Foo"), expected);
        }
    }
}

TEST_CASE("initialization_options_opt_in_to_folder_navigation")
{
    CHECK_FALSE(json::object().get<InitializationOptions>().folderNavigation);
    CHECK(json{{"folderNavigation", true}}.get<InitializationOptions>().folderNavigation);
    CHECK_FALSE(json{{"fflags", json::object()}}.get<InitializationOptions>().folderNavigation);
}

// Creating symlinks needs extra privileges on Windows.
#ifndef _WIN32
TEST_CASE_FIXTURE(Fixture, "workspace_path_preserves_symlinked_workspace_spelling")
{
    tempDir.write_child("Real/Source/Foo.luau", "return {}\n");
    auto root = Uri::file(tempDir.path());
    auto real = root.resolvePath("Real");
    auto linked = root.resolvePath("Linked");
    std::filesystem::create_directory_symlink(real.fsPath(), linked.fsPath());

    // The editor opened the symlinked path, so resolved paths are mapped back onto it.
    CHECK_EQ(toWorkspacePath(linked, real), linked);
    CHECK_EQ(toWorkspacePath(linked, real.resolvePath("Source")), linked.resolvePath("Source"));
    // Paths outside the workspace, and workspaces without symlinks, are left alone.
    CHECK_EQ(toWorkspacePath(linked, root.resolvePath("Other")), root.resolvePath("Other"));
    CHECK_EQ(toWorkspacePath(real, real.resolvePath("Source")), real.resolvePath("Source"));
}

TEST_CASE_FIXTURE(Fixture, "navigation_targets_keep_the_spelling_of_nested_symlinked_workspaces")
{
    // The workspace is a folder inside the repository, opened through a symlink, so the
    // repository root itself is outside the workspace's spelling.
    tempDir.touch_child("Real/.git");
    tempDir.write_child("Real/Sub/Source/Foo.luau", "return {}\n");
    auto root = Uri::file(tempDir.path());
    std::filesystem::create_directory_symlink(root.resolvePath("Real").fsPath(), root.resolvePath("Linked").fsPath());
    auto nestedRoot = root.resolvePath("Linked/Sub");

    client->folderNavigation = true;
    client->globalConfig.platform.type = LSPPlatformConfig::Lune;
    WorkspaceFolder nested(client.get(), "nested", nestedRoot, std::nullopt);
    nested.setupWithConfiguration(client->globalConfig);
    nested.isReady = true;

    auto definition = [&](const std::string& markedSource)
    {
        auto [source, position] = sourceWithMarker(markedSource);
        auto document = Luau::LanguageServer::newDocument(nested, "main.luau", source);
        lsp::DefinitionParams params;
        params.textDocument = lsp::TextDocumentIdentifier{document};
        params.position = position;
        auto result = nested.gotoDefinition(params, nullptr);
        REQUIRE_EQ(result.size(), 1);
        return result[0].uri;
    };
    CHECK_EQ(definition("local value = project.Sub.Source.F|oo"), nestedRoot.resolvePath("Source/Foo.luau"));
    CHECK_EQ(definition("local value = project.Sub.Sou|rce"), nestedRoot.resolvePath("Source"));
}
#endif
