#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <curl/curl.h>

class Downloader {
public:
    using ProgressCallback = std::function<void(double dlNow, double dlTotal)>;

    Downloader();
    ~Downloader();

    // Returns true if successful
    bool DownloadFile(const std::string& url, const std::string& filepath, ProgressCallback callback = nullptr);

    // Downloads a file directly to memory (for decompression)
    bool DownloadToMemory(const std::string& url, std::vector<unsigned char>& outBuffer, ProgressCallback callback = nullptr);

    // Returns file content as string (for small files like manifest/version)
    std::string DownloadString(const std::string& url);

    void Stop();
    bool IsStopped() const { return m_stop; }

private:
    std::atomic<bool> m_stop{false};
    CURL* m_curl;

    static size_t WriteData(void* ptr, size_t size, size_t nmemb, void* stream);
};
