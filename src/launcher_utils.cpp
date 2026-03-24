#include "launcher_utils.h"
#include "utils.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <iostream>
#include <vector>

namespace LauncherUtils {
    namespace {
        std::string QuoteArgument(const std::string& value) {
            return "\"" + value + "\"";
        }

        std::string GetCurrentExecutablePath() {
            char modulePath[MAX_PATH] = {};
            DWORD length = GetModuleFileNameA(NULL, modulePath, MAX_PATH);
            if (length == 0 || length >= MAX_PATH) {
                return "";
            }
            return std::filesystem::path(modulePath).lexically_normal().string();
        }
    }

    bool LaunchGame(const std::string& executablePath) {
        const std::filesystem::path gamePath = std::filesystem::path(executablePath).lexically_normal();
        const std::filesystem::path workingDirectory = gamePath.parent_path();
        const std::string normalizedPath = gamePath.string();

        Utils::Log("[INFO] launch requested path=" + normalizedPath + " workingDir=" + workingDirectory.string());
        if (!std::filesystem::exists(gamePath)) {
            Utils::Log("[ERROR] launch target does not exist: " + normalizedPath);
            return false;
        }

        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        std::string commandLine = QuoteArgument(normalizedPath);
        std::vector<char> mutableCommandLine(commandLine.begin(), commandLine.end());
        mutableCommandLine.push_back('\0');

        si.cb = sizeof(si);

        if (!CreateProcessA(
            normalizedPath.c_str(),
            mutableCommandLine.data(),
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            workingDirectory.empty() ? NULL : workingDirectory.string().c_str(),
            &si,
            &pi
        )) {
            const DWORD errorCode = GetLastError();
            if (errorCode == ERROR_ELEVATION_REQUIRED) {
                Utils::Log("[WARN] CreateProcess requires elevation, retrying with ShellExecute runas path=" + normalizedPath);
                auto result = reinterpret_cast<intptr_t>(ShellExecuteA(
                    NULL,
                    "runas",
                    normalizedPath.c_str(),
                    NULL,
                    workingDirectory.empty() ? NULL : workingDirectory.string().c_str(),
                    SW_SHOWNORMAL));
                if (result > 32) {
                    Utils::Log("[INFO] elevated launch succeeded path=" + normalizedPath);
                    return true;
                }
                Utils::Log("[ERROR] elevated launch failed code=" + std::to_string(static_cast<long long>(result)) + " path=" + normalizedPath);
            }
            Utils::Log("[ERROR] CreateProcess failed code=" + std::to_string(errorCode) + " path=" + normalizedPath);
            std::cerr << "CreateProcess failed (" << errorCode << ")." << std::endl;
            return false;
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        Utils::Log("[INFO] launch succeeded path=" + normalizedPath);
        return true;
    }

    bool OpenUrl(const std::string& url) {
        auto result = reinterpret_cast<intptr_t>(ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL));
        if (result <= 32) {
            Utils::Log("[ERROR] failed to open URL: " + url);
            return false;
        }
        Utils::Log("[INFO] opened URL: " + url);
        return true;
    }

    std::string SelectFolder(const std::string& initialPath) {
        BROWSEINFO bi = {};
        bi.lpszTitle = "Select install folder";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI | BIF_VALIDATE;

        PIDLIST_ABSOLUTE pidl = SHBrowseForFolder(&bi);
        if (!pidl) {
            return "";
        }

        char selectedPath[MAX_PATH];
        std::string result;
        if (SHGetPathFromIDListA(pidl, selectedPath)) {
            result = std::filesystem::path(selectedPath).lexically_normal().string();
            Utils::Log("[INFO] selected install folder: " + result);
        }

        CoTaskMemFree(pidl);
        return result;
    }

    bool StartSelfUpdateAndExit(const std::string& packagePath) {
        const std::string currentExe = GetCurrentExecutablePath();
        if (currentExe.empty()) {
            Utils::Log("[ERROR] failed to resolve current executable path for self-update");
            return false;
        }
        Utils::Log("[INFO] self-update current exe=" + currentExe);
        Utils::Log("[INFO] self-update package=" + packagePath);

        const std::filesystem::path helperPath =
            std::filesystem::path(currentExe).parent_path() / "helper" / "autoupdate.exe";
        Utils::Log("[INFO] self-update helper path=" + helperPath.string());

        if (!std::filesystem::exists(helperPath)) {
            Utils::Log("[ERROR] self-update helper not found: " + helperPath.string());
            return false;
        }
        if (!std::filesystem::exists(packagePath)) {
            Utils::Log("[ERROR] self-update downloaded package not found: " + packagePath);
            return false;
        }

        std::string commandLine =
            QuoteArgument(helperPath.string()) + " " +
            QuoteArgument(currentExe) + " " +
            QuoteArgument(packagePath) + " " +
            std::to_string(GetCurrentProcessId());

        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);

        std::vector<char> mutableCommandLine(commandLine.begin(), commandLine.end());
        mutableCommandLine.push_back('\0');
        Utils::Log("[INFO] self-update helper command=" + commandLine);

        if (!CreateProcessA(
                helperPath.string().c_str(),
                mutableCommandLine.data(),
                NULL,
                NULL,
                FALSE,
                CREATE_NO_WINDOW,
                NULL,
                std::filesystem::path(currentExe).parent_path().string().c_str(),
                &si,
                &pi)) {
            Utils::Log("[ERROR] failed to launch self-update helper (" + std::to_string(GetLastError()) + "): " + helperPath.string());
            return false;
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        Utils::Log("[INFO] launched self-update helper: " + helperPath.string());
        ExitProcess(0);
        return true;
    }
}
#else
// For non-Windows dummy
#include <iostream>
namespace LauncherUtils {
    bool LaunchGame(const std::string& executablePath) {
        std::cout << "[Dummy] Launching game: " << executablePath << std::endl;
        return true;
    }

    bool OpenUrl(const std::string& url) {
        std::cout << "[Dummy] Opening URL: " << url << std::endl;
        return true;
    }

    std::string SelectFolder(const std::string& initialPath) {
        return initialPath;
    }

    bool StartSelfUpdateAndExit(const std::string& packagePath) {
        std::cout << "[Dummy] Self update requested with: " << packagePath << std::endl;
        return false;
    }
}
#endif
