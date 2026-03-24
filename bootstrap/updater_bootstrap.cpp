#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <vector>

namespace {
    void LogBootstrap(const std::string& message) {
        try {
            const std::filesystem::path logPath = std::filesystem::current_path() / "autoupdate.log";
            std::ofstream out(logPath, std::ios::app);
            if (out.is_open()) {
                out << message << std::endl;
            }
        } catch (...) {
        }
    }
}

int main(int argc, char** argv) {
    LogBootstrap("[INFO] autoupdate helper start");
    if (argc < 4) {
        LogBootstrap("[ERROR] invalid arguments");
        std::cerr << "Usage: updater_bootstrap [old_exe] [new_exe] [pid_to_wait]" << std::endl;
        return -1;
    }

    std::string oldExe = argv[1];
    std::string packageZip = argv[2];
    DWORD pid = std::stoul(argv[3]);
    LogBootstrap("[INFO] oldExe=" + oldExe);
    LogBootstrap("[INFO] packageZip=" + packageZip);
    LogBootstrap("[INFO] waitPid=" + std::to_string(pid));

    // 1. Wait for PID to end
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (hProcess != NULL) {
        LogBootstrap("[INFO] waiting for launcher process to exit");
        WaitForSingleObject(hProcess, INFINITE);
        CloseHandle(hProcess);
        LogBootstrap("[INFO] launcher process exited");
    } else {
        LogBootstrap("[WARN] failed to open launcher process handle");
    }
    
    // Extra safety sleep
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    const std::filesystem::path launcherDir = std::filesystem::path(oldExe).parent_path();
    if (!std::filesystem::exists(packageZip)) {
        LogBootstrap("[ERROR] launcher update package not found");
        return -1;
    }

    std::string command =
        "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "
        "\"Expand-Archive -LiteralPath '" + packageZip + "' -DestinationPath '" + launcherDir.string() + "' -Force\"";

    STARTUPINFOA extractSi{};
    PROCESS_INFORMATION extractPi{};
    extractSi.cb = sizeof(extractSi);
    std::vector<char> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back('\0');

    LogBootstrap("[INFO] extracting launcher package to " + launcherDir.string());
    if (!CreateProcessA(NULL, mutableCommand.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, launcherDir.string().c_str(), &extractSi, &extractPi)) {
        LogBootstrap("[ERROR] failed to start Expand-Archive code=" + std::to_string(GetLastError()));
        return -1;
    }

    WaitForSingleObject(extractPi.hProcess, INFINITE);
    DWORD extractExitCode = 1;
    GetExitCodeProcess(extractPi.hProcess, &extractExitCode);
    CloseHandle(extractPi.hProcess);
    CloseHandle(extractPi.hThread);
    if (extractExitCode != 0) {
        LogBootstrap("[ERROR] Expand-Archive failed exitCode=" + std::to_string(extractExitCode));
        return -1;
    }
    LogBootstrap("[INFO] launcher package extracted successfully");

    std::error_code ec;
    std::filesystem::remove(packageZip, ec);
    if (ec) {
        LogBootstrap("[WARN] failed to remove launcher package after extraction");
    }

    // 4. Restart old
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::string workingDir = launcherDir.string();
    if (!CreateProcessA(oldExe.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, workingDir.c_str(), &si, &pi)) {
        LogBootstrap("[ERROR] failed to restart launcher code=" + std::to_string(GetLastError()));
        std::cerr << "Failed to restart: " << GetLastError() << std::endl;
        return -1;
    }
    LogBootstrap("[INFO] launcher restarted successfully");

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    LogBootstrap("[INFO] autoupdate helper done");

    return 0;
}
