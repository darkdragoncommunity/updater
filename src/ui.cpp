#include "ui.h"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "language.h"
#include "config.h"
#include "launcher_utils.h"
#include "texture_utils.h"
#include "utils.h"
#include <array>
#include <cctype>
#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <future>
#include <stb_image.h>

#ifndef LAUNCHER_VERSION
#define LAUNCHER_VERSION "0.0.0"
#endif

namespace {
    struct LanguageButton {
        const char* code;
        const char* assetPath;
    };

    struct SocialLink {
        const char* label;
        const char* key;
        const char* assetPath;
    };

    const std::array<LanguageButton, 4> kLanguageButtons = {{
        {"en", "assets/EN.png"},
        {"es", "assets/ES.png"},
        {"pt", "assets/PT.png"},
        {"ru", "assets/RU.png"}
    }};

    const std::array<SocialLink, 7> kSocialLinks = {{
        {"website", "website", "assets/logo.png"},
        {"discord", "discord", "assets/discord.png"},
        {"twitch", "twitch", "assets/twitch.png"},
        {"facebook", "facebook", "assets/facebook.png"},
        {"tiktok", "tiktok", "assets/tiktok.png"},
        {"youtube", "youtube", "assets/youtube.png"},
        {"instagram", "instagram", "assets/instagram.png"}
    }};

    std::string ToUpper(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return value;
    }

    std::string ToTitleCase(std::string value) {
        bool upperNext = true;
        for (char& c : value) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                upperNext = true;
                continue;
            }
            c = upperNext ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
                          : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            upperNext = false;
        }
        return value;
    }

    std::string LocalizedOrFallback(Language& lang, const std::string& key, const std::string& fallbackKey) {
        const std::string value = lang.GetString(key);
        if (value == key) {
            return lang.GetString(fallbackKey);
        }
        return value;
    }

    bool RenderIconOrTextButton(const char* id, unsigned int texture, const char* fallbackLabel, const ImVec2& size, bool enabled = true) {
        if (!enabled) {
            ImGui::BeginDisabled();
        }

        bool clicked = false;
        if (texture != 0) {
            clicked = ImGui::ImageButton(id, (void*)(intptr_t)texture, size);
        } else {
            clicked = ImGui::Button(fallbackLabel, size);
        }

        if (!enabled) {
            ImGui::EndDisabled();
        }

        return clicked && enabled;
    }

    void SetPointerOnHover() {
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
    }

    void RenderBadge(const std::string& id, const std::string& text, const ImVec4& bgColor, float rounding = 4.0f) {
        const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bgColor);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
        ImGui::BeginChild(id.c_str(), ImVec2(textSize.x + 20.0f, 30.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::SetCursorPos(ImVec2(10.0f, 6.0f));
        ImGui::TextUnformatted(text.c_str());
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void Spinner(const char* label, float radius, float thickness, const ImVec4& color) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImVec2 center = ImGui::GetCursorScreenPos();
        center.x += radius;
        center.y += radius;
        float time = (float)ImGui::GetTime();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const int num_segments = 30;
        const float start_angle = time * 6.0f;
        const float arc_angle = 1.5f * 3.1415926535f;

        drawList->PathClear();
        for (int i = 0; i <= num_segments; i++) {
            float a = start_angle + (i / (float)num_segments) * arc_angle;
            drawList->PathLineTo(ImVec2(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius));
        }
        drawList->PathStroke(ImGui::GetColorU32(color), false, thickness);

        ImGui::Dummy(ImVec2(radius * 2, radius * 2));
        ImGui::PopStyleColor();
    }

    void SetWindowIcon(GLFWwindow* window) {
        const std::array<const char*, 2> iconCandidates = {{
            "assets/app_icon.png",
            "assets/logo.png"
        }};

        for (const char* path : iconCandidates) {
            int width = 0;
            int height = 0;
            unsigned char* pixels = stbi_load(path, &width, &height, nullptr, 4);
            if (pixels == nullptr) {
                continue;
            }

            GLFWimage image{};
            image.width = width;
            image.height = height;
            image.pixels = pixels;
            glfwSetWindowIcon(window, 1, &image);
            stbi_image_free(pixels);
            return;
        }
    }

    void ConfigureFonts(ImGuiIO& io) {
        const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesCyrillic();
        const std::array<const char*, 2> fontCandidates = {{
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf"
        }};

        for (const char* path : fontCandidates) {
            if (std::filesystem::exists(path) && io.Fonts->AddFontFromFileTTF(path, 18.0f, nullptr, glyphRanges) != nullptr) {
                return;
            }
        }

        io.Fonts->AddFontDefault();
    }
}

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

LauncherUI::LauncherUI() : m_window(nullptr) {
    // Initialize Updaters for each client
    for (auto const& [id, client] : Config::GetInstance().GetClients()) {
        m_updaters[id] = std::make_unique<Updater>(id);
        GameClient* currentClient = Config::GetInstance().GetClient(id);
        if (!currentClient) continue;

        std::filesystem::path installPath(currentClient->installPath);
        if (!currentClient->installPath.empty() && std::filesystem::exists(installPath)) {
            currentClient->state = ClientState::INSTALLED;
        } else {
            currentClient->state = ClientState::NOT_INSTALLED;
        }
        m_updaters[id]->StartCheckOnly();
    }
}

LauncherUI::~LauncherUI() { Shutdown(); }

bool LauncherUI::Init() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return false;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    m_window = glfwCreateWindow(1000, 600, "Game Launcher", NULL, NULL);
    if (!m_window) return false;
    SetWindowIcon(m_window);
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ConfigureFonts(io);
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    m_bgTexture = TextureUtils::LoadTextureFromFile("assets/gludio.jpg");
    m_actionTextures["repair"] = TextureUtils::LoadTextureFromFile("assets/repair.png");
    m_actionTextures["settings"] = TextureUtils::LoadTextureFromFile("assets/settings.png");
    m_actionTextures["uninstall"] = TextureUtils::LoadTextureFromFile("assets/uninstall.png");
    for (const auto& languageButton : kLanguageButtons) {
        unsigned int texture = TextureUtils::LoadTextureFromFile(languageButton.assetPath);
        if (texture != 0) {
            m_languageTextures[languageButton.code] = texture;
        }
    }
    for (const auto& link : kSocialLinks) {
        int width = 0;
        int height = 0;
        unsigned int texture = TextureUtils::LoadTextureFromFile(link.assetPath, &width, &height);
        if (texture != 0) {
            m_socialTextures[link.key] = texture;
        }
    }

    return true;
}

void LauncherUI::Run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        RenderUI();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }
}

void LauncherUI::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (m_window) glfwDestroyWindow(m_window);
    glfwTerminate();
}

void LauncherUI::RenderUI() {
    Language& lang = Language::GetInstance();
    const ImVec4 panelBlue(0.09f, 0.24f, 0.56f, 0.62f);
    
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("Launcher", nullptr, window_flags);

    if (m_bgTexture != 0) {
        ImVec2 size = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddImage((void*)(intptr_t)m_bgTexture, ImVec2(0, 0), size);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    RenderBadge("LauncherTitleBadge", "Game Launcher", panelBlue);
    RenderLanguageSwitcher();

    ImGui::SetCursorPosY(60);

    // Render Client Blocks side-by-side
    auto& clients = Config::GetInstance().GetClients();
    float blockWidth = (ImGui::GetWindowWidth() - 40) / 2.0f;
    
    int count = 0;
    for (auto const& [id, client] : clients) {
        if (count > 0) ImGui::SameLine();
        ImGui::BeginChild(id.c_str(), ImVec2(blockWidth, 400), true, ImGuiWindowFlags_NoScrollbar);
        RenderClientBlock(id);
        ImGui::EndChild();
        count++;
    }

    constexpr float iconSize = 24.0f;
    constexpr float iconSpacing = 8.0f;
    const float totalIconsWidth = (iconSize * static_cast<float>(kSocialLinks.size())) + (iconSpacing * static_cast<float>(kSocialLinks.size() - 1));
    const float iconsStartX = (ImGui::GetWindowWidth() - totalIconsWidth) * 0.5f;

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 68.0f);
    ImGui::SetCursorPosX(iconsStartX);
    for (size_t i = 0; i < kSocialLinks.size(); ++i) {
        const auto& link = kSocialLinks[i];
        std::string url = Config::GetInstance().GetSocialUrl(link.key);
        unsigned int texture = m_socialTextures.count(link.key) ? m_socialTextures[link.key] : 0;
        bool enabled = !url.empty() && texture != 0;
        if (!enabled) {
            ImGui::BeginDisabled();
        }
        if (ImGui::ImageButton(link.key, (void*)(intptr_t)texture, ImVec2(iconSize, iconSize)) && enabled) {
            LauncherUtils::OpenUrl(url);
        }
        if (enabled && ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            std::string tooltip = ToTitleCase(link.label);
            ImGui::SetTooltip("%s", tooltip.c_str());
        }
        if (!enabled) {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Set the URL in settings.bin and make sure the PNG exists in assets");
            }
        }
        if (i + 1 < kSocialLinks.size()) {
            ImGui::SameLine(0.0f, iconSpacing);
        }
    }

    const std::string versionLabel = std::string("v") + LAUNCHER_VERSION;
    const ImVec2 versionSize = ImGui::CalcTextSize(versionLabel.c_str());
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - versionSize.x - 22.0f, ImGui::GetWindowHeight() - 34.0f));
    ImGui::TextDisabled("%s", versionLabel.c_str());

    ImGui::PopStyleColor();
    ImGui::End();
}

void LauncherUI::RenderLanguageSwitcher() {
    Language& lang = Language::GetInstance();
    const std::string currentLang = Config::GetInstance().GetLanguage();
    const ImVec2 buttonSize(24.0f, 16.0f);
    const float spacing = 5.0f;
    const float totalWidth = (buttonSize.x * static_cast<float>(kLanguageButtons.size())) + (spacing * static_cast<float>(kLanguageButtons.size() - 1));

    ImGui::SameLine(ImGui::GetWindowWidth() - totalWidth - 56.0f);
    for (size_t i = 0; i < kLanguageButtons.size(); ++i) {
        const auto& languageButton = kLanguageButtons[i];
        const bool isSelected = (currentLang == languageButton.code);
        const bool isAvailable = std::find(lang.GetAvailableLanguages().begin(), lang.GetAvailableLanguages().end(), languageButton.code) != lang.GetAvailableLanguages().end();
        const unsigned int texture = m_languageTextures.count(languageButton.code) ? m_languageTextures[languageButton.code] : 0;

        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.18f, 0.12f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.82f, 0.24f, 0.16f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.64f, 0.12f, 0.08f, 1.0f));
        }

        if (RenderIconOrTextButton(("lang##" + std::string(languageButton.code)).c_str(), texture, ToUpper(languageButton.code).c_str(), buttonSize, isAvailable)) {
            Language::GetInstance().SwitchLanguage(languageButton.code);
            Config::GetInstance().SetLanguage(languageButton.code);

            GameClient* classicClient = Config::GetInstance().GetClient("classic");
            if (classicClient) {
                if (classicClient->classicLanguageFollowsLauncher) {
                    if (languageButton.code == "en") {
                        classicClient->classicLanguage = "english";
                    } else if (languageButton.code == "es") {
                        classicClient->classicLanguage = "spanish";
                    } else if (languageButton.code == "pt") {
                        classicClient->classicLanguage = "portuguese";
                    } else if (languageButton.code == "ru") {
                        classicClient->classicLanguage = "russian";
                    }
                }
                Config::GetInstance().UpdateClient("classic", *classicClient);
            }
        }

        if (isSelected) {
            ImGui::PopStyleColor(3);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        if (i + 1 < kLanguageButtons.size()) {
            ImGui::SameLine(0.0f, spacing);
        }
    }
}

void LauncherUI::RenderClientBlock(const std::string& clientId) {
    Language& lang = Language::GetInstance();
    GameClient* client = Config::GetInstance().GetClient(clientId);
    Updater* updater = m_updaters[clientId].get();
    
    if (!client || !updater) return;

    const ImVec4 panelBlue(0.09f, 0.24f, 0.56f, 0.62f);
    const ImVec4 panelBlueHover(0.12f, 0.29f, 0.64f, 0.72f);
    const ImVec4 panelBlueActive(0.07f, 0.20f, 0.48f, 0.78f);
    const float controlRounding = 4.0f;

    RenderBadge("ClientTitleBadge##" + clientId, client->name, panelBlue, controlRounding);
    RenderClientActions(clientId, client, updater);
    RenderClientSettingsPopup(clientId, client, updater);

    ImGui::Separator();
    
    // Path Selection
    ImGui::Text("%s", lang.GetString("path_label").c_str());
    char pathBuf[512];
    #if defined(_MSC_VER)
    strncpy_s(pathBuf, sizeof(pathBuf), client->installPath.c_str(), _TRUNCATE);
#else
    strncpy(pathBuf, client->installPath.c_str(), sizeof(pathBuf) - 1);
    pathBuf[sizeof(pathBuf) - 1] = '\0';
#endif
    
    bool isInstalled = (client->state != ClientState::NOT_INSTALLED);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_ReadOnly;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, controlRounding);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, panelBlue);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, panelBlueHover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, panelBlueActive);
    constexpr float browseButtonWidth = 34.0f;
    constexpr float pathRightMargin = 10.0f;
    ImGui::PushItemWidth(-(browseButtonWidth + pathRightMargin));
    ImGui::InputText(("##Path" + clientId).c_str(), pathBuf, sizeof(pathBuf), flags);
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, panelBlue);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, panelBlueHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, panelBlueActive);
    if (ImGui::Button(("...##Browse" + clientId).c_str(), ImVec2(browseButtonWidth, 0.0f)) && !isInstalled) {
        std::string selectedFolder = LauncherUtils::SelectFolder(client->installPath);
        if (!selectedFolder.empty()) {
            Config::GetInstance().SetClientInstallPath(clientId, selectedFolder);
        }
    }
    SetPointerOnHover();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    
    UpdateStatus status = updater->GetStatus();
    std::string statusText = lang.GetString("status_label") + ": " + updater->GetStatusText();
    RenderBadge("StatusBadge##" + clientId, statusText, panelBlue, controlRounding);
    
    if (status == UpdateStatus::DOWNLOADING || status == UpdateStatus::VERIFYING) {
        if (status == UpdateStatus::DOWNLOADING) {
            ImGui::Text("%s: %s", lang.GetString("file_label").c_str(), updater->GetCurrentFileName().c_str());
        } else {
            ImGui::Text("%s: %s", lang.GetString("file_label").c_str(), lang.GetString("verifying_local_files").c_str());
        }
        char progressText[32];
        std::snprintf(progressText, sizeof(progressText), "%.1f%%", updater->GetProgress() * 100.0);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.12f, 0.28f, 0.60f));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, panelBlueHover);
        ImGui::PushStyleColor(ImGuiCol_Border, panelBlueActive);
        ImGui::ProgressBar((float)updater->GetProgress(), ImVec2(-1, 25), "");
        const ImVec2 barMin = ImGui::GetItemRectMin();
        const ImVec2 barMax = ImGui::GetItemRectMax();
        const ImVec2 textSize = ImGui::CalcTextSize(progressText);
        const ImVec2 textPos(
            barMin.x + ((barMax.x - barMin.x) - textSize.x) * 0.5f,
            barMin.y + ((barMax.y - barMin.y) - textSize.y) * 0.5f
        );
        ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(255, 255, 255, 255), progressText);
        ImGui::PopStyleColor(3);
    } else {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 25); // Placeholder for progress bar
    }

    ImGui::SetCursorPosY(350);
    
    bool canPlay = (client->state == ClientState::READY || client->state == ClientState::INSTALLED);
    bool isUpdateAvailable = (client->state == ClientState::UPDATE_AVAILABLE);
    bool isVerifying = (status == UpdateStatus::VERIFYING);
    bool isUpdating = (status == UpdateStatus::DOWNLOADING);
    bool isChecking = (status == UpdateStatus::CHECKING_FILES || status == UpdateStatus::CHECKING_LAUNCHER);
    bool isBusy = isUpdating || isChecking || isVerifying;
    const auto nowSteady = std::chrono::steady_clock::now();
    const bool primaryCooldownActive =
        (m_playCooldownUntil.count(clientId) > 0) &&
        (nowSteady < m_playCooldownUntil[clientId]);
    const bool disablePrimaryAction = primaryCooldownActive && !isBusy;

    std::string actionText;
    if (client->state == ClientState::NOT_INSTALLED) {
        actionText = ToUpper(lang.GetString("install"));
    } else if (isUpdateAvailable) {
        actionText = ToUpper(lang.GetString("update"));
    } else {
        actionText = ToUpper(lang.GetString("play"));
    }

    if (isBusy) {
        std::string busyLabel = ToUpper(lang.GetString("stop"));
        if (isVerifying) {
            busyLabel = ToUpper(LocalizedOrFallback(lang, "verify_short", "verifying")) + " / " + ToUpper(lang.GetString("stop"));
        } else if (isUpdating) {
            busyLabel = ToUpper(LocalizedOrFallback(lang, "download_short", "downloading")) + " / " + ToUpper(lang.GetString("stop"));
        } else if (isChecking) {
            busyLabel = ToUpper(LocalizedOrFallback(lang, "check_short", "checking")) + " / " + ToUpper(lang.GetString("stop"));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, controlRounding);
        ImGui::PushStyleColor(ImGuiCol_Button, panelBlue);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, panelBlueHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, panelBlueActive);
        if (ImGui::Button((busyLabel + "##" + clientId).c_str(), ImVec2(140, 40))) {
            updater->Stop();
        }
        SetPointerOnHover();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    } else {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, controlRounding);

        ImVec4 currentBtnColor = panelBlue;
        ImVec4 currentBtnHoverColor = panelBlueHover;
        ImVec4 currentBtnActiveColor = panelBlueActive;

        if (isUpdateAvailable) {
            // Distinct color for Update (e.g., Green/Emerald)
            currentBtnColor = ImVec4(0.13f, 0.55f, 0.13f, 0.85f);
            currentBtnHoverColor = ImVec4(0.18f, 0.65f, 0.18f, 1.0f);
            currentBtnActiveColor = ImVec4(0.09f, 0.45f, 0.09f, 1.0f);
        }

        ImGui::PushStyleColor(ImGuiCol_Button, currentBtnColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, currentBtnHoverColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, currentBtnActiveColor);
        
        if (disablePrimaryAction) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button((actionText + "##" + clientId).c_str(), ImVec2(100, 40))) {
            m_playCooldownUntil[clientId] = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            if (client->state == ClientState::NOT_INSTALLED || isUpdateAvailable) {
                updater->Start();
            } else if (canPlay && !client->runPath.empty()) {
                long long now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                
                bool updateFound = false;
                // If more than 5 minutes passed (300 seconds)
                if (now - client->lastRunCheck > 300) {
                    if (updater->CheckForUpdatesSync()) {
                        updateFound = true;
                        updater->Start(); // Start updating
                    } else {
                        client->lastRunCheck = now;
                        Config::GetInstance().Save();
                    }
                }

                if (!updateFound) {
                    std::string fullRunPath = client->installPath + "/" + client->runPath;
                    Utils::Log("[INFO] play clicked client=" + clientId + " installPath=" + client->installPath + " runPath=" + client->runPath + " fullRunPath=" + fullRunPath);
                    const bool launched = LauncherUtils::LaunchGame(fullRunPath);
                    if (!launched) {
                        Utils::Log("[ERROR] play failed client=" + clientId + " fullRunPath=" + fullRunPath);
                    }
                    client->lastRunCheck = now;
                    Config::GetInstance().Save();
                }
            }
        }
        if (disablePrimaryAction) {
            ImGui::EndDisabled();
        }
        SetPointerOnHover();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }
}

void LauncherUI::RenderClientActions(const std::string& clientId, GameClient* client, Updater* updater) {
    Language& lang = Language::GetInstance();
    const float iconSize = 20.0f;
    const ImVec2 buttonSize(iconSize, iconSize);
    const float totalWidth = (iconSize * 3.0f) + 12.0f;
    const ImVec4 panelBlue(0.09f, 0.24f, 0.56f, 0.62f);
    const ImVec4 panelBlueHover(0.12f, 0.29f, 0.64f, 0.72f);
    const ImVec4 panelBlueActive(0.07f, 0.20f, 0.48f, 0.78f);
    const float controlRounding = 4.0f;
    ImGui::SameLine(ImGui::GetWindowWidth() - totalWidth - 54.0f);

    bool canMutateFiles = (client->state != ClientState::NOT_INSTALLED);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, controlRounding);
    ImGui::PushStyleColor(ImGuiCol_Button, panelBlue);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, panelBlueHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, panelBlueActive);
    if (RenderIconOrTextButton(("repair##" + clientId).c_str(), m_actionTextures["repair"], "R", buttonSize, canMutateFiles)) {
        Utils::Log("[INFO] repair requested client=" + clientId);
        updater->ForceRepair();
        m_clientFeedback[clientId] = lang.GetString("repair_queued");
    }
    SetPointerOnHover();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", lang.GetString("repair").c_str());
    }
    ImGui::SameLine();

    if (RenderIconOrTextButton(("settings##" + clientId).c_str(), m_actionTextures["settings"], "S", buttonSize, canMutateFiles)) {
        ImGui::OpenPopup((lang.GetString("client_settings") + "##" + clientId).c_str());
    }
    SetPointerOnHover();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", lang.GetString("settings").c_str());
    }
    ImGui::SameLine();

    if (RenderIconOrTextButton(("uninstall##" + clientId).c_str(), m_actionTextures["uninstall"], "U", buttonSize, canMutateFiles)) {
        ImGui::OpenPopup((lang.GetString("confirm_uninstall_title") + "##" + clientId).c_str());
    }
    SetPointerOnHover();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", lang.GetString("uninstall").c_str());
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();

    if (ImGui::BeginPopupModal((lang.GetString("confirm_uninstall_title") + "##" + clientId).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%s", lang.GetString("confirm_uninstall").c_str());
        ImGui::Spacing();
        std::string deleteLabel = ToUpper(lang.GetString("delete"));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, controlRounding);
        ImGui::PushStyleColor(ImGuiCol_Button, panelBlue);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, panelBlueHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, panelBlueActive);
        if (ImGui::Button(deleteLabel.c_str(), ImVec2(100, 0))) {
            Config::GetInstance().UninstallClient(clientId);
            ImGui::CloseCurrentPopup();
        }
        SetPointerOnHover();
        ImGui::SameLine();
        std::string cancelLabel = ToUpper(lang.GetString("cancel"));
        if (ImGui::Button(cancelLabel.c_str(), ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        SetPointerOnHover();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
}

void LauncherUI::RenderClientSettingsPopup(const std::string& clientId, GameClient* client, Updater* updater) {
    Language& lang = Language::GetInstance();

    bool popupOpen = true;
    if (!ImGui::BeginPopupModal((lang.GetString("client_settings") + "##" + clientId).c_str(), &popupOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    auto closeIt = m_clientSettingsCloseAt.find(clientId);
    if (closeIt != m_clientSettingsCloseAt.end()) {
        if (std::chrono::steady_clock::now() >= closeIt->second) {
            m_clientSettingsCloseAt.erase(closeIt);
            ImGui::CloseCurrentPopup();
        }
    }

    if (!popupOpen) {
        Config::GetInstance().UpdateClient(clientId, *client);
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    ImGui::Text("%s", lang.GetString("client_settings").c_str());
    ImGui::Separator();

    bool changed = false;

    if (clientId == "classic") {
        const std::array<const char*, 5> classicOptions = {"english", "portuguese", "russian", "spanish", "chinese"};
        const std::array<const char*, 4> classicUiOptions = {
            "classic_default",
            "classic_all_buffs_only",
            "classic_noblesse_only",
            "classic_all_buffs_and_noblesse"
        };
        int selectedIndex = 0;
        for (int i = 0; i < static_cast<int>(classicOptions.size()); ++i) {
            if (client->classicLanguage == classicOptions[i]) {
                selectedIndex = i;
                break;
            }
        }
        int selectedUiIndex = 0;
        for (int i = 0; i < static_cast<int>(classicUiOptions.size()); ++i) {
            if (client->classicUiVariant == classicUiOptions[i]) {
                selectedUiIndex = i;
                break;
            }
        }

        ImGui::Text("%s", lang.GetString("game_language_label").c_str());
        if (ImGui::BeginCombo(("##ClassicLanguage" + clientId).c_str(), lang.GetString(classicOptions[selectedIndex]).c_str())) {
            for (int i = 0; i < static_cast<int>(classicOptions.size()); ++i) {
                bool isSelected = (selectedIndex == i);
                if (ImGui::Selectable(lang.GetString(classicOptions[i]).c_str(), isSelected)) {
                    client->classicLanguage = classicOptions[i];
                    client->classicLanguageFollowsLauncher = false;
                    changed = true;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Text("%s", lang.GetString("classic_ui_customization").c_str());
        if (ImGui::BeginCombo(("##ClassicUiVariant" + clientId).c_str(), lang.GetString(classicUiOptions[selectedUiIndex]).c_str())) {
            for (int i = 0; i < static_cast<int>(classicUiOptions.size()); ++i) {
                bool isSelected = (selectedUiIndex == i);
                if (ImGui::Selectable(lang.GetString(classicUiOptions[i]).c_str(), isSelected)) {
                    client->classicUiVariant = classicUiOptions[i];
                    changed = true;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        bool borderless = client->classicBorderless;
        if (ImGui::Checkbox(lang.GetString("borderless_window").c_str(), &borderless)) {
            client->classicBorderless = borderless;
            changed = true;
        }

        bool colorfulSystemMessages = client->classicColorfulSystemMessages;
        if (ImGui::Checkbox(lang.GetString("colorful_system_messages").c_str(), &colorfulSystemMessages)) {
            client->classicColorfulSystemMessages = colorfulSystemMessages;
            changed = true;
        }
    } else if (clientId == "interlude") {
        ImGui::Text("%s", lang.GetString("game_language_label").c_str());
        ImGui::BeginDisabled();
        if (ImGui::BeginCombo(("##InterludeLanguage" + clientId).c_str(), lang.GetString("english").c_str())) {
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        bool borderless = client->interludeBorderless;
        if (ImGui::Checkbox(lang.GetString("borderless_window").c_str(), &borderless)) {
            client->interludeBorderless = borderless;
            changed = true;
        }

        bool noRotate = client->interludeRightClickNoRotate;
        if (ImGui::Checkbox(lang.GetString("right_click_camera_no_rotate").c_str(), &noRotate)) {
            client->interludeRightClickNoRotate = noRotate;
            changed = true;
        }

        bool colorfulSystemMessages = client->interludeColorfulSystemMessages;
        if (ImGui::Checkbox(lang.GetString("colorful_system_messages").c_str(), &colorfulSystemMessages)) {
            client->interludeColorfulSystemMessages = colorfulSystemMessages;
            changed = true;
        }
    } else {
        ImGui::Text("%s", lang.GetString("no_custom_settings").c_str());
    }

    ImGui::Spacing();
    auto jobIt = m_clientApplyJobs.find(clientId);
    bool isApplying = false;
    if (jobIt != m_clientApplyJobs.end()) {
        if (jobIt->second.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            bool success = jobIt->second.get();
            m_clientApplyJobs.erase(jobIt);
            Utils::Log("[INFO] apply settings client=" + clientId + " success=" + std::string(success ? "true" : "false"));
            Config::GetInstance().UpdateClient(clientId, *client);
            m_clientFeedback[clientId] = success ? lang.GetString("apply_success") : lang.GetString("apply_failed");
            if (success) {
                m_clientSettingsCloseAt[clientId] = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            }
        } else {
            isApplying = true;
        }
    }

    if (isApplying) {
        ImGui::Text("%s", lang.GetString("apply_in_progress").c_str());
        ImGui::SameLine();
        std::string spinnerId = "apply_spinner_" + clientId;
        Spinner(spinnerId.c_str(), 10.0f, 2.5f, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
    } else {
        std::string applyLabel = ToUpper(lang.GetString("apply"));
        if (ImGui::Button(applyLabel.c_str(), ImVec2(100, 0))) {
            m_clientFeedback[clientId] = lang.GetString("apply_in_progress");
            GameClient snapshot = *client;
            auto task = std::async(std::launch::async, [this, clientId, snapshot, updater]() {
                return PerformClientApply(clientId, snapshot, updater);
            });
            m_clientApplyJobs.emplace(clientId, std::move(task));
        }
    }
    ImGui::SameLine();
    std::string closeLabel = ToUpper(lang.GetString("close"));
    if (ImGui::Button(closeLabel.c_str(), ImVec2(100, 0))) {
        Config::GetInstance().UpdateClient(clientId, *client);
        ImGui::CloseCurrentPopup();
    }

    const std::string& feedback = m_clientFeedback[clientId];
    if (!feedback.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%s", feedback.c_str());
    }

    ImGui::EndPopup();
}

bool LauncherUI::PerformClientApply(const std::string& clientId, const GameClient& snapshot, Updater* updater) {
    bool success = true;

    auto applyIfPresent = [&](const std::string& key) {
        if (updater->GetOptions().find(key) != updater->GetOptions().end()) {
            return updater->ApplyOption(key);
        }
        Utils::Log("[INFO] option not present in manifest, skipping: " + key);
        return true;
    };

    auto applyToggle = [&](bool enabled, const std::string& enableKey, const std::string& disableKey) {
        if (enabled) {
            return applyIfPresent(enableKey);
        } else if (!disableKey.empty()) {
            return applyIfPresent(disableKey);
        }
        return true;
    };

    if (clientId == "classic") {
        success = updater->ApplyOption(snapshot.classicLanguage);
        success = success && updater->ApplyOption(snapshot.classicUiVariant);
        success = success && applyToggle(snapshot.classicBorderless, "borderless", "borderless_default");
        success = success && applyToggle(snapshot.classicColorfulSystemMessages, "colorful_system_messages", "colorful_system_messages_default");
    } else if (clientId == "interlude") {
        success = success && applyToggle(snapshot.interludeBorderless, "borderless", "borderless_default");
        success = success && applyToggle(snapshot.interludeRightClickNoRotate, snapshot.interludeRightClickOption, snapshot.interludeRightClickRestoreOption);
        success = success && applyToggle(snapshot.interludeColorfulSystemMessages, "colorful_system_messages", "colorful_system_messages_default");
    }
    return success;
}
