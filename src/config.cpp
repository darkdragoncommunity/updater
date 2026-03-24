#include "config.h"
#include "crypto_utils.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace {
    std::string NormalizeInstallPath(const std::string& path) {
        std::filesystem::path installPath(path.empty() ? "." : path);
        if (installPath.is_relative()) {
            installPath = std::filesystem::current_path() / installPath;
        }
        return installPath.lexically_normal().string();
    }

    void AppendUniqueUrl(std::vector<std::string>& urls, const std::string& url) {
        if (url.empty()) {
            return;
        }
        if (std::find(urls.begin(), urls.end(), url) == urls.end()) {
            urls.push_back(url);
        }
    }
}

Config& Config::GetInstance() {
    static Config instance;
    return instance;
}

void Config::Load() {
    Utils::Log("[INFO] loading config.json");
    // 1. Load standard user config.json
    std::ifstream configFile(m_configPath);
    if (configFile.is_open()) {
        try {
            configFile >> m_configData;
            Utils::Log("[INFO] config.json loaded successfully");
        } catch (...) {
            Utils::Log("[WARN] failed to parse config.json, using defaults");
            m_configData = {{"language", "en"}};
        }
    } else {
        Utils::Log("[INFO] config.json not found, using defaults");
        m_configData = {{"language", "en"}};
    }

    // 2. Load system settings.bin
    Utils::Log("[INFO] loading settings.bin");
    std::ifstream settingsFile(m_settingsPath);
    if (settingsFile.is_open()) {
        std::string hexData;
        settingsFile >> hexData;
        std::string xored = CryptoUtils::FromHex(hexData);
        std::string decrypted = CryptoUtils::ProcessXOR(xored, m_encryptionKey);
        try {
            nlohmann::json settings = nlohmann::json::parse(decrypted);
            m_remotePatchUrl = settings.value("patch_url", "https://patch.mygame.com");
            m_remotePatchUrls.clear();
            AppendUniqueUrl(m_remotePatchUrls, m_remotePatchUrl);
            if (settings.contains("patch_mirror_urls") && settings["patch_mirror_urls"].is_array()) {
                for (const auto& value : settings["patch_mirror_urls"]) {
                    if (value.is_string()) {
                        AppendUniqueUrl(m_remotePatchUrls, value.get<std::string>());
                    }
                }
            }
            m_remoteDebugEnabled = settings.value("remote_debug_enabled", false);
            m_remoteDebugLogUrl = settings.value("remote_debug_log_url", "");
            m_socialUrls.clear();
            if (settings.contains("social_urls") && settings["social_urls"].is_object()) {
                for (const auto& [key, value] : settings["social_urls"].items()) {
                    if (value.is_string()) {
                        m_socialUrls[key] = value.get<std::string>();
                    }
                }
            }
            Utils::Log("[INFO] patch URL: " + m_remotePatchUrl);
            for (size_t i = 1; i < m_remotePatchUrls.size(); ++i) {
                Utils::Log("[INFO] patch mirror URL: " + m_remotePatchUrls[i]);
            }
        } catch (...) {
            m_remotePatchUrl = "https://patch.mygame.com";
            m_remotePatchUrls = {m_remotePatchUrl};
            m_remoteDebugEnabled = false;
            m_remoteDebugLogUrl.clear();
            m_socialUrls.clear();
            Utils::Log("[WARN] failed to parse settings.bin, using default patch URL");
        }
    } else {
        m_remotePatchUrl = "https://patch.mygame.com";
        m_remotePatchUrls = {m_remotePatchUrl};
        m_remoteDebugEnabled = false;
        m_remoteDebugLogUrl.clear();
        m_socialUrls.clear();
        Utils::Log("[WARN] settings.bin not found, using default patch URL");
    }

    // 3. Populate Client Map
    if (m_configData.contains("clients")) {
        for (auto& [id, cdata] : m_configData["clients"].items()) {
            GameClient client;
            client.id = id;
            client.name = cdata.value("name", id);
            client.installPath = NormalizeInstallPath(cdata.value("install_path", "./" + id));
            client.runPath = cdata.value("run_path", "");
            client.version = cdata.value("version", "0.0.0");
            client.classicLanguage = cdata.value("classic_language", "english");
            client.classicLanguageFollowsLauncher = cdata.value("classic_language_follows_launcher", true);
            client.classicUiVariant = cdata.value("classic_ui_variant", "classic_default");
            client.classicBorderless = cdata.value("classic_borderless", false);
            client.classicColorfulSystemMessages = cdata.value("classic_colorful_system_messages", false);
            client.interludeBorderless = cdata.value("interlude_borderless", false);
            client.interludeRightClickNoRotate = cdata.value("interlude_right_click_no_rotate", false);
            client.interludeColorfulSystemMessages = cdata.value("interlude_colorful_system_messages", false);
            client.interludeRightClickOption = cdata.value("interlude_right_click_option", "right_click_camera_no_rotate");
            client.interludeRightClickRestoreOption = cdata.value("interlude_right_click_restore_option", "right_click_camera_default");
            client.lastRunCheck = cdata.value("last_run_check", 0LL);
            m_clients[id] = client;
            Utils::Log("[INFO] configured client: " + id + " installPath=" + client.installPath);
        }
    }

    // Default clients if none exist
    if (m_clients.empty()) {
        GameClient classic;
        classic.id = "classic";
        classic.name = "Classic Client";
        classic.installPath = NormalizeInstallPath("./classic");
        classic.version = "0.0.0";
        classic.classicLanguage = "english";
        classic.classicLanguageFollowsLauncher = true;
        classic.classicUiVariant = "classic_default";
        classic.classicBorderless = false;
        classic.classicColorfulSystemMessages = false;
        m_clients["classic"] = classic;

        GameClient interlude;
        interlude.id = "interlude";
        interlude.name = "Interlude Client";
        interlude.installPath = NormalizeInstallPath("./interlude");
        interlude.version = "0.0.0";
        interlude.interludeRightClickOption = "right_click_camera_no_rotate";
        interlude.interludeRightClickRestoreOption = "right_click_camera_default";
        m_clients["interlude"] = interlude;
        Utils::Log("[INFO] no client config found, using default classic/interlude entries");
    }
}

void Config::Save() {
    nlohmann::json clientsJson;
    for (auto const& [id, client] : m_clients) {
        clientsJson[id] = {
            {"name", client.name},
            {"install_path", client.installPath},
            {"run_path", client.runPath},
            {"version", client.version},
            {"classic_language", client.classicLanguage},
            {"classic_language_follows_launcher", client.classicLanguageFollowsLauncher},
            {"classic_ui_variant", client.classicUiVariant},
            {"classic_borderless", client.classicBorderless},
            {"classic_colorful_system_messages", client.classicColorfulSystemMessages},
            {"interlude_borderless", client.interludeBorderless},
            {"interlude_right_click_no_rotate", client.interludeRightClickNoRotate},
            {"interlude_colorful_system_messages", client.interludeColorfulSystemMessages},
            {"interlude_right_click_option", client.interludeRightClickOption},
            {"interlude_right_click_restore_option", client.interludeRightClickRestoreOption},
            {"last_run_check", client.lastRunCheck}
        };
    }
    m_configData["clients"] = clientsJson;

    std::ofstream file(m_configPath);
    if (file.is_open()) {
        file << m_configData.dump(4);
    }
}

std::string Config::GetLanguage() const {
    return m_configData.value("language", "en");
}

void Config::SetLanguage(const std::string& lang) {
    m_configData["language"] = lang;
    Save();
}

std::map<std::string, GameClient>& Config::GetClients() {
    return m_clients;
}

GameClient* Config::GetClient(const std::string& id) {
    if (m_clients.count(id)) return &m_clients[id];
    return nullptr;
}

void Config::UpdateClient(const std::string& id, const GameClient& client) {
    m_clients[id] = client;
    Save();
}

void Config::SetClientInstallPath(const std::string& id, const std::string& path) {
    if (m_clients.count(id)) {
        m_clients[id].installPath = NormalizeInstallPath(path);
        Save();
    }
}

void Config::UninstallClient(const std::string& id) {
    if (m_clients.count(id)) {
        std::string path = m_clients[id].installPath;
        if (!path.empty() && std::filesystem::exists(path)) {
            try {
                std::filesystem::remove_all(path);
            } catch (const std::exception& e) {
                std::cerr << "Uninstall error: " << e.what() << std::endl;
            }
        }
        m_clients[id].version = "0.0.0";
        m_clients[id].state = ClientState::NOT_INSTALLED;
        m_clients[id].lastRunCheck = 0;
        Save();
    }
}

std::string Config::GetRemotePatchUrl() const {
    return m_remotePatchUrl;
}

const std::vector<std::string>& Config::GetRemotePatchUrls() const {
    return m_remotePatchUrls;
}

std::string Config::GetSocialUrl(const std::string& key) const {
    auto it = m_socialUrls.find(key);
    if (it != m_socialUrls.end()) {
        return it->second;
    }
    return "";
}

bool Config::IsRemoteDebugEnabled() const {
    return m_remoteDebugEnabled && !m_remoteDebugLogUrl.empty();
}

std::string Config::GetRemoteDebugLogUrl() const {
    return m_remoteDebugLogUrl;
}

std::string Config::GetLauncherUpdateRetryVersion() const {
    return m_configData.value("launcher_update_retry_version", "");
}

int Config::GetLauncherUpdateRetryCount() const {
    return m_configData.value("launcher_update_retry_count", 0);
}

void Config::SetLauncherUpdateRetryState(const std::string& version, int count) {
    m_configData["launcher_update_retry_version"] = version;
    m_configData["launcher_update_retry_count"] = count;
    Save();
}

void Config::ClearLauncherUpdateRetryState() {
    m_configData.erase("launcher_update_retry_version");
    m_configData.erase("launcher_update_retry_count");
    Save();
}
