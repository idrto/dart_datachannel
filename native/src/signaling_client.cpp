#include "signaling_client.hpp"

#include <rtc/rtc.hpp>

#include <atomic>
#include <memory>
#include <mutex>

struct SignalingClient::Impl {
    std::shared_ptr<rtc::WebSocket> ws;
    MessageHandler onMessage;
    StateHandler onState;
    std::mutex mutex;
    std::atomic<bool> connected{false};
};

SignalingClient::SignalingClient() : impl_(new Impl()) {}

SignalingClient::~SignalingClient() { disconnect(); delete impl_; }

void SignalingClient::setMessageHandler(MessageHandler handler) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->onMessage = std::move(handler);
}

void SignalingClient::setStateHandler(StateHandler handler) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->onState = std::move(handler);
}

bool SignalingClient::connect(const std::string &url, const std::string &peerId,
                              int mode) {
    disconnect();

    auto ws = std::make_shared<rtc::WebSocket>();
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->ws = ws;
    }

    ws->onOpen([this, peerId, mode]() {
        impl_->connected = true;
        const char *role = mode == 0 ? "client" : (mode == 1 ? "server" : "hybrid");
        std::string registerMsg = std::string(R"({"type":"register","peer_id":")") +
                                  peerId + R"(","role":")" + role + R"("})";
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            if (impl_->ws)
                impl_->ws->send(registerMsg);
        }
        StateHandler handler;
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            handler = impl_->onState;
        }
        if (handler)
            handler(true, "connected");
    });

    ws->onClosed([this]() {
        impl_->connected = false;
        StateHandler handler;
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            handler = impl_->onState;
        }
        if (handler)
            handler(false, "closed");
    });

    ws->onError([this](std::string err) {
        impl_->connected = false;
        StateHandler handler;
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            handler = impl_->onState;
        }
        if (handler)
            handler(false, err);
    });

    ws->onMessage([this](auto data) {
        MessageHandler handler;
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            handler = impl_->onMessage;
        }
        if (!handler)
            return;
        if (std::holds_alternative<std::string>(data)) {
            handler(std::get<std::string>(data));
        } else if (std::holds_alternative<rtc::binary>(data)) {
            const auto &bin = std::get<rtc::binary>(data);
            handler(std::string(reinterpret_cast<const char *>(bin.data()), bin.size()));
        }
    });

    try {
        ws->open(url);
        return true;
    } catch (const std::exception &e) {
        StateHandler handler;
        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            handler = impl_->onState;
        }
        if (handler)
            handler(false, e.what());
        return false;
    }
}

void SignalingClient::disconnect() {
    std::shared_ptr<rtc::WebSocket> ws;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        ws = std::move(impl_->ws);
        impl_->connected = false;
    }
    if (ws) {
        try {
            ws->close();
        } catch (...) {
        }
    }
}

bool SignalingClient::isConnected() const { return impl_->connected.load(); }

bool SignalingClient::sendJson(const std::string &json) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->ws || !impl_->connected)
        return false;
    try {
        impl_->ws->send(json);
        return true;
    } catch (...) {
        return false;
    }
}
