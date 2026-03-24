#include "extractor.h"
#include "utils.h"
#include <fstream>
#include <vector>
#include <iostream>
#include <filesystem>
#include <zlib.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Extractor {
    bool ExtractToDisk(const std::vector<unsigned char>& compressedData, const std::string& targetPath, size_t uncompressedSize) {
        std::vector<unsigned char> uncompressed(uncompressedSize);
        z_stream stream{};
        stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressedData.data()));
        stream.avail_in = static_cast<uInt>(compressedData.size());
        stream.next_out = reinterpret_cast<Bytef*>(uncompressed.data());
        stream.avail_out = static_cast<uInt>(uncompressed.size());

        int res = inflateInit2(&stream, -MAX_WBITS);
        if (res != Z_OK) {
            Utils::Log("[ERROR] failed to initialize decompression for " + targetPath + " zlib=" + std::to_string(res));
            return false;
        }

        res = inflate(&stream, Z_FINISH);
        if (res != Z_STREAM_END) {
            Utils::Log("[ERROR] decompression failed for " + targetPath + " zlib=" + std::to_string(res));
            inflateEnd(&stream);
            return false;
        }

        int endRes = inflateEnd(&stream);
        if (endRes != Z_OK) {
            Utils::Log("[ERROR] decompression cleanup failed for " + targetPath + " zlib=" + std::to_string(endRes));
            return false;
        }

        size_t outSize = static_cast<size_t>(stream.total_out);
        if (outSize != uncompressedSize) {
            Utils::Log("[WARN] decompressed size mismatch for " + targetPath + " expected=" + std::to_string(uncompressedSize) + " actual=" + std::to_string(outSize));
        }

        std::ofstream file(targetPath, std::ios::binary);
        if (!file.is_open()) {
            Utils::Log("[ERROR] failed to open extracted output file: " + targetPath);
            return false;
        }
        file.write(reinterpret_cast<const char*>(uncompressed.data()), static_cast<std::streamsize>(outSize));
        Utils::Log("[INFO] extracted file to " + targetPath);
        return true;
    }

    bool ExtractZipToDirectory(const std::string& zipPath, const std::string& destinationPath) {
#ifdef _WIN32
        std::filesystem::create_directories(destinationPath);

        std::string command =
            "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "
            "\"Expand-Archive -LiteralPath '" + zipPath + "' -DestinationPath '" + destinationPath + "' -Force\"";

        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);

        std::vector<char> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back('\0');

        Utils::Log("[INFO] extracting zip archive: " + zipPath + " -> " + destinationPath);

        if (!CreateProcessA(
                NULL,
                mutableCommand.data(),
                NULL,
                NULL,
                FALSE,
                CREATE_NO_WINDOW,
                NULL,
                NULL,
                &si,
                &pi)) {
            Utils::Log("[ERROR] failed to start Expand-Archive for " + zipPath);
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (exitCode != 0) {
            Utils::Log("[ERROR] Expand-Archive failed for " + zipPath + " exitCode=" + std::to_string(exitCode));
            return false;
        }

        Utils::Log("[INFO] extracted zip archive successfully: " + zipPath);
        return true;
#else
        (void)zipPath;
        (void)destinationPath;
        Utils::Log("[ERROR] zip extraction is only implemented on Windows");
        return false;
#endif
    }
}
