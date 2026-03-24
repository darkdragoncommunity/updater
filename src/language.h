#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

class Language {
public:
    static Language& GetInstance();
    void SetLanguage(const std::string& lang);
    void SwitchLanguage(const std::string& lang, bool downloadIfMissing = true);
    std::string GetString(const std::string& key);
    std::string GetCurrentLanguage() const { return m_currentLanguage; }

    const std::vector<std::string>& GetAvailableLanguages() const;

private:
    Language();
    ~Language() = default;

    std::map<std::string, std::string> m_strings;
    std::vector<std::string> m_availableLanguages;
    std::string m_currentLanguage;

    void LoadLanguageFile(const std::string& lang);
};
