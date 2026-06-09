#include "ollama_proxy.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

struct ParsedUrl {
    std::string host;
    int port = 11434;
    std::string pathPrefix;
};

ParsedUrl parseBaseUrl(const std::string &url) {
    ParsedUrl out;
    std::string u = url;
    if (u.rfind("http://", 0) == 0)
        u = u.substr(7);
    else if (u.rfind("https://", 0) == 0)
        throw std::runtime_error("HTTPS not supported in minimal proxy; use http://");

    auto slash = u.find('/');
    std::string hostPort = slash == std::string::npos ? u : u.substr(0, slash);
    if (slash != std::string::npos)
        out.pathPrefix = u.substr(slash);
    else
        out.pathPrefix = "";

    auto colon = hostPort.find(':');
    if (colon == std::string::npos) {
        out.host = hostPort;
    } else {
        out.host = hostPort.substr(0, colon);
        out.port = std::stoi(hostPort.substr(colon + 1));
    }
    return out;
}

std::string httpRequest(const ParsedUrl &base, const std::string &method,
                        const std::string &path, const std::string &body) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        throw std::runtime_error("WSAStartup failed");
#endif

    struct addrinfo hints {}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    std::string portStr = std::to_string(base.port);
    if (getaddrinfo(base.host.c_str(), portStr.c_str(), &hints, &res) != 0)
        throw std::runtime_error("DNS resolve failed for " + base.host);

    int sock = static_cast<int>(socket(res->ai_family, res->ai_socktype, res->ai_protocol));
    if (sock < 0) {
        freeaddrinfo(res);
        throw std::runtime_error("socket() failed");
    }

    if (connect(sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
        freeaddrinfo(res);
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        throw std::runtime_error("connect() failed to " + base.host);
    }
    freeaddrinfo(res);

    std::string fullPath = base.pathPrefix + path;
    if (fullPath.empty() || fullPath[0] != '/')
        fullPath = "/" + fullPath;

    std::ostringstream req;
    req << method << " " << fullPath << " HTTP/1.1\r\n";
    req << "Host: " << base.host << "\r\n";
    req << "Content-Type: application/json\r\n";
    req << "Accept: application/json\r\n";
    req << "Connection: close\r\n";
    req << "Content-Length: " << body.size() << "\r\n\r\n";
    req << body;

    std::string reqStr = req.str();
    size_t sent = 0;
    while (sent < reqStr.size()) {
#ifdef _WIN32
        int n = send(sock, reqStr.data() + sent, static_cast<int>(reqStr.size() - sent), 0);
#else
        ssize_t n = send(sock, reqStr.data() + sent, reqStr.size() - sent, 0);
#endif
        if (n <= 0)
            break;
        sent += static_cast<size_t>(n);
    }

    std::string response;
    char buf[4096];
    while (true) {
#ifdef _WIN32
        int n = recv(sock, buf, sizeof(buf), 0);
#else
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
#endif
        if (n <= 0)
            break;
        response.append(buf, static_cast<size_t>(n));
    }

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    auto bodyStart = response.find("\r\n\r\n");
    if (bodyStart == std::string::npos)
        return response;
    return response.substr(bodyStart + 4);
}

} // namespace

OllamaProxy::OllamaProxy(std::string baseUrl) : baseUrl_(std::move(baseUrl)) {}

void OllamaProxy::setBaseUrl(const std::string &baseUrl) { baseUrl_ = baseUrl; }

void OllamaProxy::handleRequest(const std::string &requestJson, ResponseCallback cb) {
    try {
        auto j = nlohmann::json::parse(requestJson);
        std::string method = j.value("method", "POST");
        std::string path = j.value("path", "/api/generate");
        std::string body;
        if (j.contains("body")) {
            if (j["body"].is_string())
                body = j["body"].get<std::string>();
            else
                body = j["body"].dump();
        }

        auto parsed = parseBaseUrl(baseUrl_);
        std::string responseBody = httpRequest(parsed, method, path, body);

        nlohmann::json envelope;
        envelope["type"] = "ollama_response";
        envelope["ok"] = true;
        envelope["body"] = responseBody;
        cb(true, envelope.dump());
    } catch (const std::exception &e) {
        nlohmann::json envelope;
        envelope["type"] = "ollama_response";
        envelope["ok"] = false;
        envelope["error"] = e.what();
        cb(false, envelope.dump());
    }
}
