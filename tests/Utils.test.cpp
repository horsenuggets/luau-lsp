#include "doctest.h"
#include "Analyze/AnalyzeCli.hpp"
#include "LSP/Utils.hpp"
#include "LuauFileUtils.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "TempDir.h"

#include <cerrno>
#include <set>
#include <stdexcept>

#ifndef _WIN32
#include <unistd.h>
#endif

TEST_SUITE_BEGIN("UtilsTest");

TEST_CASE("getAncestorPath finds ancestor from given name")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "Module", nullptr), "game/ReplicatedStorage/Module");
}

TEST_CASE("getAncestorPath handles when ancestor is not found")
{
    CHECK_FALSE(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "NonExistent", nullptr).has_value());
}

TEST_CASE("getAncestorPath handles when ancestor is root of DataModel node")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "game", nullptr), "game");
}

TEST_CASE("getAncestorPath handles when ancestor is root of non-DataModel node")
{
    SourceNode node("Foo", "ClassName", {}, {});

    CHECK_EQ(getAncestorPath("ProjectRoot/Bar", "Foo", &node), "ProjectRoot");
}

TEST_CASE("getAncestorPath handles when ancestor is root of non-DataModel node and its name has multiple occurrences")
{
    SourceNode node("Foo", "ClassName", {}, {});

    CHECK_EQ(getAncestorPath("ProjectRoot/Bar/Foo/Baz", "Foo", &node), "ProjectRoot/Bar/Foo");
}

TEST_CASE("getAncestorPath returns nothing when ancestorName == current name, and no ancestor of name found")
{
    CHECK_FALSE(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "Foo", nullptr).has_value());
}

TEST_CASE("getAncestorPath handles when ancestor name is the same as current name")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Module", "Module", nullptr), "game/ReplicatedStorage/Module");
}

TEST_CASE("getAncestorPath handles when ancestor name is a substring of a child folder name")
{
    // "Foo" appears as a suffix substring inside "PrefixFoo" — searching for
    // "/Foo/" avoids matching inside "PrefixFoo/".
    CHECK_EQ(
        getAncestorPath(
            "game/Packages/_Workspace/Foo/Foo/Components/PrefixFoo/PrefixFoo",
            "Foo",
            nullptr),
        "game/Packages/_Workspace/Foo/Foo");

    // Prefix substring case: "Foo" inside "FooSuffix"
    CHECK_EQ(getAncestorPath("game/Root/Foo/Components/FooSuffix/Script", "Foo", nullptr), "game/Root/Foo");

    // Suffix substring case: "Bar" inside "PrefixBar"
    CHECK_EQ(getAncestorPath("game/Root/Bar/Components/PrefixBar/Script", "Bar", nullptr), "game/Root/Bar");
}

TEST_CASE("convertToScriptPath handles when path is empty")
{
    CHECK_EQ(convertToScriptPath(""), "");
}

TEST_CASE("convertToScriptPath handles when path is root")
{
    CHECK_EQ(convertToScriptPath("game"), "game");
}

TEST_CASE("convertToScriptPath handles when path is a single folder")
{
    CHECK_EQ(convertToScriptPath("game/ReplicatedStorage"), "game.ReplicatedStorage");
}

TEST_CASE("convertToScriptPath handles whitespaces in path")
{
    CHECK_EQ(convertToScriptPath("game/Replicated Storage/Common"), "game[\"Replicated Storage\"].Common");
}

TEST_CASE("convertToScriptPath handles non-identifier characters in path")
{
    CHECK_EQ(convertToScriptPath("game/ReplicatedStorage/Packages/react-spring"), "game.ReplicatedStorage.Packages[\"react-spring\"]");
}

TEST_CASE("convertToScriptPath handles relative paths")
{
    CHECK_EQ(convertToScriptPath("../Child/Foo"), "script.Parent.Child.Foo");
}

TEST_CASE("convertToScriptPath handles path where name contains dot")
{
    CHECK_EQ(convertToScriptPath("../Child.Foo"), "script.Parent[\"Child.Foo\"]");
}

TEST_CASE("getHomeDirectory finds a home directory")
{
    // we cannot confirm *what* the home directory is
    // since it varies per test runner
    CHECK(getHomeDirectory());
}

TEST_CASE("resolvePath resolves paths including tilde expansions")
{
    CHECK_EQ(resolvePath("C:/Users/test/foo.lua"), "C:/Users/test/foo.lua");

    auto home = getHomeDirectory();
    REQUIRE(home);

    CHECK_EQ(resolvePath("~/foo.lua"), Luau::FileUtils::joinPaths(*home, "foo.lua"));
}

TEST_CASE("isDataModel returns true when path starts with game")
{
    CHECK_EQ(isDataModel("game/ReplicatedStorage"), true);
}

TEST_CASE("isDataModel returns false when path starts with ProjectRoot")
{
    CHECK_EQ(isDataModel("ProjectRoot/Foo/Bar"), false);
}

TEST_CASE("getFirstLine returns first line")
{
    CHECK_EQ(getFirstLine("--##{'x':true}\nhello = world"), "--##{'x':true}");
}

TEST_CASE("getFirstLine returns string when there is no newline")
{
    CHECK_EQ(getFirstLine(""), "");
    CHECK_EQ(getFirstLine("testing"), "testing");
}

TEST_CASE("readFile can handle non-ASCII characters in path")
{
    auto path = Luau::FileUtils::joinPaths(*Luau::FileUtils::getCurrentWorkingDirectory(), "tests/testdata/non-ascii/ō.luau");
    auto result = Luau::FileUtils::readFile(path);
    REQUIRE(result);
    CHECK_EQ(*result, "local _ = 1");
}

TEST_CASE("traverseDirectory can handle non-ASCII characters in path")
{
    auto basePath = Luau::FileUtils::joinPaths(*Luau::FileUtils::getCurrentWorkingDirectory(), "tests/testdata/non-ascii");

    std::vector<std::string> paths;
    Luau::FileUtils::traverseDirectoryRecursive(basePath,
        [&](const auto& path)
        {
            paths.push_back(path);
        });

    CHECK_EQ(paths.size(), 2);

    paths.clear();
    auto nonAsciiBasePath = Luau::FileUtils::joinPaths(*Luau::FileUtils::getCurrentWorkingDirectory(), "tests/testdata/non-ascii/Рабочий стол");
    Luau::FileUtils::traverseDirectoryRecursive(nonAsciiBasePath,
        [&](const auto& path)
        {
            paths.push_back(path);
        });

    CHECK_EQ(paths.size(), 1);
}

TEST_CASE("traverseDirectory succeeds for readable empty directories")
{
    TempDir directory("traverse_empty");
    size_t callbacks = 0;
    auto callback = [&](const std::string&) { ++callbacks; };

    CHECK(Luau::FileUtils::traverseDirectory(directory.path(), callback));
    CHECK(Luau::FileUtils::traverseDirectoryRecursive(directory.path(), callback));
    CHECK_EQ(callbacks, 0);
}

TEST_CASE("traverseDirectory preserves shallow and recursive traversal")
{
    TempDir directory("traverse_contents");
    auto file = directory.touch_child("file.luau");
    auto nestedFile = directory.touch_child("nested/child.luau");
    auto nestedDirectory = directory.path() + "/nested";
    std::filesystem::create_directory(directory.path() + "/empty");

    std::set<std::string> paths;
    auto callback = [&](const std::string& path)
    {
        paths.insert(path);
        // Callback errors must not be mistaken for enumeration errors.
        errno = EACCES;
    };

    CHECK(Luau::FileUtils::traverseDirectory(directory.path(), callback));
    CHECK((paths == std::set<std::string>{file, nestedDirectory, directory.path() + "/empty"}));

    paths.clear();
    CHECK(Luau::FileUtils::traverseDirectoryRecursive(directory.path(), callback));
    CHECK((paths == std::set<std::string>{file, nestedFile}));
}

TEST_CASE("traverseDirectory rejects missing and non-directory paths")
{
    TempDir directory("traverse_invalid");
    auto file = directory.touch_child("file.luau");
    size_t callbacks = 0;
    auto callback = [&](const std::string&) { ++callbacks; };

    CHECK_FALSE(Luau::FileUtils::traverseDirectory(directory.path() + "/missing", callback));
    CHECK_FALSE(Luau::FileUtils::traverseDirectoryRecursive(directory.path() + "/missing", callback));
    CHECK_FALSE(Luau::FileUtils::traverseDirectory(file, callback));
    CHECK_FALSE(Luau::FileUtils::traverseDirectoryRecursive(file, callback));
    CHECK_EQ(callbacks, 0);
}

#ifndef _WIN32
TEST_CASE("traverseDirectory skips symbolic links")
{
    TempDir directory("traverse_symlinks");
    auto file = directory.touch_child("nested/file.luau");
    std::filesystem::create_directory_symlink(directory.path(), directory.path() + "/nested/cycle");
    std::filesystem::create_symlink(file, directory.path() + "/linked-file");
    std::filesystem::create_symlink(directory.path() + "/missing", directory.path() + "/broken-link");

    std::set<std::string> paths;
    CHECK(Luau::FileUtils::traverseDirectoryRecursive(directory.path(), [&](const std::string& path) { paths.insert(path); }));
    CHECK(paths == std::set<std::string>{file});
}

TEST_CASE("traverseDirectory reports unreadable nested directories")
{
    if (geteuid() == 0)
    {
        MESSAGE("Permission denial cannot be tested reliably when running as root.");
        return;
    }

    TempDir directory("traverse_unreadable");
    directory.touch_child("nested/file.luau");
    auto nested = directory.path() + "/nested";
    auto permissions = std::filesystem::status(nested).permissions();
    std::filesystem::permissions(nested, std::filesystem::perms::none);

    auto callback = [](const std::string&) {};
    bool shallowSuccess = Luau::FileUtils::traverseDirectory(directory.path(), callback);
    bool recursiveSuccess = Luau::FileUtils::traverseDirectoryRecursive(directory.path(), callback);
    bool directSuccess = Luau::FileUtils::traverseDirectory(nested, callback);
    std::string discoveryError;
    try
    {
        getFilesToAnalyze({directory.path()});
    }
    catch (const std::runtime_error& error)
    {
        discoveryError = error.what();
    }
    std::error_code restoreError;
    std::filesystem::permissions(nested, permissions, restoreError);

    REQUIRE_FALSE(restoreError);
    CHECK(shallowSuccess);
    CHECK_FALSE(recursiveSuccess);
    CHECK_FALSE(directSuccess);
    CHECK(discoveryError.find("Cannot enumerate analysis directory") != std::string::npos);
}
#endif

TEST_CASE("writeFileIfModified writes content to a new file")
{
    TempDir t("write_file_new");
    auto path = t.path() + "/test.txt";

    bool success = Luau::FileUtils::writeFileIfModified(path, "hello world");
    CHECK(success);

    auto content = Luau::FileUtils::readFile(path);
    REQUIRE(content);
    CHECK_EQ(*content, "hello world");
}

TEST_CASE("writeFileIfModified overwrites an existing file")
{
    TempDir t("write_file_overwrite");
    auto path = t.write_child("existing.txt", "original content");

    bool success = Luau::FileUtils::writeFileIfModified(path, "new content");
    CHECK(success);

    auto content = Luau::FileUtils::readFile(path);
    REQUIRE(content);
    CHECK_EQ(*content, "new content");
}

TEST_CASE("writeFileIfModified writes empty content")
{
    TempDir t("write_file_empty");
    auto path = t.path() + "/empty.txt";

    bool success = Luau::FileUtils::writeFileIfModified(path, "");
    CHECK(success);

    auto content = Luau::FileUtils::readFile(path);
    REQUIRE(content);
    CHECK_EQ(*content, "");
}

TEST_CASE("writeFileIfModified returns false for invalid path")
{
    // Attempt to write to a non-existent directory
    bool success = Luau::FileUtils::writeFileIfModified("/nonexistent/directory/path/file.txt", "content");
    CHECK_FALSE(success);
}

TEST_CASE("writeFileIfModified only writes when content differs from existing file")
{
    TempDir t("write_file_skip");
    auto path = t.write_child("same_content.txt", "unchanged content");

    // Set an older modification time so we can verify it's not updated
    auto oldTime = std::filesystem::last_write_time(path) - std::chrono::seconds(10);
    std::filesystem::last_write_time(path, oldTime);
    auto timeBefore = std::filesystem::last_write_time(path);

    // Write the same content - should succeed but not actually write
    bool success = Luau::FileUtils::writeFileIfModified(path, "unchanged content");
    CHECK(success);

    // File modification time should be unchanged
    auto timeAfterUnchanged = std::filesystem::last_write_time(path);
    CHECK_EQ(timeBefore, timeAfterUnchanged);

    // Content should still be the same
    auto contentUnchanged = Luau::FileUtils::readFile(path);
    REQUIRE(contentUnchanged);
    CHECK_EQ(*contentUnchanged, "unchanged content");

    // Write different content - should actually write
    success = Luau::FileUtils::writeFileIfModified(path, "modified content");
    CHECK(success);

    // File modification time should be updated
    auto timeAfterModified = std::filesystem::last_write_time(path);
    CHECK_NE(timeBefore, timeAfterModified);

    auto contentModified = Luau::FileUtils::readFile(path);
    REQUIRE(contentModified);
    CHECK_EQ(*contentModified, "modified content");
}

#ifdef _WIN32
TEST_CASE("toUtf8 converts wide string to UTF-8 string")
{
    // Test Polish character 'ż' (U+017C)
    // L"Użytkownik" = { 'U', 0x017C, 'y', 't', 'k', 'o', 'w', 'n', 'i', 'k' }
    std::wstring polish = L"U\u017Cytkownik";
    std::string utf8 = Luau::FileUtils::toUtf8(polish);
    CHECK_EQ(utf8, "Użytkownik");

    // Test Cyrillic: "Рабочий стол" (Russian for "Desktop")
    std::wstring cyrillic = L"\u0420\u0430\u0431\u043E\u0447\u0438\u0439 \u0441\u0442\u043E\u043B";
    std::string utf8Cyr = Luau::FileUtils::toUtf8(cyrillic);
    CHECK_EQ(utf8Cyr, "Рабочий стол");
}

TEST_CASE("fromUtf8 and toUtf8 are inverses for Unicode paths")
{
    std::string original = "C:/Users/Użytkownik/Desktop/Рабочий стол/test.luau";
    std::wstring wide = Luau::FileUtils::fromUtf8(original);
    std::string roundTrip = Luau::FileUtils::toUtf8(wide);
    CHECK_EQ(original, roundTrip);
}

TEST_CASE("getUtf8CommandLineArgs returns command line arguments")
{
    auto args = Luau::FileUtils::getUtf8CommandLineArgs();
    // Should have at least the program name
    CHECK_FALSE(args.empty());
}
#endif

TEST_SUITE_END();
