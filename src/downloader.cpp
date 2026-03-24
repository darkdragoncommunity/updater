#include "downloader.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <filesystem>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 26812) // Unscoped enum CURLcode
#endif
#include <curl/curl.h>

namespace fs = std::filesystem;

Downloader::Downloader() {
    m_curl = curl_easy_init();
}

Downloader::~Downloader() {
    if (m_curl) curl_easy_cleanup(m_curl);
}

size_t Downloader::WriteData(void* ptr, size_t size, size_t nmemb, void* stream) {
    std::ofstream* out = (std::ofstream*)stream;
    out->write((char*)ptr, size * nmemb);
    return size * nmemb;
}

static size_t WriteStringData(void* ptr, size_t size, size_t nmemb, void* stream) {
    std::string* out = (std::string*)stream;
    out->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

struct ProgressData {
    Downloader* downloader;
    Downloader::ProgressCallback callback;

    ProgressData() : downloader(nullptr), callback(nullptr) {}
};

static int XferInfoCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    ProgressData* data = (ProgressData*)clientp;
    if (data->downloader && data->downloader->IsStopped()) return 1; // Abort
    if (data->callback) {
        data->callback((double)dlnow, (double)dltotal);
    }
    return 0;
}

bool Downloader::DownloadFile(const std::string& url, const std::string& filepath, ProgressCallback callback) {
    if (!m_curl) m_curl = curl_easy_init();
    if (!m_curl) return false;

    m_stop = false;
    Utils::Log("[INFO] DownloadFile url=" + url + " target=" + filepath);

    std::string tempPath = filepath + ".tmp";
    curl_off_t resumeOffset = 0;

    // Check for partial download
    if (fs::exists(tempPath)) {
        resumeOffset = (curl_off_t)fs::file_size(tempPath);
    }

    std::ofstream file(tempPath, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        Utils::Log("[ERROR] failed to open temp file for download: " + tempPath);
        return false;
    }

    curl_easy_reset(m_curl);
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "GameLauncher/1.0");
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, Downloader::WriteData);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &file);
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(m_curl, CURLOPT_ACCEPT_ENCODING, ""); // Enable all supported compressions

    if (resumeOffset > 0) {
        curl_easy_setopt(m_curl, CURLOPT_RESUME_FROM_LARGE, (curl_off_t)resumeOffset);
    }

    ProgressData progData;
    progData.downloader = this;
    progData.callback = callback;

    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, &progData);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, XferInfoCallback);

    CURLcode res = curl_easy_perform(m_curl);

    file.close();

    if (res == CURLE_OK) {
        // Move temp to final
        try {
            fs::rename(tempPath, filepath);
            Utils::Log("[INFO] download completed: " + filepath);
            return true;
        } catch (std::exception& e) {
            Utils::Log(std::string("[ERROR] failed to rename downloaded file: ") + e.what());
            std::cerr << "Failed to rename file: " << e.what() << std::endl;
            return false;
        }
    } else {
        long responseCode = 0;
        curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &responseCode);
        Utils::Log("[ERROR] download failed url=" + url + " curl=" + std::string(curl_easy_strerror(res)) + " http=" + std::to_string(responseCode));
        // If aborted or error, keep temp for resume unless it's a 404 or critical error
        // For now, keep it.
        std::cerr << "Curl error: " << curl_easy_strerror(res) << std::endl;
        return false;
    }
}

static size_t WriteMemoryData(void* ptr, size_t size, size_t nmemb, void* stream) {
    std::vector<unsigned char>* out = (std::vector<unsigned char>*)stream;
    out->insert(out->end(), (unsigned char*)ptr, (unsigned char*)ptr + size * nmemb);
    return size * nmemb;
}

bool Downloader::DownloadToMemory(const std::string& url, std::vector<unsigned char>& outBuffer, ProgressCallback callback) {
    if (!m_curl) m_curl = curl_easy_init();
    if (!m_curl) return false;

    m_stop = false;
    outBuffer.clear();
    Utils::Log("[INFO] DownloadToMemory url=" + url);

    curl_easy_reset(m_curl);
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "GameLauncher/1.0");
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, WriteMemoryData);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &outBuffer);
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, 1L);

    ProgressData progData;
    progData.downloader = this;
    progData.callback = callback;
    curl_easy_setopt(m_curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFODATA, &progData);
    curl_easy_setopt(m_curl, CURLOPT_XFERINFOFUNCTION, XferInfoCallback);

    CURLcode res = curl_easy_perform(m_curl);
    if (res != CURLE_OK) {
        long responseCode = 0;
        curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &responseCode);
        Utils::Log("[ERROR] memory download failed url=" + url + " curl=" + std::string(curl_easy_strerror(res)) + " http=" + std::to_string(responseCode));
    }
    return (res == CURLE_OK);
}

std::string Downloader::DownloadString(const std::string& url) {
    if (!m_curl) m_curl = curl_easy_init();
    if (!m_curl) return "";

    std::string buffer;
    Utils::Log("[INFO] DownloadString url=" + url);
    curl_easy_reset(m_curl);
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_USERAGENT, "GameLauncher/1.0");
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, WriteStringData);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(m_curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(m_curl, CURLOPT_ACCEPT_ENCODING, ""); // Enable all supported compressions
    
    CURLcode res = curl_easy_perform(m_curl);
    if (res != CURLE_OK) {
        long responseCode = 0;
        curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &responseCode);
        Utils::Log("[ERROR] string download failed url=" + url + " curl=" + std::string(curl_easy_strerror(res)) + " http=" + std::to_string(responseCode));
        return "";
    }
    Utils::Log("[INFO] DownloadString success bytes=" + std::to_string(buffer.size()));
    return buffer;
}

void Downloader::Stop() {
    m_stop = true;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
