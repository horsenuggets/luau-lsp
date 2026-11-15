#include "doctest.h"
#include "Fixture.h"

#include "LSP/IostreamHelpers.hpp"

TEST_SUITE_BEGIN("GoToDefinition");

TEST_CASE_FIXTURE(Fixture, "local_variable_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local value = 5

        local y = val|ue
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 14});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 19});
}

TEST_CASE_FIXTURE(Fixture, "local_variable_definition_pointing_to_a_table")
{
    auto [source, position] = sourceWithMarker(R"(
        local process = { call = function() end }
        local value = pro|cess.call()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{1, 14});
    CHECK_EQ(result[0].range.end, lsp::Position{1, 21});
}

TEST_CASE_FIXTURE(Fixture, "local_inlined_primitive_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {
            value = 5
        }

        local y = T.val|ue
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 17});
}

TEST_CASE_FIXTURE(Fixture, "local_separate_primitive_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {}
        T.value = 5

        local y = T.val|ue
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 10});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 15});
}

TEST_CASE_FIXTURE(Fixture, "local_inlined_function_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {
            useFunction = function() end
        }

        local y = T.useFu|nction()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 23});
}

TEST_CASE_FIXTURE(Fixture, "local_separate_anonymous_function_table_property_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {}
        T.useFunction = function()
        end

        local y = T.useFu|nction()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 10});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 21});
}

TEST_CASE_FIXTURE(Fixture, "local_named_function_table_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local T = {}
        function T.useFunction()
        end

        local y = T.useFu|nction()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "type_alias_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        --!strict
        type Foo = string

        local y: Fo|o = ""
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 8});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 25});
}

TEST_CASE_FIXTURE(Fixture, "methods_on_explicitly_defined_self")
{
    auto [source, position] = sourceWithMarker(R"(
        local Test = {}
        Test.__index = Test

        function Test.new()
            local self = setmetatable({}, Test)
            return self
        end

        function Test.someFunc(self: Test)
        end

        function Test:anotherFunc()
        end

        function Test.anotherCallingFunc(self: Test)
            self:some|Func()
            self:anotherFunc()
        end

        export type Test = typeof(Test.new())

        return Test
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{9, 22});
    CHECK_EQ(result[0].range.end, lsp::Position{9, 30});

    params.position.line += 1;
    result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{12, 22});
    CHECK_EQ(result[0].range.end, lsp::Position{12, 33});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_inlined_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        return {
            useFunction = function(x: string)
            end,
        }
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 23});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_indirect_variable_inlined_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local T = {
            useFunction = function(x: string)
            end,
        }
        return T
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 23});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_table_property_referencing_local_variable_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local function useFunction(x: string)
        end

        return {
            useFunction = useFunction
        }
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{6, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{6, 23});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_separate_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local T = {}
        T.useFunction = function(x: string)
        end

        return T
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 10});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 21});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_named_function_table_property_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local T = {}
        function T.useFunction(x: string)
        end

        return T
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y = utilities.useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{3, 19});
    CHECK_EQ(result[0].range.end, lsp::Position{3, 30});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_imported_type_alias_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        export type Foo = string

        return {}
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y: utilities.Fo|o = ""
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 8});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 32});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_imported_type_alias_definition_after_check_without_retaining_type_graphs")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        export type Foo = string

        return {}
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y: utilities.Fo|o = ""
    )");
    auto document = newDocument("main.luau", source);

    // This test explicitly expects type graphs to not be retained (i.e., the required module scope was cleared)
    // We should still be able to find the type references.
    workspace.checkSimple(workspace.fileResolver.getModuleName(document), /* cancellationToken= */ nullptr);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 8});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 32});
}

TEST_CASE_FIXTURE(Fixture, "cross_module_imported_type_alias_definition_after_check_without_retaining_type_graphs_goto_type_definition")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        export type Foo = string

        return {}
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local utilities = require(game.Testing.useFunction)

        local y: utilities.Fo|o = ""
    )");
    auto document = newDocument("main.luau", source);

    // This test explicitly expects type graphs to not be retained (i.e., the required module scope was cleared)
    // We should still be able to find the type references.
    workspace.checkSimple(workspace.fileResolver.getModuleName(document), /* cancellationToken= */ nullptr);

    auto params = lsp::TypeDefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoTypeDefinition(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->uri, required);
    CHECK_EQ(result->range.start, lsp::Position{2, 8});
    CHECK_EQ(result->range.end, lsp::Position{2, 32});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_of_a_named_function_returns_the_underlying_definition_and_not_the_require_stmt")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        local function useFunction(x: string)
        end

        return useFunction
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local useFunction = require(game.Testing.useFunction)

        local y = useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 23});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 34});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_of_an_anonymous_function_returns_the_underlying_definition_and_not_the_require_stmt")
{
    auto required = newDocument("required.luau", R"(
        --!strict
        return function(x: string)
        end
    )");
    registerDocumentForVirtualPath(required, "game/Testing/useFunction");

    auto [source, position] = sourceWithMarker(R"(
        --!strict
        local useFunction = require(game.Testing.useFunction)

        local y = useFu|nction("testing")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, required);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 15});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 15});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_works_for_a_string_require_path")
{
    auto [source, position] = sourceWithMarker(R"(
        local X = require("./te|st")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, workspace.rootUri.resolvePath("test.lua"));
    CHECK_EQ(result[0].range.start, lsp::Position{0, 0});
    CHECK_EQ(result[0].range.end, lsp::Position{0, 0});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_works_for_a_roblox_require_path")
{
    loadSourcemap(R"({
        "name": "Game",
        "className": "DataModel",
        "children": [
            {
                "name": "ReplicatedStorage",
                "className": "ReplicatedStorage",
                "children": [{ "name": "Test", "className": "ModuleScript", "filePaths": ["source.luau"] }]
            }
        ]
    })");

    auto [source, position] = sourceWithMarker(R"(
        local X = require(game.ReplicatedStorage.Te|st)
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, workspace.rootUri.resolvePath("source.luau"));
    CHECK_EQ(result[0].range.start, lsp::Position{0, 0});
    CHECK_EQ(result[0].range.end, lsp::Position{0, 0});
}

TEST_CASE_FIXTURE(Fixture, "property_on_table_type_without_actual_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        type Process = {
            spawn: (string) -> string
        }

        local process = {} :: Process
        local value = process.sp|awn()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 17});
}

TEST_CASE_FIXTURE(Fixture, "property_on_the_return_of_a_function_call")
{
    auto [source, position] = sourceWithMarker(R"(
        type Result = {
            unwrap: (Result) -> boolean
        }

        type Process = {
            spawn: (string) -> Result
        }

        local process = {} :: Process
        local value = process.spawn("test"):unwr|ap()
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range.start, lsp::Position{2, 12});
    CHECK_EQ(result[0].range.end, lsp::Position{2, 18});
}

TEST_CASE_FIXTURE(Fixture, "go_to_type_definition_returns_the_table_location")
{
    auto [source, position] = sourceWithMarker(R"(
        type Process = {
            spawn: (string) -> Result
        }

        local process = {} :: Process
        local value = pr|ocess.spawn("test")
    )");
    auto document = newDocument("main.luau", source);

    auto params = lsp::TypeDefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoTypeDefinition(params, nullptr);
    REQUIRE(result);
    CHECK_EQ(result->uri, document);
    CHECK_EQ(result->range.start, lsp::Position{1, 23});
    CHECK_EQ(result->range.end, lsp::Position{3, 9});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.gotoDefinition(lsp::DefinitionParams{{{document}}}, cancellationToken), RequestCancelledException);
}

TEST_CASE_FIXTURE(Fixture, "go_to_type_definition_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.gotoTypeDefinition(lsp::TypeDefinitionParams{{{document}}}, cancellationToken), RequestCancelledException);
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_for_property_on_union_type_with_same_definition")
{
    auto document = newDocument("main.luau", R"(
        type BaseNode<HOS> = {
            has_one_supporter: HOS,
        }

        type Node = BaseNode<true> | BaseNode<false>

        local x: Node = {} :: any
        local y = x.has_one_supporter
    )");

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = lsp::Position{8, 20}; // x.has_one_supporter

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{2, 12}, {2, 29}});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_for_property_on_union_type_with_different_definitions")
{
    auto document = newDocument("main.luau", R"(
        type A = {
            prop: number,
        }

        type B = {
            prop: string,
        }

        type Union = A | B

        local x: Union = {} :: any
        local y = x.prop
    )");

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = lsp::Position{12, 20}; // x.prop

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 2);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[1].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{2, 12}, {2, 16}});
    CHECK_EQ(result[1].range, lsp::Range{{6, 12}, {6, 16}});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_on_original_global_function_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        function global|Function()
        end
    )");

    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{1, 17}, {1, 31}});
}

TEST_CASE_FIXTURE(Fixture, "go_to_definition_on_original_local_function_definition")
{
    auto [source, position] = sourceWithMarker(R"(
        local function local|Function()
        end
    )");

    auto document = newDocument("main.luau", source);

    auto params = lsp::DefinitionParams{};
    params.textDocument = lsp::TextDocumentIdentifier{document};
    params.position = position;

    auto result = workspace.gotoDefinition(params, nullptr);
    REQUIRE_EQ(result.size(), 1);
    CHECK_EQ(result[0].uri, document);
    CHECK_EQ(result[0].range, lsp::Range{{1, 23}, {1, 36}});
}

namespace
{
struct InstanceNavigationFixture : Fixture
{
    lsp::DefinitionResult definition(const std::string& markedSource, const std::string& from = "main.luau")
    {
        auto [source, position] = sourceWithMarker(markedSource);
        auto uri = workspace.rootUri.resolvePath(from);
        if (workspace.fileResolver.getTextDocument(uri))
            updateDocument(uri, source);
        else
            newDocument(from, source);
        lsp::DefinitionParams params;
        params.textDocument = lsp::TextDocumentIdentifier{uri};
        params.position = position;
        return workspace.gotoDefinition(params, nullptr);
    }

    void target(const std::string& markedSource, const std::string& expected, const std::string& from = "main.luau")
    {
        CAPTURE(markedSource);
        auto result = definition(markedSource, from);
        REQUIRE_EQ(result.size(), 1);
        CHECK_EQ(result[0].uri, workspace.rootUri.resolvePath(expected));
        CHECK_EQ(result[0].range, (lsp::Range{{0, 0}, {0, 0}}));
    }

    void roblox()
    {
        loadSourcemap(R"({
            "name": "Game", "className": "DataModel", "children": [
                {"name": "Main", "className": "ModuleScript", "filePaths": ["main.luau"], "children": [
                    {"name": "Child", "className": "ModuleScript", "filePaths": ["Child/init.luau"], "children": [
                        {"name": "Leaf", "className": "LocalScript", "filePaths": ["Child/Leaf.client.luau"]}
                    ]},
                    {"name": "Source", "className": "ModuleScript", "filePaths": ["Source.luau"]},
                    {"name": "Name", "className": "ModuleScript", "filePaths": ["Name.luau"]},
                    {"name": "GetChildren", "className": "ModuleScript", "filePaths": ["GetChildren.luau"]},
                    {"name": "Empty", "className": "Folder"}
                ]},
                {"name": "ReplicatedStorage", "className": "ReplicatedStorage", "children": [
                    {"name": "Library", "className": "ModuleScript", "filePaths": ["Library.lua"]}
                ]}
            ]
        })");
    }

    void lune()
    {
        switchToStandardPlatform();
        client->globalConfig.platform.type = LSPPlatformConfig::Lune;
    }
};
} // namespace

TEST_CASE_FIXTURE(InstanceNavigationFixture, "standalone_instance_navigation_segments")
{
    roblox();
    for (const auto& source : {
             "local value = scr|ipt.Child.Leaf",
             "local value = require(scr|ipt.Child.Leaf)",
             "local value = script.Child.Par|ent",
             "local value = game.Ma|in.Child.Leaf",
             "local value = script.Child[\"Par|ent\"]",
         })
        target(source, "main.luau");
    for (const auto& source : {
             "local value = script.Ch|ild.Leaf",
             "local value = script[\"Ch|ild\"].Leaf",
             "local value = (script).Ch|ild",
             "local value = script:WaitForChild(\"Ch|ild\")",
             "local value = script:WaitForChild(\"Ch|ild\", 5)",
             "local value = script:FindFirstChild(\"Ch|ild\")",
             "local value = script.Child.Leaf:FindFirstAncestor(\"Ch|ild\")",
             "local value = require(script.Ch|ild.Leaf)",
             "local value = require(script:WaitForChild(\"Ch|ild\", 5).Leaf)",
         })
        target(source, "Child/init.luau");
    for (const auto& source : {
             "local value = script.Child.Le|af",
             "local value = script[\"Child\"][\"Le|af\"]",
             "local value = (script.Child).Le|af",
             "local value = script:WaitForChild(\"Child\", 5).Le|af",
             "local value = require(script:WaitForChild(\"Child\", 5).Le|af)",
         })
        target(source, "Child/Leaf.client.luau");
    target("local value = scr|ipt", "Child/init.luau", "Child/init.luau");
    target("local value = script.Le|af", "Child/Leaf.client.luau", "Child/init.luau");
    target("local value = script.Par|ent", "main.luau", "Child/init.luau");
    target("local value = game:GetService(\"ReplicatedStorage\").Lib|rary", "Library.lua");
    target("local value = script:WaitForChild(\"Sou|rce\")", "Source.luau");
}

TEST_CASE_FIXTURE(InstanceNavigationFixture, "standalone_instance_navigation_does_not_invent_targets")
{
    roblox();
    for (const auto& source : {
             "local value = script.Mis|sing",
             "local value = script.Emp|ty",
             "local value = script[\"Missing\"].Ch|ild",
             "local value = script[unknown].Ch|ild",
             "local value = script:WaitForChild(unknown).Ch|ild",
             "local value = script:FindFirstChild(\"Child\", true).Le|af",
             "local value = script:Clone().Ch|ild",
             "local value = require(script.Emp|ty)",
             "local value = require(script[unknown].Ch|ild)",
             "local value = \"Ch|ild\"",
         })
    {
        CAPTURE(source);
        CHECK(definition(source).empty());
    }
    for (const auto& source : {
             "local value = script.Sou|rce",
             "local value = script[\"Na|me\"]",
             "local value = script.GetChil|dren",
             "local value = script:WaitForCh|ild(\"Child\")",
             "local value = script:WaitForChild(\"Child\", 5|)",
         })
    {
        CAPTURE(source);
        for (const auto& location : definition(source))
        {
            CHECK_NE(location.uri, workspace.rootUri.resolvePath("Source.luau"));
            CHECK_NE(location.uri, workspace.rootUri.resolvePath("Name.luau"));
            CHECK_NE(location.uri, workspace.rootUri.resolvePath("GetChildren.luau"));
            CHECK_NE(location.uri, workspace.rootUri.resolvePath("Child/init.luau"));
        }
    }
}

TEST_CASE_FIXTURE(InstanceNavigationFixture, "standalone_instance_navigation_preserves_local_and_property_definitions")
{
    roblox();
    for (const auto& name : {"script", "game", "project", "ordinary"})
    {
        std::string prefix = "local " + std::string(name) + " = { Child = 1 }\nlocal value = ";
        auto result = definition(prefix + name + ".Ch|ild");
        REQUIRE_EQ(result.size(), 1);
        CHECK_EQ(result[0].uri, workspace.rootUri.resolvePath("main.luau"));
        CHECK_EQ(result[0].range.start.line, 0);
        CHECK_NE(result[0].range.end.character, 0);
    }
    auto localResult = definition("local reference = script.Child\nlocal value = ref|erence");
    REQUIRE_EQ(localResult.size(), 1);
    CHECK_EQ(localResult[0].range, (lsp::Range{{0, 6}, {0, 15}}));
    auto shadowResult = definition("local script = {}\nlocal value = scr|ipt");
    REQUIRE_EQ(shadowResult.size(), 1);
    CHECK_EQ(shadowResult[0].range, (lsp::Range{{0, 6}, {0, 12}}));
    CHECK(definition("local reference = script\nlocal value = reference.Ch|ild").empty());
}

TEST_CASE_FIXTURE(InstanceNavigationFixture, "standalone_project_navigation_resolves_omitted_entry_points")
{
    tempDir.touch_child(".git");
    tempDir.write_child("init.luau", "return {}\n");
    tempDir.write_child("default.project.json", R"({"name":"Package","tree":{"$path":"."}})");
    loadSourcemap(R"({"name":"Package","className":"Folder","filePaths":["default.project.json"],"children":[
        {"name":"Main","className":"ModuleScript","filePaths":["main.luau"]}
    ]})");
    target("local value = pro|ject", "init.luau");
    target("local value = project.Ma|in", "main.luau");
    target("local value = script.Par|ent", "init.luau");
}

TEST_CASE_FIXTURE(InstanceNavigationFixture, "standalone_project_navigation_uses_mapped_script_redirects")
{
    tempDir.touch_child(".git");
    loadSourcemap(R"({"name":"Package","className":"ModuleScript","filePaths":["default.project.json","Source/init.luau"],"children":[
        {"name":"Main","className":"ModuleScript","filePaths":["main.luau"]}
    ]})");
    target("local value = pro|ject", "Source/init.luau");
    target("local value = script.Par|ent", "Source/init.luau");
}

TEST_CASE_FIXTURE(InstanceNavigationFixture, "lune_standalone_navigation_without_sourcemap")
{
    lune();
    tempDir.touch_child(".git");
    tempDir.write_child("main.luau", "return {}\n");
    tempDir.write_child("Sibling.lua", "return {}\n");
    tempDir.write_child("Child/init.luau", "return {}\n");
    tempDir.write_child("Child/Leaf.luau", "return {}\n");
    tempDir.write_child("Legacy/init.lua", "return {}\n");
    tempDir.write_child("RequirePath.luau", "return {}\n");
    target("local value = script.Require|Path", "RequirePath.luau");
    target("local value = scr|ipt", "main.luau");
    for (const auto& source : {
             "local value = script.Ch|ild.Leaf",
             "local value = script[\"Ch|ild\"].Leaf",
             "local value = script.Parent.Ch|ild",
             "local value = project.Ch|ild",
             "local value = (project).Ch|ild",
         })
        target(source, "Child/init.luau");
    target("local value = script.Sib|ling", "Sibling.lua");
    target("local value = project.Leg|acy", "Legacy/init.lua");
    target("local value = project.Child.Le|af", "Child/Leaf.luau");
    target("local value = script.Parent.Parent.Sib|ling", "Sibling.lua", "Child/init.luau");
    target("local value = script.Le|af", "Child/Leaf.luau", "Child/init.luau");
    target("local value = script.Parent.Le|af", "Child/Leaf.luau", "Child/init.luau");
}

TEST_CASE_FIXTURE(InstanceNavigationFixture, "lune_project_navigation_uses_nearest_git_file_or_directory")
{
    lune();
    tempDir.touch_child(".git/HEAD");
    tempDir.write_child("Child/init.luau", "return {}\n");
    tempDir.write_child("Nested/.git", "gitdir: ../.git/worktrees/nested\n");
    tempDir.write_child("Nested/Child/init.lua", "return {}\n");
    tempDir.write_child("Nested/Scripts/main.luau", "return {}\n");
    target("local value = project.Ch|ild", "Child/init.luau");
    target("local value = project.Ch|ild", "Nested/Child/init.lua", "Nested/Scripts/main.luau");
}

TEST_CASE_FIXTURE(InstanceNavigationFixture, "lune_navigation_project_child_mappings_and_directory_redirects")
{
    lune();
    tempDir.touch_child(".git");
    tempDir.write_child("default.project.json", R"({"tree":{"Alias":{"$path":"Source/Actual.lua"}}})");
    tempDir.write_child("Source/Actual.lua", "return {}\n");
    tempDir.write_child("Package/default.project.json", R"({"tree":{"$path":"Source"}})");
    tempDir.write_child("Package/Source/init.luau", "return {}\n");
    tempDir.write_child("Package/init.luau", "return false\n");
    tempDir.write_child("Direct/default.project.json", R"({"tree":{"$path":"Entry.lua"}})");
    tempDir.write_child("Direct/Entry.lua", "return {}\n");
    target("local value = project.Al|ias", "Source/Actual.lua");
    target("local value = script.Parent.Al|ias", "Source/Actual.lua");
    target("local value = project.Pack|age", "Package/Source/init.luau");
    target("local value = project.Dir|ect", "Direct/Entry.lua");
    target("local value = script.Parent.Pack|age", "Package/Source/init.luau");
}

TEST_CASE_FIXTURE(InstanceNavigationFixture, "lune_navigation_rejects_nonreferences_and_ambiguous_or_missing_sources")
{
    lune();
    tempDir.touch_child(".git");
    tempDir.write_child("Child/init.luau", "return {}\n");
    tempDir.touch_child("Empty/.keep");
    tempDir.touch_child("Both.lua");
    tempDir.touch_child("Both.luau");
    tempDir.touch_child("Mixed.lua");
    tempDir.touch_child("Mixed/init.lua");
    tempDir.touch_child("Double/init.lua");
    tempDir.touch_child("Double/init.luau");
    tempDir.touch_child("Source.luau");
    tempDir.touch_child("Name.luau");
    tempDir.touch_child("GetFullName.luau");
    tempDir.write_child("Loop/default.project.json", R"({"tree":{"$path":"."}})");
    tempDir.write_child("Broken/default.project.json", R"({"tree":{"$path":"Missing"}})");
    tempDir.write_child("Malformed/default.project.json", "not json");
    for (const auto& source : {
             "local value = project.Mis|sing",
             "local value = project.Emp|ty",
             "local value = project.Bo|th",
             "local value = project.Mix|ed",
             "local value = project.Doub|le",
             "local value = project.Lo|op",
             "local value = project.Brok|en",
             "local value = project.Malfor|med",
             "local value = project.Sou|rce",
             "local value = project[\"Na|me\"]",
             "local value = project.GetFull|Name",
             "local value = project[unknown].Ch|ild",
             "local value = project:WaitForChild(\"Ch|ild\")",
             "local value = game.Ch|ild",
             "local reference = script\nlocal value = reference.Ch|ild",
             "local project = {}\nlocal value = project.Ch|ild",
             "local script = {}\nlocal value = script.Ch|ild",
         })
    {
        CAPTURE(source);
        CHECK(definition(source).empty());
    }
}

TEST_CASE_FIXTURE(InstanceNavigationFixture, "lune_project_navigation_requires_a_git_root")
{
    lune();
    tempDir.write_child("Child.luau", "return {}\n");
    CHECK(definition("local value = project.Ch|ild").empty());
    target("local value = script.Ch|ild", "Child.luau");
    target("local value = scr|ipt", "main.luau");
}

TEST_CASE_FIXTURE(InstanceNavigationFixture, "lune_navigation_preserves_shadowed_local_definitions")
{
    lune();
    tempDir.touch_child(".git");
    tempDir.touch_child("Child.luau");
    for (const auto& name : {"script", "project"})
    {
        auto result = definition("local " + std::string(name) + " = { Child = 1 }\nlocal value = " + name + ".Ch|ild");
        REQUIRE_EQ(result.size(), 1);
        CHECK_EQ(result[0].uri, workspace.rootUri.resolvePath("main.luau"));
        CHECK_EQ(result[0].range.start.line, 0);
        CHECK_NE(result[0].range.end.character, 0);
    }
}

TEST_CASE_FIXTURE(InstanceNavigationFixture, "standard_platform_does_not_enable_instance_navigation")
{
    switchToStandardPlatform();
    tempDir.touch_child(".git");
    tempDir.write_child("Child.luau", "return {}\n");
    CHECK(definition("local value = script.Ch|ild").empty());
    CHECK(definition("local value = project.Ch|ild").empty());
}

TEST_SUITE_END();
