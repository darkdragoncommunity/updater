#pragma once
#include <string>
#include <vector>

namespace CryptoUtils {
    // Simple XOR-based encryption/decryption with a key
    // For a game launcher, this is enough to hide URLs from casual string snooping
    // without the complexity of full AES/RSA for static settings.
    std::string ProcessXOR(const std::string& data, const std::string& key);

    // Helper to convert to/from hex for storage in text files
    std::string ToHex(const std::string& data);
    std::string FromHex(const std::string& hex);
}
