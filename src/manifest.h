#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

struct ManifestFile {
    std::string path;
    size_t size;
    std::string sha256;
    size_t compressedSize;
    std::string compressedSha256;
    std::string action; // "update" (default) or "delete"
    bool skipIfExists;  // NEW: Skip downloading if file exists locally
    double mtime;       
};

struct ManifestOption {
    std::string key;
    std::string source;
    std::string extract;
};

class Manifest {
public:
    static Manifest Parse(const std::string& jsonString);

    const std::vector<ManifestFile>& GetFiles() const { return m_files; }
    const std::string& GetRunPath() const { return m_runPath; }
    const std::string& GetManifestVersion() const { return m_manifestVersion; }
    const std::map<std::string, ManifestOption>& GetOptions() const { return m_options; }

private:
    std::vector<ManifestFile> m_files;
    std::string m_runPath;
    std::string m_manifestVersion;
    std::map<std::string, ManifestOption> m_options;
};
