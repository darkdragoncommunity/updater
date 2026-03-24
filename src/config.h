#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include <map>
#include <vector>

enum class ClientState {
    NOT_INSTALLED,
    INSTALLED,
    UPDATE_AVAILABLE,
    UPDATING,
    READY,
    STATE_ERROR
};

struct GameClient {
    std::string id;
    std::string name;
    std::string installPath;
    std::string runPath;
    std::string version;
    std::string classicLanguage = "english";
    bool classicLanguageFollowsLauncher = true;
    std::string classicUiVariant = "classic_default";
    bool classicBorderless = false;
    bool classicColorfulSystemMessages = false;
    bool interludeBorderless = false;
    bool interludeRightClickNoRotate = false;
    bool interludeColorfulSystemMessages = false;
    std::string interludeRightClickOption = "right_click_camera_no_rotate";
    std::string interludeRightClickRestoreOption = "right_click_camera_default";
    long long lastRunCheck = 0;
    ClientState state = ClientState::NOT_INSTALLED;
};

class Config {
public:
    static Config& GetInstance();

    void Load();
    void Save();

    std::string GetLanguage() const;
    void SetLanguage(const std::string& lang);

    std::map<std::string, GameClient>& GetClients();
    GameClient* GetClient(const std::string& id);
    void UpdateClient(const std::string& id, const GameClient& client);
    void SetClientInstallPath(const std::string& id, const std::string& path);
    void UninstallClient(const std::string& id);

    std::string GetRemotePatchUrl() const;
    const std::vector<std::string>& GetRemotePatchUrls() const;
    std::string GetSocialUrl(const std::string& key) const;
    bool IsRemoteDebugEnabled() const;
    std::string GetRemoteDebugLogUrl() const;
    std::string GetLauncherUpdateRetryVersion() const;
    int GetLauncherUpdateRetryCount() const;
    void SetLauncherUpdateRetryState(const std::string& version, int count);
    void ClearLauncherUpdateRetryState();

private:
    Config() = default;
    ~Config() = default;

    nlohmann::json m_configData;
    std::map<std::string, GameClient> m_clients;
    const std::string m_configPath = "config.json";
    const std::string m_settingsPath = "settings.bin";
    
    // The key can be injected at compile-time via -DLAUNCHER_ENCRYPTION_KEY
#ifndef LAUNCHER_ENCRYPTION_KEY
#define LAUNCHER_ENCRYPTION_KEY "CHANGE_ME_DEFAULT_KEY_2026"
#endif
    const std::string m_encryptionKey = LAUNCHER_ENCRYPTION_KEY;

    std::string m_remotePatchUrl;
    std::vector<std::string> m_remotePatchUrls;
    std::map<std::string, std::string> m_socialUrls;
    bool m_remoteDebugEnabled = false;
    std::string m_remoteDebugLogUrl;
};
