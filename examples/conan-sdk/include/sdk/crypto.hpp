#pragma once

#include <string>
#include <vector>
#include <memory>

namespace gameengine {

/**
 * Cryptographic utilities using OpenSSL.
 */
class CryptoProvider {
public:
    CryptoProvider();
    ~CryptoProvider();
    
    // Hashing
    std::string sha256(const std::vector<uint8_t>& data);
    std::string sha256(const std::string& data);
    
    // Random
    std::vector<uint8_t> random_bytes(size_t count);
    std::string random_hex(size_t bytes);
    
    // Base64
    std::string base64_encode(const std::vector<uint8_t>& data);
    std::vector<uint8_t> base64_decode(const std::string& encoded);
    
    // AES-256-GCM encryption
    struct EncryptedData {
        std::vector<uint8_t> ciphertext;
        std::vector<uint8_t> iv;
        std::vector<uint8_t> tag;
    };
    
    EncryptedData encrypt(const std::vector<uint8_t>& plaintext,
                          const std::vector<uint8_t>& key);
    
    std::vector<uint8_t> decrypt(const EncryptedData& encrypted,
                                  const std::vector<uint8_t>& key);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gameengine
