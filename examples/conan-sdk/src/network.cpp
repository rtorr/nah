#include "sdk/network.hpp"

#include <curl/curl.h>
#include <spdlog/spdlog.h>
#include <deque>
#include <mutex>

namespace gameengine {

struct NetworkManager::Impl {
    CURLM* multi_handle = nullptr;
    std::mutex pending_mutex;
    
    struct PendingRequest {
        CURL* handle;
        ResponseCallback callback;
        std::vector<uint8_t> response_data;
        std::map<std::string, std::string> response_headers;
    };
    std::deque<PendingRequest> pending;
};

static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* data = static_cast<std::vector<uint8_t>*>(userdata);
    size_t total = size * nmemb;
    data->insert(data->end(), ptr, ptr + total);
    return total;
}

NetworkManager::NetworkManager() : impl_(std::make_unique<Impl>()) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    impl_->multi_handle = curl_multi_init();
    spdlog::debug("NetworkManager initialized");
}

NetworkManager::~NetworkManager() {
    if (impl_->multi_handle) {
        curl_multi_cleanup(impl_->multi_handle);
    }
    curl_global_cleanup();
}

HttpResponse NetworkManager::request(const HttpRequest& req) {
    HttpResponse response;
    response.status_code = 0;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        response.error = "Failed to initialize curl";
        return response;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, req.timeout_ms);
    
    if (!req.verify_ssl) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    struct curl_slist* headers = nullptr;
    for (const auto& [key, value] : req.headers) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    if (req.method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req.body.size());
    } else if (req.method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, req.body.size());
    } else if (req.method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        response.error = curl_easy_strerror(res);
    } else {
        long code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        response.status_code = static_cast<int>(code);
    }
    
    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);
    
    return response;
}

HttpResponse NetworkManager::get(const std::string& url) {
    HttpRequest req;
    req.url = url;
    req.method = "GET";
    return request(req);
}

HttpResponse NetworkManager::post(const std::string& url, const std::string& body,
                                   const std::string& content_type) {
    HttpRequest req;
    req.url = url;
    req.method = "POST";
    req.body = std::vector<uint8_t>(body.begin(), body.end());
    req.headers["Content-Type"] = content_type;
    return request(req);
}

void NetworkManager::request_async(const HttpRequest& req, ResponseCallback callback) {
    // Simplified: just run sync in this example
    // Real implementation would use curl_multi
    auto response = request(req);
    callback(std::move(response));
}

void NetworkManager::poll() {
    // Process pending async requests
    // In a real implementation, this would call curl_multi_perform
}

} // namespace gameengine
