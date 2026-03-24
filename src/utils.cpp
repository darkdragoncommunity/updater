#include "utils.h"
#include <curl/curl.h>
#include <openssl/evp.h>
#include <chrono>
#include <ctime>
#include <mutex>

namespace Utils {
    namespace {
        std::mutex g_logMutex;
        const char* kLogPath = "launcher.log";

        std::string Timestamp() {
            auto now = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm, &tt);
#else
            localtime_r(&tt, &tm);
#endif
            std::ostringstream oss;
            oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
            return oss.str();
        }
    }

    std::string CalculateFileSHA256(const std::string& filepath) {
        if (!fs::exists(filepath)) return "";

        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) return "";

        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) return "";

        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }

        const int bufferSize = 4096;
        char buffer[bufferSize];
        while (file.read(buffer, bufferSize) || file.gcount() > 0) {
            if (EVP_DigestUpdate(ctx, buffer, file.gcount()) != 1) {
                EVP_MD_CTX_free(ctx);
                return "";
            }
            if (file.eof()) break;
        }

        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int lengthOfHash = 0;
        if (EVP_DigestFinal_ex(ctx, hash, &lengthOfHash) != 1) {
            EVP_MD_CTX_free(ctx);
            return "";
        }

        EVP_MD_CTX_free(ctx);

        std::stringstream ss;
        for (unsigned int i = 0; i < lengthOfHash; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

    size_t GetFileSize(const std::string& filepath) {
        if (!fs::exists(filepath)) return 0;
        return fs::file_size(filepath);
    }

    bool FileExists(const std::string& filepath) {
        return fs::exists(filepath);
    }
    
    bool CreateDirectories(const std::string& path) {
        if (path.empty()) return true;
        try {
            fs::create_directories(path);
            return true;
        } catch (...) {
            return false;
        }
    }

    std::string AppendQueryParam(const std::string& url, const std::string& key, const std::string& value) {
        if (url.empty() || key.empty() || value.empty()) {
            return url;
        }
        const char delimiter = (url.find('?') == std::string::npos) ? '?' : '&';
        return url + delimiter + key + "=" + value;
    }

    void ResetLog() {
        std::lock_guard<std::mutex> lock(g_logMutex);
        std::ofstream file(kLogPath, std::ios::trunc);
        if (file.is_open()) {
            file << Timestamp() << " [INFO] launcher log started" << std::endl;
        }
    }

    void Log(const std::string& message) {
        std::lock_guard<std::mutex> lock(g_logMutex);
        std::ofstream file(kLogPath, std::ios::app);
        if (file.is_open()) {
            file << Timestamp() << " " << message << std::endl;
        }
        std::cerr << message << std::endl;
    }

    bool UploadLogFile(const std::string& url, const std::string& source, const std::string& stage) {
        std::ifstream file(kLogPath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        const std::string body = buffer.str();

        CURL* curl = curl_easy_init();
        if (!curl) {
            return false;
        }

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");
        headers = curl_slist_append(headers, ("X-Launcher-Source: " + source).c_str());
        headers = curl_slist_append(headers, ("X-Launcher-Stage: " + stage).c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

        const CURLcode res = curl_easy_perform(curl);
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        return (res == CURLE_OK) && (responseCode >= 200 && responseCode < 300);
    }
}
