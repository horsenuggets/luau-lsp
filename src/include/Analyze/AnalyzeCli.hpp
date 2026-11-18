#pragma once

#include <chrono>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "Protocol/Diagnostics.hpp"

#include "argparse/argparse.hpp"

#include "Analyze/CliClient.hpp"

struct WorkspaceFileResolver;
class WorkspaceFolder;
namespace Luau
{
struct TypeError;
}

std::unordered_map<std::string, std::string> processDefinitionsFilePaths(const argparse::ArgumentParser& program);

enum class ReportFormat
{
    Default,
    Luacheck,
    Gnu,
};

struct FilePathInformation
{
    Uri uri;
    std::string relativePath;
};

FilePathInformation getFilePath(const WorkspaceFileResolver* fileResolver, const std::string& moduleName);

class AnalysisJsonlReporter
{
public:
    explicit AnalysisJsonlReporter(std::ostream& output);
    void start(const std::string& version, const std::string& platform, const std::string& solver);
    void progress(const std::string& phase, const std::string& state, size_t completed, std::optional<size_t> total,
        const std::string& path = "");
    bool diagnostic(const std::string& path, const std::string& category, const lsp::Diagnostic& diagnostic);
    void finish(size_t completed, size_t total);
    size_t errorCount() const;
    size_t warningCount() const;

private:
    void emit(json event);
    std::ostream& output;
    std::chrono::steady_clock::time_point started;
    std::set<std::string> diagnostics;
    size_t errors = 0;
    size_t warnings = 0;
};

bool reportTypeErrorJsonl(WorkspaceFolder& workspace, const Luau::TypeError& error, AnalysisJsonlReporter& reporter);
bool analyzeFileJsonl(WorkspaceFolder& workspace, const std::string& path, AnalysisJsonlReporter& reporter);
bool analyzeFilesJsonl(WorkspaceFolder& workspace, const std::vector<std::string>& paths, AnalysisJsonlReporter& reporter);
bool analyzeFile(WorkspaceFolder& workspace, const std::string& path, ReportFormat format, bool annotate);
std::vector<std::string> getFilesToAnalyze(const std::vector<std::string>& paths, WorkspaceFolder* workspace = nullptr);
void applySettings(const std::string& settingsContents, CliClient& client);
/// Adds the built-in definitions for the configured platform. Returns false if Roblox definitions are needed but none are embedded.
bool addBuiltinDefinitions(CliClient& client);
int startAnalyze(const argparse::ArgumentParser& program);
