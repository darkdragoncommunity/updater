#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace Utils {
    // Calculate SHA256 of a file
    std::string CalculateFileSHA256(const std::string& filepath);

    // Get file size
    size_t GetFileSize(const std::string& filepath);

    // Check if file exists
    bool FileExists(const std::string& filepath);
    
    // Create directories recursively
    bool CreateDirectories(const std::string& path);

    // Append a single query parameter to a URL
    std::string AppendQueryParam(const std::string& url, const std::string& key, const std::string& value);

    // Reset the launcher log file for a fresh run
    void ResetLog();

    // Append a single line to the launcher log
    void Log(const std::string& message);

    // Upload the current launcher log to a remote endpoint.
    bool UploadLogFile(const std::string& url, const std::string& source, const std::string& stage);
}
