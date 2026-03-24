#include "language.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

Language& Language::GetInstance() {
    static Language instance;
    return instance;
}

#include "config.h"
#include "downloader.h"
#include <algorithm>

Language::Language() {
    // Scan for available languages
    if (std::filesystem::exists("languages")) {
        for (const auto& entry : std::filesystem::directory_iterator("languages")) {
            if (entry.path().extension() == ".json") {
                m_availableLanguages.push_back(entry.path().stem().string());
            }
        }
    }
    
    // Ensure standard ones are in the list if not found
    if (std::find(m_availableLanguages.begin(), m_availableLanguages.end(), "en") == m_availableLanguages.end()) m_availableLanguages.push_back("en");
    if (std::find(m_availableLanguages.begin(), m_availableLanguages.end(), "es") == m_availableLanguages.end()) m_availableLanguages.push_back("es");
    if (std::find(m_availableLanguages.begin(), m_availableLanguages.end(), "pt") == m_availableLanguages.end()) m_availableLanguages.push_back("pt");
    if (std::find(m_availableLanguages.begin(), m_availableLanguages.end(), "ru") == m_availableLanguages.end()) m_availableLanguages.push_back("ru");
    std::sort(m_availableLanguages.begin(), m_availableLanguages.end());
}

void Language::SetLanguage(const std::string& lang) {
    if (lang == m_currentLanguage) return;
    LoadLanguageFile(lang);
    m_currentLanguage = lang;
}

void Language::SwitchLanguage(const std::string& lang, bool downloadIfMissing) {
    if (lang == m_currentLanguage) return;

    std::string path = "languages/" + lang + ".json";
    if (!std::filesystem::exists(path) && downloadIfMissing) {
        Downloader downloader;

        if (!std::filesystem::exists("languages")) {
            std::filesystem::create_directories("languages");
        }

        for (const auto& baseUrl : Config::GetInstance().GetRemotePatchUrls()) {
            std::string url = baseUrl + "/languages/" + lang + ".json";
            if (downloader.DownloadFile(url, path)) {
                if (std::find(m_availableLanguages.begin(), m_availableLanguages.end(), lang) == m_availableLanguages.end()) {
                    m_availableLanguages.push_back(lang);
                }
                break;
            }
        }
    }

    SetLanguage(lang);
}

void Language::LoadLanguageFile(const std::string& lang) {
    m_strings.clear();

    auto loadFile = [this](const std::string& fileLang) {
        std::string path = "languages/" + fileLang + ".json";
        std::ifstream file(path);
        if (!file.is_open()) {
            return;
        }

        try {
            nlohmann::json json;
            file >> json;
            for (auto& [key, value] : json.items()) {
                m_strings[key] = value;
            }
        } catch (...) {
            std::cerr << "Failed to parse language file: " << path << std::endl;
        }
    };

    loadFile("en");
    if (lang != "en") {
        loadFile(lang);
    }
}

std::string Language::GetString(const std::string& key) {
    if (m_strings.count(key)) {
        return m_strings[key];
    }
    return key;
}

const std::vector<std::string>& Language::GetAvailableLanguages() const {
    return m_availableLanguages;
}
