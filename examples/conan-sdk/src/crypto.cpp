#include "sdk/crypto.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

namespace gameengine {

struct CryptoProvider::Impl {
    // No state needed for now
};

CryptoProvider::CryptoProvider() : impl_(std::make_unique<Impl>()) {
    spdlog::debug("CryptoProvider initialized");
}

CryptoProvider::~CryptoProvider() = default;

std::string CryptoProvider::sha256(const std::vector<uint8_t>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
    
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();
}

std::string CryptoProvider::sha256(const std::string& data) {
    return sha256(std::vector<uint8_t>(data.begin(), data.end()));
}

std::vector<uint8_t> CryptoProvider::random_bytes(size_t count) {
    std::vector<uint8_t> bytes(count);
    if (RAND_bytes(bytes.data(), count) != 1) {
        spdlog::error("Failed to generate random bytes");
        return {};
    }
    return bytes;
}

std::string CryptoProvider::random_hex(size_t bytes) {
    auto data = random_bytes(bytes);
    std::ostringstream oss;
    for (uint8_t b : data) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return oss.str();
}

std::string CryptoProvider::base64_encode(const std::vector<uint8_t>& data) {
    static const char* chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    result.reserve((data.size() + 2) / 3 * 4);
    
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < data.size()) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < data.size()) n |= static_cast<uint32_t>(data[i + 2]);
        
        result += chars[(n >> 18) & 0x3F];
        result += chars[(n >> 12) & 0x3F];
        result += (i + 1 < data.size()) ? chars[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < data.size()) ? chars[n & 0x3F] : '=';
    }
    
    return result;
}

std::vector<uint8_t> CryptoProvider::base64_decode(const std::string& encoded) {
    static const int decode_table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    std::vector<uint8_t> result;
    result.reserve(encoded.size() * 3 / 4);
    
    uint32_t n = 0;
    int bits = 0;
    
    for (char c : encoded) {
        if (c == '=') break;
        int val = decode_table[static_cast<unsigned char>(c)];
        if (val < 0) continue;
        
        n = (n << 6) | val;
        bits += 6;
        
        if (bits >= 8) {
            bits -= 8;
            result.push_back((n >> bits) & 0xFF);
        }
    }
    
    return result;
}

CryptoProvider::EncryptedData CryptoProvider::encrypt(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key) {
    
    EncryptedData result;
    result.iv = random_bytes(12);  // GCM standard IV size
    result.tag.resize(16);
    result.ciphertext.resize(plaintext.size());
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return result;
    
    EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), result.iv.data());
    
    int len;
    EVP_EncryptUpdate(ctx, result.ciphertext.data(), &len, 
                      plaintext.data(), plaintext.size());
    
    EVP_EncryptFinal_ex(ctx, result.ciphertext.data() + len, &len);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, result.tag.data());
    
    EVP_CIPHER_CTX_free(ctx);
    return result;
}

std::vector<uint8_t> CryptoProvider::decrypt(
    const EncryptedData& encrypted,
    const std::vector<uint8_t>& key) {
    
    std::vector<uint8_t> plaintext(encrypted.ciphertext.size());
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
    EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), encrypted.iv.data());
    
    int len;
    EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                      encrypted.ciphertext.data(), encrypted.ciphertext.size());
    
    EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, 
                        const_cast<uint8_t*>(encrypted.tag.data()));
    
    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);
    
    if (ret <= 0) {
        spdlog::error("Decryption failed - authentication error");
        return {};
    }
    
    return plaintext;
}

} // namespace gameengine
