#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include "downloader.h"
#include "manifest.h"

enum class UpdateStatus {
    IDLE,
    CHECKING_LAUNCHER,
    UPDATING_LAUNCHER,
    CHECKING_FILES,
    DOWNLOADING,
    VERIFYING,
    READY,
    STATUS_ERROR
};

class Updater {
public:
    Updater(const std::string& clientId);
    ~Updater();

    void Start();
    void StartCheckOnly();
    void Stop();
    void ForceRepair();
    bool CheckForUpdatesSync();
    bool ApplyOption(const std::string& optionKey);

    UpdateStatus GetStatus() const { return m_status; }
    std::string GetStatusText() const;
    const std::map<std::string, ManifestOption>& GetOptions() const { return m_options; }
    
    double GetProgress() const { return m_progress; } 
    double GetCurrentFileProgress() const { return m_currentFileProgress; }
    std::string GetCurrentFileName() const { return m_currentFileName; }
    double GetDownloadSpeed() const { return m_downloadSpeed; }

private:
    void Run();
    void CheckLauncher();
    void CheckFiles();
    void DownloadQueue();
    bool ApplyConfiguredOptions();
    void UploadDebugLogIfEnabled(const std::string& stage);

    std::string m_clientId;
    std::atomic<UpdateStatus> m_status{UpdateStatus::IDLE};
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_forceRepair{false};
    std::atomic<bool> m_checkOnly{false};
    
    std::thread m_thread;
    Downloader m_downloader;
    
    // UI feedback
    std::atomic<double> m_progress{0.0};
    std::atomic<double> m_currentFileProgress{0.0};
    std::atomic<double> m_downloadSpeed{0.0};
    std::string m_currentFileName;
    mutable std::mutex m_mutex;

    std::vector<ManifestFile> m_downloadQueue;
    std::map<std::string, ManifestOption> m_options;
    size_t m_totalDownloadSize{0};
    size_t m_currentDownloadSize{0};
    std::string m_manifestVersionToken;
};
