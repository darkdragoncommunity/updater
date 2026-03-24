#pragma once

#include "config.h"
#include "updater.h"
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <chrono>
#include <future>

struct GLFWwindow;

class LauncherUI {
public:
    LauncherUI();
    ~LauncherUI();

    bool Init();
    void Run();
    void Shutdown();

private:
    void RenderUI();
    void RenderClientBlock(const std::string& clientId);
    void RenderClientActions(const std::string& clientId, GameClient* client, Updater* updater);
    void RenderClientSettingsPopup(const std::string& clientId, GameClient* client, Updater* updater);
    void RenderLanguageSwitcher();
    bool PerformClientApply(const std::string& clientId, const GameClient& snapshot, Updater* updater);
    
    std::map<std::string, std::unique_ptr<Updater>> m_updaters;
    GLFWwindow* m_window;
    
    // UI state
    int m_selectedLanguage = 0;
    
    // UI textures
    unsigned int m_bgTexture = 0;
    std::map<std::string, unsigned int> m_socialTextures;
    std::map<std::string, unsigned int> m_actionTextures;
    std::map<std::string, unsigned int> m_languageTextures;
    std::map<std::string, std::chrono::steady_clock::time_point> m_playCooldownUntil;
    std::map<std::string, std::string> m_clientFeedback;
    std::map<std::string, std::future<bool>> m_clientApplyJobs;
    std::map<std::string, std::chrono::steady_clock::time_point> m_clientSettingsCloseAt;
};
