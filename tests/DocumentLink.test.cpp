#include "Fixture.h"
#include "doctest.h"

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
