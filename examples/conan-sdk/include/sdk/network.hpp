#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <map>

namespace gameengine {

/**
 * HTTP response.
 */
struct HttpResponse {
    int status_code;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    std::string error;
    
    bool ok() const { return status_code >= 200 && status_code < 300; }
    std::string body_string() const { 
        return std::string(body.begin(), body.end()); 
    }
};

/**
 * HTTP request options.
 */
struct HttpRequest {
    std::string url;
    std::string method = "GET";
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    int timeout_ms = 30000;
    bool verify_ssl = true;
};

/**
 * Network manager providing HTTP and WebSocket functionality.
 * Uses libcurl + OpenSSL internally.
 */
class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    
    // Synchronous HTTP
    HttpResponse request(const HttpRequest& req);
    
    // Convenience methods
    HttpResponse get(const std::string& url);
    HttpResponse post(const std::string& url, const std::string& body,
                      const std::string& content_type = "application/json");
    
    // Async HTTP
    using ResponseCallback = std::function<void(HttpResponse)>;
    void request_async(const HttpRequest& req, ResponseCallback callback);
    
    // Process pending async operations (call from main loop)
    void poll();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gameengine
