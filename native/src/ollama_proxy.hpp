#pragma once

#include <functional>
#include <string>

/** Proxies Ollama HTTP requests received over a data channel. */
class OllamaProxy {
public:
    using ResponseCallback =
        std::function<void(bool ok, const std::string &responseBody)>;

    explicit OllamaProxy(std::string baseUrl);

    void handleRequest(const std::string &requestJson, ResponseCallback cb);

    void setBaseUrl(const std::string &baseUrl);
    const std::string &baseUrl() const { return baseUrl_; }

private:
    std::string baseUrl_;
};
