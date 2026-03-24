#pragma once
#include <string>
#include <vector>

namespace Extractor {
    // Extracts a memory buffer (containing DEFLATE data) to a file
    bool ExtractToDisk(const std::vector<unsigned char>& compressedData, const std::string& targetPath, size_t uncompressedSize);

    // Extract a zip archive file to a destination directory
    bool ExtractZipToDirectory(const std::string& zipPath, const std::string& destinationPath);
}
