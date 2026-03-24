#include "manifest.h"
#include "utils.h"
#include <nlohmann/json.hpp>

Manifest Manifest::Parse(const std::string& jsonString) {
    Manifest manifest;
    try {
        nlohmann::json json = nlohmann::json::parse(jsonString);
        manifest.m_runPath = json.value("run_path", "");
        manifest.m_manifestVersion = json.value("manifest_version", "");
        if (!manifest.m_manifestVersion.empty()) {
            Utils::Log("[INFO] manifest version=" + manifest.m_manifestVersion);
        }
        Utils::Log("[INFO] manifest parsed: run_path=" + manifest.m_runPath);
        if (json.contains("options") && json["options"].is_object()) {
            for (auto& [key, value] : json["options"].items()) {
                if (!value.is_object()) {
                    continue;
                }
                ManifestOption option;
                option.key = key;
                option.source = value.value("source", "");
                option.extract = value.value("extract", "");
                manifest.m_options[key] = option;
            }
            Utils::Log("[INFO] manifest option count: " + std::to_string(manifest.m_options.size()));
        }
        if (json.contains("files") && json["files"].is_array()) {
            for (auto& item : json["files"]) {
                ManifestFile file;
                file.path = item["path"];
                file.size = item["size"];
                file.sha256 = item["sha256"];
                file.compressedSize = item.value("compressed_size", 0);
                file.compressedSha256 = item.value("compressed_sha256", "");
                file.action = item.value("action", "update");
                file.skipIfExists = item.value("skip_if_exists", false);
                file.mtime = item.value("mtime", 0.0);
                manifest.m_files.push_back(file);
            }
            Utils::Log("[INFO] manifest file count: " + std::to_string(manifest.m_files.size()));
        } else {
            Utils::Log("[WARN] manifest has no files array");
        }
    } catch (const std::exception& e) {
        Utils::Log(std::string("[ERROR] failed to parse manifest: ") + e.what());
    } catch (...) {
        Utils::Log("[ERROR] failed to parse manifest: unknown exception");
    }
    return manifest;
}
