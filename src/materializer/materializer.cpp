#include "nah/materializer.hpp"
#include "nah/packaging.hpp"
#include "nah/platform.hpp"
#include "nah/nak_record.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>

#include <openssl/evp.h>
#include <curl/curl.h>

namespace fs = std::filesystem;

namespace nah {

// ============================================================================
// Reference Parsing
// ============================================================================

ParsedReference parse_artifact_reference(const std::string& reference) {
    ParsedReference result;
    
    if (reference.empty()) {
        result.error = "empty reference";
        return result;
    }
    
    // Check for file: scheme
    if (reference.rfind("file:", 0) == 0) {
        result.type = ReferenceType::File;
        result.path_or_url = reference.substr(5);  // Skip "file:"
        
        if (result.path_or_url.empty()) {
            result.type = ReferenceType::Invalid;
            result.error = "empty file path";
        }
        return result;
    }
    
    // Check for https: scheme
    if (reference.rfind("https://", 0) == 0) {
        result.type = ReferenceType::Https;
        
        // Find the fragment (#sha256=...)
        auto hash_pos = reference.find('#');
        if (hash_pos == std::string::npos) {
            result.type = ReferenceType::Invalid;
            result.error = "HTTPS reference must include #sha256=<hex> digest";
            return result;
        }
        
        result.path_or_url = reference.substr(0, hash_pos);
        std::string fragment = reference.substr(hash_pos + 1);
        
        // Parse sha256=<hex>
        if (fragment.rfind("sha256=", 0) != 0) {
            result.type = ReferenceType::Invalid;
            result.error = "fragment must be sha256=<hex>, got: " + fragment;
            return result;
        }
        
        result.sha256_digest = fragment.substr(7);  // Skip "sha256="
        
        // Validate hex digest (must be 64 lowercase hex chars)
        if (result.sha256_digest.size() != 64) {
            result.type = ReferenceType::Invalid;
            result.error = "SHA-256 digest must be 64 hex characters, got " + 
                          std::to_string(result.sha256_digest.size());
            return result;
        }
        
        for (char c : result.sha256_digest) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                result.type = ReferenceType::Invalid;
                result.error = "SHA-256 digest contains invalid character: " + 
                              std::string(1, c);
                return result;
            }
        }
        
        // Normalize to lowercase
        std::transform(result.sha256_digest.begin(), result.sha256_digest.end(),
                      result.sha256_digest.begin(), 
                      [](unsigned char c) { return std::tolower(c); });
        
        return result;
    }
    
    // Check for http: (not allowed per SPEC)
    if (reference.rfind("http://", 0) == 0) {
        result.type = ReferenceType::Invalid;
        result.error = "HTTP (non-TLS) references are not allowed, use HTTPS";
        return result;
    }
    
    // Unknown scheme
    result.type = ReferenceType::Invalid;
    result.error = "unsupported reference scheme, expected file: or https://";
    return result;
}

// ============================================================================
// SHA-256 Implementation (using OpenSSL 3.0+ EVP API)
// ============================================================================

namespace {

// RAII wrapper for EVP_MD_CTX
class EvpMdCtx {
public:
    EvpMdCtx() : ctx_(EVP_MD_CTX_new()) {}
    ~EvpMdCtx() { if (ctx_) EVP_MD_CTX_free(ctx_); }
    
    EvpMdCtx(const EvpMdCtx&) = delete;
    EvpMdCtx& operator=(const EvpMdCtx&) = delete;
    
    EVP_MD_CTX* get() { return ctx_; }
    explicit operator bool() const { return ctx_ != nullptr; }
    
private:
    EVP_MD_CTX* ctx_;
};

std::string bytes_to_hex(const unsigned char* data, size_t len) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string result;
    result.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        result.push_back(hex_chars[(data[i] >> 4) & 0x0F]);
        result.push_back(hex_chars[data[i] & 0x0F]);
    }
    return result;
}

} // namespace

HashResult compute_sha256(const std::vector<uint8_t>& data) {
    HashResult result;
    
    EvpMdCtx ctx;
    if (!ctx) {
        result.error = "EVP_MD_CTX_new failed";
        return result;
    }
    
    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        result.error = "EVP_DigestInit_ex failed";
        return result;
    }
    
    if (EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1) {
        result.error = "EVP_DigestUpdate failed";
        return result;
    }
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    
    if (EVP_DigestFinal_ex(ctx.get(), hash, &hash_len) != 1) {
        result.error = "EVP_DigestFinal_ex failed";
        return result;
    }
    
    result.hex_digest = bytes_to_hex(hash, hash_len);
    result.ok = true;
    return result;
}

HashResult compute_sha256(const std::string& file_path) {
    HashResult result;
    
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        result.error = "failed to open file: " + file_path;
        return result;
    }
    
    EvpMdCtx ctx;
    if (!ctx) {
        result.error = "EVP_MD_CTX_new failed";
        return result;
    }
    
    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        result.error = "EVP_DigestInit_ex failed";
        return result;
    }
    
    char buffer[8192];
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        if (EVP_DigestUpdate(ctx.get(), buffer, static_cast<size_t>(file.gcount())) != 1) {
            result.error = "EVP_DigestUpdate failed";
            return result;
        }
    }
    
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    
    if (EVP_DigestFinal_ex(ctx.get(), hash, &hash_len) != 1) {
        result.error = "EVP_DigestFinal_ex failed";
        return result;
    }
    
    result.hex_digest = bytes_to_hex(hash, hash_len);
    result.ok = true;
    return result;
}

Sha256VerifyResult verify_sha256(const std::vector<uint8_t>& data,
                                  const std::string& expected_hex) {
    Sha256VerifyResult result;
    result.expected_digest = expected_hex;
    
    // Normalize expected to lowercase
    std::string expected_lower = expected_hex;
    std::transform(expected_lower.begin(), expected_lower.end(),
                  expected_lower.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    
    auto hash_result = compute_sha256(data);
    if (!hash_result.ok) {
        result.error = hash_result.error;
        return result;
    }
    
    result.actual_digest = hash_result.hex_digest;
    
    if (hash_result.hex_digest != expected_lower) {
        result.error = "SHA-256 mismatch: expected " + expected_lower +
                      ", got " + hash_result.hex_digest;
        return result;
    }
    
    result.ok = true;
    return result;
}

// ============================================================================
// HTTP Fetching with libcurl
// ============================================================================

namespace {

// Callback for libcurl to write received data
size_t curl_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buffer = static_cast<std::vector<uint8_t>*>(userdata);
    size_t total = size * nmemb;
    buffer->insert(buffer->end(), ptr, ptr + total);
    return total;
}

// RAII wrapper for CURL handle
class CurlHandle {
public:
    CurlHandle() : handle_(curl_easy_init()) {}
    ~CurlHandle() { if (handle_) curl_easy_cleanup(handle_); }
    
    CurlHandle(const CurlHandle&) = delete;
    CurlHandle& operator=(const CurlHandle&) = delete;
    
    CURL* get() { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }
    
private:
    CURL* handle_;
};

// Global curl initialization (thread-safe in modern libcurl)
class CurlGlobalInit {
public:
    CurlGlobalInit() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobalInit() { curl_global_cleanup(); }
};

CurlGlobalInit& get_curl_init() {
    static CurlGlobalInit init;
    return init;
}

} // namespace

FetchResult fetch_https(const std::string& url) {
    FetchResult result;
    
    // Ensure global initialization
    get_curl_init();
    
    CurlHandle curl;
    if (!curl) {
        result.error = "failed to initialize CURL";
        return result;
    }
    
    std::vector<uint8_t> buffer;
    char error_buffer[CURL_ERROR_SIZE] = {0};
    
    // Set options
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, curl_write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, error_buffer);
    
    // Follow redirects
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 10L);
    
    // TLS verification (required per SPEC)
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Reasonable timeouts
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 300L);  // 5 minutes for large files
    
    // User agent
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "nah-materializer/1.0");
    
    // Perform request
    CURLcode res = curl_easy_perform(curl.get());
    
    if (res != CURLE_OK) {
        result.error = std::string("HTTP request failed: ") + 
                      (error_buffer[0] ? error_buffer : curl_easy_strerror(res));
        return result;
    }
    
    // Get response info
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &result.http_status);
    
    char* content_type = nullptr;
    curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_TYPE, &content_type);
    if (content_type) {
        result.content_type = content_type;
    }
    
    // Check HTTP status
    if (result.http_status < 200 || result.http_status >= 300) {
        result.error = "HTTP " + std::to_string(result.http_status);
        return result;
    }
    
    result.data = std::move(buffer);
    result.ok = true;
    return result;
}

} // namespace nah
