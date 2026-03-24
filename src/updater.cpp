#include "updater.h"
#include "utils.h"
#include "config.h"
#include "language.h"
#include "extractor.h"
#include "launcher_utils.h"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <vector>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

#ifndef LAUNCHER_VERSION
#define LAUNCHER_VERSION "0.0.0"
#endif

namespace {
    std::atomic<bool> g_launcherCheckPerformed{false};
    constexpr int kMaxLauncherUpdateRetries = 2;
    std::string gLauncherRetryVersion;
    int gLauncherRetryCount = 0;

    std::string TrimWhitespace(const std::string& value) {
        const auto start = value.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            return "";
        }
        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(start, end - start + 1);
    }

    std::string BuildCacheBustedUrl(const std::string& url, const std::string& token) {
        if (token.empty()) {
            return url;
        }
        return Utils::AppendQueryParam(url, "v", token);
    }

    std::string DownloadStringWithMirrors(Downloader& downloader, const std::vector<std::string>& urls) {
        for (const auto& url : urls) {
            std::string data = downloader.DownloadString(url);
            if (!data.empty()) {
                return data;
            }
            Utils::Log("[WARN] mirror string download failed: " + url);
        }
        return "";
    }

    std::string FetchManifestVersionToken(Downloader& downloader, const std::vector<std::string>& baseUrls, const std::string& clientId) {
        std::vector<std::string> versionUrls;
        versionUrls.reserve(baseUrls.size());
        for (const auto& baseUrl : baseUrls) {
            versionUrls.push_back(baseUrl + "/" + clientId + "/manifest_version.txt");
        }
        std::string token = DownloadStringWithMirrors(downloader, versionUrls);
        return TrimWhitespace(token);
    }

    bool DownloadFileWithMirrors(Downloader& downloader, const std::vector<std::string>& urls, const std::string& path, Downloader::ProgressCallback callback = nullptr) {
        for (const auto& url : urls) {
            if (downloader.DownloadFile(url, path, callback)) {
                return true;
            }
            Utils::Log("[WARN] mirror file download failed: " + url);
        }
        return false;
    }

    bool DownloadToMemoryWithMirrors(Downloader& downloader, const std::vector<std::string>& urls, std::vector<unsigned char>& outBuffer, Downloader::ProgressCallback callback = nullptr) {
        for (const auto& url : urls) {
            if (downloader.DownloadToMemory(url, outBuffer, callback)) {
                return true;
            }
            Utils::Log("[WARN] mirror memory download failed: " + url);
        }
        return false;
    }
}

Updater::Updater(const std::string& clientId) : m_clientId(clientId) {
    m_status = UpdateStatus::IDLE;
}

Updater::~Updater() {
    Stop();
}

void Updater::UploadDebugLogIfEnabled(const std::string& stage) {
    Config& config = Config::GetInstance();
    if (!config.IsRemoteDebugEnabled()) {
        return;
    }

    if (Utils::UploadLogFile(config.GetRemoteDebugLogUrl(), m_clientId, stage)) {
        Utils::Log("[INFO] uploaded remote debug log client=" + m_clientId + " stage=" + stage);
    } else {
        Utils::Log("[WARN] failed to upload remote debug log client=" + m_clientId + " stage=" + stage);
    }
}

void Updater::Start() {
    if (m_thread.joinable()) {
        if (m_status == UpdateStatus::DOWNLOADING || m_status == UpdateStatus::VERIFYING || m_status == UpdateStatus::CHECKING_FILES) {
            Utils::Log("[INFO] updater already running, ignoring start client=" + m_clientId);
            return;
        }
        m_thread.join();
    }

    if (!m_thread.joinable()) {
        m_stop = false;
        m_checkOnly = false;
        m_progress = 0.0;
        m_currentFileProgress = 0.0;
        m_downloadSpeed = 0.0;
        m_status = UpdateStatus::IDLE;
        m_thread = std::thread(&Updater::Run, this);
    }
}

void Updater::StartCheckOnly() {
    if (m_thread.joinable() && m_status == UpdateStatus::IDLE) {
        m_thread.join();
    }

    if (!m_thread.joinable() && m_status == UpdateStatus::IDLE) {
        m_stop = false;
        m_checkOnly = true;
        m_thread = std::thread(&Updater::Run, this);
    }
}

void Updater::Stop() {
    m_stop = true;
    m_downloader.Stop();
    if (m_thread.joinable()) m_thread.join();
    m_status = UpdateStatus::IDLE;
}

void Updater::ForceRepair() {
    m_forceRepair = true;
    Start();
}

void Updater::Run() {
    try {
        Utils::Log("[INFO] updater run start client=" + m_clientId);
        CheckLauncher();
        if (m_stop) return;

        CheckFiles();
        if (m_stop) return;

        if (m_checkOnly) {
            m_status = UpdateStatus::IDLE;
            Utils::Log("[INFO] updater check-only finished client=" + m_clientId);
            return;
        }

        if (!m_downloadQueue.empty()) {
            DownloadQueue();
        }

        if (m_stop) return;
        if (m_status == UpdateStatus::STATUS_ERROR) {
            GameClient* client = Config::GetInstance().GetClient(m_clientId);
            if (client) {
                client->state = ClientState::UPDATE_AVAILABLE;
            }
            Utils::Log("[WARN] updater run ended with status error client=" + m_clientId);
            UploadDebugLogIfEnabled("run_status_error");
            return;
        }

        if (!ApplyConfiguredOptions()) {
            GameClient* client = Config::GetInstance().GetClient(m_clientId);
            if (client) {
                client->state = ClientState::UPDATE_AVAILABLE;
            }
            m_status = UpdateStatus::STATUS_ERROR;
            Utils::Log("[ERROR] updater configured option apply failed client=" + m_clientId);
            UploadDebugLogIfEnabled("apply_configured_options");
            return;
        }

        m_status = UpdateStatus::READY;
        m_progress = 1.0;
        
        // Update client state in config
        GameClient* client = Config::GetInstance().GetClient(m_clientId);
        if (client) {
            client->state = ClientState::READY;
            client->lastRunCheck = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            Config::GetInstance().Save();
        }
        Utils::Log("[INFO] updater run finished client=" + m_clientId + " status=READY");
    } catch (const std::exception& e) {
        Utils::Log(std::string("[ERROR] updater run exception client=") + m_clientId + " error=" + e.what());
        m_status = UpdateStatus::STATUS_ERROR;
        UploadDebugLogIfEnabled("run_exception");
    } catch (...) {
        Utils::Log("[ERROR] updater run exception client=" + m_clientId + " error=unknown");
        m_status = UpdateStatus::STATUS_ERROR;
        UploadDebugLogIfEnabled("run_exception_unknown");
    }
}

bool Updater::ApplyConfiguredOptions() {
    GameClient* client = Config::GetInstance().GetClient(m_clientId);
    if (!client) {
        return false;
    }

    auto applyIfPresent = [&](const std::string& optionKey) {
        if (optionKey.empty()) {
            return true;
        }
        if (m_options.find(optionKey) == m_options.end()) {
            Utils::Log("[INFO] configured option not present in manifest, skipping: " + optionKey);
            return true;
        }
        return ApplyOption(optionKey);
    };

    auto applyToggleOption = [&](bool enabled, const std::string& enableKey, const std::string& disableKey) {
        if (enabled) {
            return applyIfPresent(enableKey);
        }
        if (!disableKey.empty() && m_options.find(disableKey) != m_options.end()) {
            return ApplyOption(disableKey);
        }
        return true;
    };

    bool success = true;
    if (m_clientId == "classic") {
        success = success && applyIfPresent(client->classicLanguage);
        success = success && applyIfPresent(client->classicUiVariant);
        success = success && applyToggleOption(client->classicBorderless, "borderless", "borderless_default");
        success = success && applyToggleOption(client->classicColorfulSystemMessages, "colorful_system_messages", "colorful_system_messages_default");
    } else if (m_clientId == "interlude") {
        success = success && applyToggleOption(client->interludeBorderless, "borderless", "borderless_default");
        success = success && applyToggleOption(client->interludeRightClickNoRotate, client->interludeRightClickOption, client->interludeRightClickRestoreOption);
        success = success && applyToggleOption(client->interludeColorfulSystemMessages, "colorful_system_messages", "colorful_system_messages_default");
    }

    return success;
}

void Updater::CheckLauncher() {
    bool expected = false;
    if (!g_launcherCheckPerformed.compare_exchange_strong(expected, true)) {
        Utils::Log("[INFO] launcher check already performed in this process, skipping for client=" + m_clientId);
        return;
    }

    // Launcher self-update logic remains global or can be tied to a specific check
    m_status = UpdateStatus::CHECKING_LAUNCHER;
    const auto& baseUrls = Config::GetInstance().GetRemotePatchUrls();
    std::vector<std::string> versionUrls;
    for (const auto& baseUrl : baseUrls) {
        versionUrls.push_back(baseUrl + "/launcher/version.json");
    }
    std::string versionData = DownloadStringWithMirrors(m_downloader, versionUrls);
    if (versionData.empty()) {
        Utils::Log("[WARN] launcher version check skipped due to empty response");
        return;
    }

    try {
        nlohmann::json json = nlohmann::json::parse(versionData);
        std::string remoteVersion = json["version"];
        std::string localVersion = LAUNCHER_VERSION;
        Utils::Log("[INFO] launcher version local=" + localVersion + " remote=" + remoteVersion);

        if (remoteVersion != localVersion) {
            if (gLauncherRetryVersion == remoteVersion && gLauncherRetryCount >= kMaxLauncherUpdateRetries) {
                Utils::Log("[WARN] launcher self-update skipped due to retry limit remote=" + remoteVersion + " retries=" + std::to_string(gLauncherRetryCount));
                return;
            }
            m_status = UpdateStatus::UPDATING_LAUNCHER;
            std::string launcherUrl = json["launcher_url"];
            std::filesystem::path currentExeDir = std::filesystem::current_path();
#ifdef _WIN32
            char modulePath[MAX_PATH] = {};
            DWORD length = GetModuleFileNameA(NULL, modulePath, MAX_PATH);
            if (length > 0 && length < MAX_PATH) {
                currentExeDir = std::filesystem::path(modulePath).parent_path();
            }
#endif
            std::string tempLauncher = (currentExeDir / "launcher_update.zip").string();
            Utils::Log("[INFO] launcher update required url=" + launcherUrl + " target=" + tempLauncher);

            std::vector<std::string> launcherUrls = {launcherUrl};
            if (!(launcherUrl.rfind("http://", 0) == 0 || launcherUrl.rfind("https://", 0) == 0)) {
                launcherUrls.clear();
                for (const auto& baseUrl : baseUrls) {
                    launcherUrls.push_back(baseUrl + "/" + launcherUrl);
                }
            }
            for (const auto& candidateUrl : launcherUrls) {
                Utils::Log("[INFO] launcher update candidate url=" + candidateUrl);
            }
            if (gLauncherRetryVersion == remoteVersion && gLauncherRetryCount >= kMaxLauncherUpdateRetries) {
                Utils::Log("[WARN] launcher self-update skipped due to retry limit remote=" + remoteVersion + " retries=" + std::to_string(gLauncherRetryCount));
                return;
            }
            if (gLauncherRetryVersion != remoteVersion) {
                gLauncherRetryVersion = remoteVersion;
                gLauncherRetryCount = 0;
            }
            gLauncherRetryCount++;
            Utils::Log("[INFO] launcher self-update attempt remote=" + remoteVersion + " attempt=" + std::to_string(gLauncherRetryCount));

            if (DownloadFileWithMirrors(m_downloader, launcherUrls, tempLauncher)) {
                Utils::Log("[INFO] launcher update downloaded successfully path=" + tempLauncher);
                Utils::Log("[INFO] starting self-update helper");
                if (!LauncherUtils::StartSelfUpdateAndExit(tempLauncher)) {
                    Utils::Log("[ERROR] failed to start self-update helper");
                    m_status = UpdateStatus::STATUS_ERROR;
                }
            } else {
                Utils::Log("[ERROR] launcher update download failed");
                m_status = UpdateStatus::STATUS_ERROR;
            }
        } else {
            gLauncherRetryVersion.clear();
            gLauncherRetryCount = 0;
            Utils::Log("[INFO] launcher already up to date");
        }
    } catch (const std::exception& e) {
        Utils::Log(std::string("[ERROR] failed to parse launcher version metadata: ") + e.what());
    } catch (...) {
        Utils::Log("[ERROR] failed to parse launcher version metadata: unknown exception");
    }
}

void Updater::CheckFiles() {
    CheckForUpdatesSync();
}

bool Updater::CheckForUpdatesSync() {
    m_status = m_forceRepair ? UpdateStatus::VERIFYING : UpdateStatus::CHECKING_FILES;
    m_progress = 0.0;
    const auto& baseUrls = Config::GetInstance().GetRemotePatchUrls();
    GameClient* client = Config::GetInstance().GetClient(m_clientId);
    if (!client) {
        Utils::Log("[ERROR] client not found: " + m_clientId);
        return false;
    }

    m_manifestVersionToken = FetchManifestVersionToken(m_downloader, baseUrls, m_clientId);
    if (!m_manifestVersionToken.empty()) {
        Utils::Log("[INFO] manifest cache token client=" + m_clientId + " token=" + m_manifestVersionToken);
    }

    std::vector<std::string> manifestUrls;
    for (const auto& baseUrl : baseUrls) {
        manifestUrls.push_back(BuildCacheBustedUrl(baseUrl + "/" + m_clientId + "/manifest.json", m_manifestVersionToken));
    }
    if (!manifestUrls.empty()) {
        Utils::Log("[INFO] checking manifest client=" + m_clientId + " url=" + manifestUrls.front());
    }
    std::string manifestData = DownloadStringWithMirrors(m_downloader, manifestUrls);
    if (manifestData.empty()) {
        m_status = UpdateStatus::STATUS_ERROR;
        Utils::Log("[ERROR] empty manifest response client=" + m_clientId);
        return false;
    }

    Manifest manifest = Manifest::Parse(manifestData);
    client->runPath = manifest.GetRunPath();
    m_options = manifest.GetOptions();
    if (!manifest.GetManifestVersion().empty()) {
        m_manifestVersionToken = manifest.GetManifestVersion();
        Utils::Log("[INFO] manifest version parsed: " + m_manifestVersionToken);
    }
    Utils::Log("[INFO] client=" + m_clientId + " runPath=" + client->runPath);
    
    m_downloadQueue.clear();
    m_totalDownloadSize = 0;
    const auto& manifestFiles = manifest.GetFiles();
    const size_t totalFiles = manifestFiles.size();

    if (m_forceRepair) {
        unsigned int workerCount = std::thread::hardware_concurrency();
        if (workerCount == 0) {
            workerCount = 4;
        }
        if (workerCount > 6) {
            workerCount = 6;
        }
        if (workerCount < 1) {
            workerCount = 1;
        }
        std::atomic<size_t> nextIndex{0};
        std::atomic<size_t> processedFiles{0};
        std::vector<bool> needsUpdate(totalFiles, false);
        std::vector<std::thread> workers;
        workers.reserve(workerCount);

        auto verifyWorker = [&]() {
            while (!m_stop) {
                const size_t index = nextIndex.fetch_add(1);
                if (index >= totalFiles) {
                    break;
                }

                const auto& mFile = manifestFiles[index];
                bool localNeedsUpdate = false;

                if (mFile.action == "delete") {
                    std::string fullLocalPath = client->installPath + "/" + mFile.path;
                    if (Utils::FileExists(fullLocalPath)) {
                        try {
                            fs::remove(fullLocalPath);
                        } catch (...) {
                            localNeedsUpdate = false;
                        }
                    }
                } else {
                    std::string fullLocalPath = client->installPath + "/" + mFile.path;
                    const bool fileExists = Utils::FileExists(fullLocalPath);
                    if (!fileExists) {
                        localNeedsUpdate = true;
                    } else {
                        size_t localSize = Utils::GetFileSize(fullLocalPath);
                        if (localSize != mFile.size) {
                            localNeedsUpdate = true;
                        } else {
                            std::string localHash = Utils::CalculateFileSHA256(fullLocalPath);
                            if (localHash != mFile.sha256) {
                                localNeedsUpdate = true;
                            }
                        }
                    }
                }

                needsUpdate[index] = localNeedsUpdate;
                const size_t done = processedFiles.fetch_add(1) + 1;
                if (totalFiles > 0) {
                    m_progress = static_cast<double>(done) / static_cast<double>(totalFiles);
                }
            }
        };

        for (unsigned int i = 0; i < workerCount; ++i) {
            workers.emplace_back(verifyWorker);
        }
        for (auto& worker : workers) {
            worker.join();
        }

        if (m_stop) {
            return false;
        }

        for (size_t index = 0; index < totalFiles; ++index) {
            const auto& mFile = manifestFiles[index];
            if (mFile.action == "delete") {
                continue;
            }
            if (needsUpdate[index]) {
                m_downloadQueue.push_back(mFile);
                m_totalDownloadSize += mFile.compressedSha256.empty() ? mFile.size : mFile.compressedSize;
            }
        }

        m_progress = 1.0;
    } else {
        size_t processedFiles = 0;

        for (const auto& mFile : manifestFiles) {
            if (m_stop) return false;

            std::string fullLocalPath = client->installPath + "/" + mFile.path;

            if (mFile.action == "delete") {
                if (Utils::FileExists(fullLocalPath)) {
                    fs::remove(fullLocalPath);
                }
                continue;
            }

            bool needsUpdate = false;
            const bool fileExists = Utils::FileExists(fullLocalPath);
            if (!fileExists) {
                needsUpdate = true;
            } else {
                // Milestone 8: File Exception List
                // If the file exists and is marked as 'skipIfExists', we don't update it.
                if (mFile.skipIfExists && client->state != ClientState::NOT_INSTALLED) {
                    // Skip hashing and update, trust local file
                } else {
                    size_t localSize = Utils::GetFileSize(fullLocalPath);
                    if (localSize != mFile.size) {
                        needsUpdate = true;
                    }
                }
            }

            if (needsUpdate) {
                m_downloadQueue.push_back(mFile);
                m_totalDownloadSize += mFile.compressedSha256.empty() ? mFile.size : mFile.compressedSize;
            }

            ++processedFiles;
        }
    }

    m_forceRepair = false;
    if (!m_downloadQueue.empty()) {
        client->state = ClientState::UPDATE_AVAILABLE;
        m_status = UpdateStatus::IDLE;
        Utils::Log("[INFO] updates available client=" + m_clientId + " files=" + std::to_string(m_downloadQueue.size()));
        return true;
    } else {
        client->state = ClientState::READY;
        m_status = UpdateStatus::IDLE;
        Utils::Log("[INFO] client ready with no updates client=" + m_clientId);
        return false;
    }
}

bool Updater::ApplyOption(const std::string& optionKey) {
    auto optionIt = m_options.find(optionKey);
    if (optionIt == m_options.end()) {
        Utils::Log("[ERROR] option not found in manifest: " + optionKey);
        return false;
    }

    GameClient* client = Config::GetInstance().GetClient(m_clientId);
    if (!client) {
        Utils::Log("[ERROR] cannot apply option, client not found: " + m_clientId);
        return false;
    }

    const ManifestOption& option = optionIt->second;
    std::string sourceUrl = option.source;
    std::string normalizedSource = sourceUrl;
    while (!normalizedSource.empty() && (normalizedSource.front() == '/' || normalizedSource.front() == '\\')) {
        normalizedSource.erase(0, 1);
    }
    if (sourceUrl.empty() || normalizedSource.empty()) {
        Utils::Log("[ERROR] option source missing: " + optionKey);
        return false;
    }

    std::string urlPath = normalizedSource;
    if (urlPath.rfind("files/", 0) != 0) {
        urlPath = "files/" + urlPath;
    }

    fs::path cachedOptionPath = client->installPath;
    cachedOptionPath /= normalizedSource;
    bool useCachedOption = fs::exists(cachedOptionPath);

    std::vector<std::string> sourceUrls;
    if (sourceUrl.rfind("http://", 0) == 0 || sourceUrl.rfind("https://", 0) == 0) {
        sourceUrls.push_back(sourceUrl);
    } else {
        for (const auto& baseUrl : Config::GetInstance().GetRemotePatchUrls()) {
            const std::string generatedUrl = baseUrl + "/" + m_clientId + "/" + urlPath;
            sourceUrls.push_back(BuildCacheBustedUrl(generatedUrl, m_manifestVersionToken));
        }
    }

    std::filesystem::path extractPath = client->installPath;
    if (!option.extract.empty()) {
        extractPath /= option.extract;
    }
    extractPath = extractPath.lexically_normal();

    std::filesystem::path tempZip = std::filesystem::temp_directory_path() / (m_clientId + "_" + optionKey + ".zip");
    std::string zipSourcePath;
    bool tempDownloaded = false;
    if (useCachedOption) {
        zipSourcePath = cachedOptionPath.string();
        Utils::Log("[INFO] using cached option archive: " + zipSourcePath);
    } else {
        Utils::Log("[INFO] applying option " + optionKey + " to " + extractPath.string());
        Downloader downloader;
        if (!DownloadFileWithMirrors(downloader, sourceUrls, tempZip.string())) {
            Utils::Log("[ERROR] failed to download option archive: " + optionKey);
            return false;
        }
        zipSourcePath = tempZip.string();
        tempDownloaded = true;
    }

    bool success = Extractor::ExtractZipToDirectory(zipSourcePath, extractPath.string());
    if (tempDownloaded) {
        std::error_code ec;
        std::filesystem::remove(tempZip, ec);
    }
    if (!success) {
        Utils::Log("[ERROR] failed to apply option archive: " + optionKey);
        return false;
    }

    Utils::Log("[INFO] option applied successfully: " + optionKey);
    return true;
}

void Updater::DownloadQueue() {
    m_status = UpdateStatus::DOWNLOADING;
    m_currentDownloadSize = 0;
    GameClient* client = Config::GetInstance().GetClient(m_clientId);
    if (!client) return;

    if (m_downloadQueue.empty()) {
        return;
    }

    auto startTime = std::chrono::steady_clock::now();
    const size_t maxParallelDownloads = std::min<size_t>(5, m_downloadQueue.size());
    constexpr int maxRetriesPerFile = 3;
    std::atomic<size_t> nextIndex{0};
    std::atomic<bool> failed{false};
    std::atomic<long long> downloadedBytes{0};

    auto updateAggregateProgress = [&](double currentFileProgress, long long fileBytesDone) {
        if (fileBytesDone > 0) {
            downloadedBytes.fetch_add(fileBytesDone);
        }
        m_currentFileProgress = currentFileProgress;
        long long totalDl = downloadedBytes.load();
        if (m_totalDownloadSize > 0) {
            m_progress = static_cast<double>(totalDl) / static_cast<double>(m_totalDownloadSize);
        }
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        if (duration > 0) {
            m_downloadSpeed = static_cast<double>(totalDl) / static_cast<double>(duration);
        }
    };

    auto worker = [&]() {
        Downloader downloader;

        while (!m_stop && !failed) {
            size_t index = nextIndex.fetch_add(1);
            if (index >= m_downloadQueue.size()) {
                return;
            }

            const auto& mFile = m_downloadQueue[index];
            std::string fullLocalPath = client->installPath + "/" + mFile.path;

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_currentFileName = mFile.path;
            }

            fs::path p(fullLocalPath);
            if (p.has_parent_path()) {
                Utils::CreateDirectories(p.parent_path().string());
            }

            bool useCompressed = !mFile.compressedSha256.empty();
            size_t transferSize = useCompressed ? mFile.compressedSize : mFile.size;
            Utils::Log("[INFO] downloading client file=" + mFile.path + " compressed=" + std::string(useCompressed ? "true" : "false"));
            std::vector<std::string> candidateUrls;
            for (const auto& baseUrl : Config::GetInstance().GetRemotePatchUrls()) {
                const std::string url = baseUrl + "/" + m_clientId + "/files/" + mFile.path + (useCompressed ? ".gz" : "");
                candidateUrls.push_back(BuildCacheBustedUrl(url, m_manifestVersionToken));
            }

            double lastReported = 0.0;
            auto progressCallback = [&](double dlNow, double dlTotal) {
                long long delta = static_cast<long long>(dlNow - lastReported);
                if (delta > 0) {
                    lastReported = dlNow;
                } else {
                    delta = 0;
                }
                updateAggregateProgress(dlTotal > 0 ? (dlNow / dlTotal) : 0.0, delta);
            };

            bool success = false;
            for (int attempt = 1; attempt <= maxRetriesPerFile && !m_stop && !failed; ++attempt) {
                lastReported = 0.0;
                m_currentFileProgress = 0.0;

                if (useCompressed) {
                    std::vector<unsigned char> compressedData;
                    success = DownloadToMemoryWithMirrors(downloader, candidateUrls, compressedData, progressCallback);

                    if (success) {
                        success = Extractor::ExtractToDisk(compressedData, fullLocalPath, mFile.size);
                    }
                } else {
                    success = DownloadFileWithMirrors(downloader, candidateUrls, fullLocalPath, progressCallback);
                }

                if (success) {
                    break;
                }

                Utils::Log("[WARN] download attempt failed client file=" + mFile.path +
                           " attempt=" + std::to_string(attempt) +
                           "/" + std::to_string(maxRetriesPerFile));
                if (attempt < maxRetriesPerFile) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(400));
                }
            }

            if (!success) {
                failed = true;
                m_status = UpdateStatus::STATUS_ERROR;
                Utils::Log("[ERROR] failed to download client file=" + mFile.path);
                return;
            }

            long long remainder = static_cast<long long>(transferSize) - static_cast<long long>(lastReported);
            if (remainder > 0) {
                updateAggregateProgress(1.0, remainder);
            }
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_currentDownloadSize += transferSize;
            }
            Utils::Log("[INFO] finished client file=" + mFile.path);
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(maxParallelDownloads);
    for (size_t i = 0; i < maxParallelDownloads; ++i) {
        workers.emplace_back(worker);
    }

    for (auto& workerThread : workers) {
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    if (failed) {
        return;
    }

    m_progress = 1.0;
}

std::string Updater::GetStatusText() const {
    Language& lang = Language::GetInstance();
    
    GameClient* client = Config::GetInstance().GetClient(m_clientId);
    if (client && client->state == ClientState::UPDATE_AVAILABLE && m_status == UpdateStatus::IDLE) {
        return lang.GetString("update_available");
    }

    switch (m_status) {
        case UpdateStatus::IDLE: return lang.GetString("idle");
        case UpdateStatus::CHECKING_LAUNCHER: return lang.GetString("checking_launcher");
        case UpdateStatus::UPDATING_LAUNCHER: return lang.GetString("updating_launcher");
        case UpdateStatus::CHECKING_FILES: return lang.GetString("checking_files");
        case UpdateStatus::DOWNLOADING: return lang.GetString("downloading");
        case UpdateStatus::VERIFYING: return lang.GetString("verifying");
        case UpdateStatus::READY: return lang.GetString("ready");
        case UpdateStatus::STATUS_ERROR: return lang.GetString("error");
    }
    return "";
}
