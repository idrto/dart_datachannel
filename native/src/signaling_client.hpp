#pragma once

#include <functional>
#include <mutex>
#include <string>

class SignalingClient {
public:
    using MessageHandler = std::function<void(const std::string &json)>;
    using StateHandler = std::function<void(bool connected, const std::string &reason)>;

    SignalingClient();
    ~SignalingClient();

    SignalingClient(const SignalingClient &) = delete;
    SignalingClient &operator=(const SignalingClient &) = delete;

    bool connect(const std::string &url, const std::string &peerId, int mode);
    void disconnect();
    bool isConnected() const;

    bool sendJson(const std::string &json);

    void setMessageHandler(MessageHandler handler);
    void setStateHandler(StateHandler handler);

private:
    struct Impl;
    Impl *impl_;
};
