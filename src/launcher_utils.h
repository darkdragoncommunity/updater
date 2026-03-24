#pragma once

#include <string>

namespace LauncherUtils {
    // Launch game and return true if successful
    bool LaunchGame(const std::string& executablePath);

    // Open a native folder picker and return the selected folder, or empty if cancelled
    std::string SelectFolder(const std::string& initialPath = "");

    // Open a URL using the operating system default handler
    bool OpenUrl(const std::string& url);

    // Launch the update helper to apply a downloaded launcher package, then terminate.
    bool StartSelfUpdateAndExit(const std::string& packagePath);
}
