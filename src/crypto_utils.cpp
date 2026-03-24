#include "crypto_utils.h"
#include <sstream>
#include <iomanip>

namespace CryptoUtils {
    std::string ProcessXOR(const std::string& data, const std::string& key) {
        std::string result = data;
        if (key.empty()) return result;
        for (size_t i = 0; i < data.length(); ++i) {
            result[i] = data[i] ^ key[i % key.length()];
        }
        return result;
    }

    std::string ToHex(const std::string& data) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (unsigned char c : data) {
            ss << std::setw(2) << (int)c;
        }
        return ss.str();
    }

    std::string FromHex(const std::string& hex) {
        std::string result;
        for (size_t i = 0; i < hex.length(); i += 2) {
            std::string part = hex.substr(i, 2);
            char ch = (char)std::stoi(part, nullptr, 16);
            result += ch;
        }
        return result;
    }
}
