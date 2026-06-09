#include "fdc_ffi.h"

#include "ollama_proxy.hpp"
#include "signaling_client.hpp"

#include <rtc/rtc.hpp>

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

std::once_flag gInitFlag;
std::atomic<int> gInstanceCount{0};

void ensureInit() {
    std::call_once(gInitFlag, [] {
        rtc::InitLogger(rtc::LogLevel::Warning);
    });
}

const char *modeName(FdcMode mode) {
    switch (mode) {
    case FDC_MODE_CLIENT:
        return "client";
    case FDC_MODE_SERVER:
        return "server";
    case FDC_MODE_HYBRID:
        return "hybrid";
    }
    return "unknown";
}

bool modeAllowsClient(FdcMode mode) {
    return mode == FDC_MODE_CLIENT || mode == FDC_MODE_HYBRID;
}

bool modeAllowsServer(FdcMode mode) {
    return mode == FDC_MODE_SERVER || mode == FDC_MODE_HYBRID;
}

} // namespace

struct PeerEntry {
    std::string peerId;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel> dc;
    FdcPeerState state = FDC_PEER_DISCONNECTED;
    bool isInitiator = false;
};

struct FdcContext {
    FdcConfig config{};
    FdcCallbacks callbacks{};
    void *userData = nullptr;

    std::mutex mutex;
    FdcRuntimeState runtimeState = FDC_STATE_STOPPED;
    std::string lastError;

    SignalingClient signaling;
    std::unique_ptr<OllamaProxy> ollamaProxy;

    std::map<std::string, PeerEntry> peers;
    std::vector<std::string> iceServersStorage;
    std::vector<const char *> iceServerPtrs;

    void setError(const std::string &msg) {
        lastError = msg;
        emitRuntime(FDC_STATE_ERROR, msg);
    }

    void emitRuntime(FdcRuntimeState state, const std::string &msg = {}) {
        runtimeState = state;
        FdcOnRuntimeState cb = callbacks.on_runtime_state;
        if (cb)
            cb(userData, state, msg.empty() ? nullptr : msg.c_str());
    }

    void emitPeer(const std::string &peerId, FdcPeerState state) {
        FdcOnPeerState cb = callbacks.on_peer_state;
        if (cb)
            cb(userData, peerId.c_str(), state);
    }

    void emitMessage(const std::string &peerId, const std::string &payload) {
        FdcOnMessage cb = callbacks.on_message;
        if (cb)
            cb(userData, peerId.c_str(),
               reinterpret_cast<const uint8_t *>(payload.data()), payload.size());
    }

    void emitLog(int level, const std::string &msg) {
        FdcOnLog cb = callbacks.on_log;
        if (cb)
            cb(userData, level, msg.c_str());
    }

    rtc::Configuration buildRtcConfig() {
        iceServersStorage.clear();
        iceServerPtrs.clear();

        if (config.stun_server && config.stun_server[0]) {
            iceServersStorage.push_back(config.stun_server);
        }
        if (config.turn_server && config.turn_server[0]) {
            std::string turn = config.turn_server;
            if (config.turn_username && config.turn_username[0]) {
                turn += "?transport=udp&username=";
                turn += config.turn_username;
                if (config.turn_password && config.turn_password[0]) {
                    turn += "&password=";
                    turn += config.turn_password;
                }
            }
            iceServersStorage.push_back(turn);
        }
        if (iceServersStorage.empty()) {
            iceServersStorage.push_back("stun:stun.l.google.com:19302");
        }

        for (const auto &s : iceServersStorage)
            iceServerPtrs.push_back(s.c_str());
        iceServerPtrs.push_back(nullptr);

        rtc::Configuration rtcConfig;
        for (const char *srv : iceServerPtrs) {
            if (srv)
                rtcConfig.iceServers.emplace_back(srv);
        }
        return rtcConfig;
    }

    std::string channelLabel() const {
        if (config.data_channel_label && config.data_channel_label[0])
            return config.data_channel_label;
        return "ollama";
    }

    void setupDataChannel(PeerEntry &entry, std::shared_ptr<rtc::DataChannel> dc) {
        entry.dc = dc;
        const std::string peerId = entry.peerId;

        dc->onOpen([this, peerId]() {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = peers.find(peerId);
            if (it != peers.end()) {
                it->second.state = FDC_PEER_CONNECTED;
                emitPeer(peerId, FDC_PEER_CONNECTED);
            }
        });

        dc->onClosed([this, peerId]() {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = peers.find(peerId);
            if (it != peers.end()) {
                it->second.state = FDC_PEER_DISCONNECTED;
                emitPeer(peerId, FDC_PEER_DISCONNECTED);
            }
        });

        dc->onMessage([this, peerId](auto data) {
            std::string payload;
            if (std::holds_alternative<std::string>(data))
                payload = std::get<std::string>(data);
            else {
                const auto &bin = std::get<rtc::binary>(data);
                payload.assign(reinterpret_cast<const char *>(bin.data()), bin.size());
            }

            if (modeAllowsServer(config.mode) && ollamaProxy) {
                try {
                    auto j = nlohmann::json::parse(payload);
                    if (j.value("type", "") == "ollama_request") {
                        std::string reqDump = j.dump();
                        ollamaProxy->handleRequest(reqDump, [this, peerId](bool, const std::string &resp) {
                            std::lock_guard<std::mutex> lock(mutex);
                            auto it = peers.find(peerId);
                            if (it != peers.end() && it->second.dc && it->second.dc->isOpen()) {
                                it->second.dc->send(resp);
                            }
                            emitMessage(peerId, resp);
                        });
                        return;
                    }
                } catch (...) {
                }
            }
            emitMessage(peerId, payload);
        });
    }

    void wirePeerConnection(PeerEntry &entry) {
        const std::string peerId = entry.peerId;
        auto pc = entry.pc;

        pc->onLocalDescription([this, peerId](rtc::Description desc) {
            nlohmann::json msg;
            msg["type"] = desc.typeString();
            msg["from"] = config.peer_id ? config.peer_id : "";
            msg["to"] = peerId;
            msg["sdp"] = std::string(desc);
            signaling.sendJson(msg.dump());
        });

        pc->onLocalCandidate([this, peerId](rtc::Candidate cand) {
            nlohmann::json msg;
            msg["type"] = "candidate";
            msg["from"] = config.peer_id ? config.peer_id : "";
            msg["to"] = peerId;
            msg["candidate"] = std::string(cand);
            msg["mid"] = cand.mid();
            signaling.sendJson(msg.dump());
        });

        pc->onStateChange([this, peerId](rtc::PeerConnection::State state) {
            if (state == rtc::PeerConnection::State::Failed) {
                std::lock_guard<std::mutex> lock(mutex);
                emitPeer(peerId, FDC_PEER_FAILED);
            }
        });

        pc->onDataChannel([this, peerId](std::shared_ptr<rtc::DataChannel> dc) {
            std::lock_guard<std::mutex> lock(mutex);
            auto it = peers.find(peerId);
            if (it != peers.end())
                setupDataChannel(it->second, dc);
        });
    }

    PeerEntry *createPeer(const std::string &peerId, bool initiator) {
        auto rtcConfig = buildRtcConfig();
        PeerEntry entry;
        entry.peerId = peerId;
        entry.isInitiator = initiator;
        entry.state = FDC_PEER_CONNECTING;
        entry.pc = std::make_shared<rtc::PeerConnection>(rtcConfig);

        wirePeerConnection(entry);

        if (initiator) {
            auto dc = entry.pc->createDataChannel(channelLabel());
            setupDataChannel(entry, dc);
        }

        peers[peerId] = entry;
        emitPeer(peerId, FDC_PEER_CONNECTING);
        return &peers[peerId];
    }

    void handleSignalingMessage(const std::string &json) {
        nlohmann::json msg;
        try {
            msg = nlohmann::json::parse(json);
        } catch (...) {
            return;
        }

        const std::string type = msg.value("type", "");
        if (type == "peers") {
            FdcOnPeersUpdated cb = callbacks.on_peers_updated;
            if (cb)
                cb(userData, json.c_str());
            return;
        }

        if (type == "offer" || type == "answer") {
            if (!modeAllowsServer(config.mode))
                return;

            std::string remoteId = msg.value("from", "");
            std::string sdp = msg.value("sdp", "");
            if (remoteId.empty() || sdp.empty())
                return;

            std::lock_guard<std::mutex> lock(mutex);
            PeerEntry *entry = nullptr;
            auto it = peers.find(remoteId);
            if (it == peers.end()) {
                entry = createPeer(remoteId, false);
            } else {
                entry = &it->second;
            }

            rtc::Description desc(sdp, type == "offer" ? rtc::Description::Type::Offer
                                                       : rtc::Description::Type::Answer);
            entry->pc->setRemoteDescription(desc);
            return;
        }

        if (type == "candidate") {
            std::string remoteId = msg.value("from", "");
            std::string candStr = msg.value("candidate", "");
            std::string mid = msg.value("mid", "");
            if (remoteId.empty() || candStr.empty())
                return;

            std::lock_guard<std::mutex> lock(mutex);
            auto it = peers.find(remoteId);
            if (it == peers.end())
                return;
            rtc::Candidate cand(candStr, mid);
            it->second.pc->addRemoteCandidate(cand);
            return;
        }

        if (type == "connect_request") {
            // Client asked signaling to notify us — auto-accept by waiting for their offer
            emitLog(3, "Incoming connect request from " + msg.value("from", ""));
            return;
        }
    }
};

extern "C" {

const char *fdc_version(void) { return "0.1.0"; }

void fdc_init(void) { ensureInit(); }

void fdc_cleanup(void) {
    // libdatachannel cleans up per PeerConnection; global cleanup is optional
}

FdcContext *fdc_create(const FdcConfig *config) {
    if (!config || !config->peer_id || !config->peer_id[0] || !config->signaling_url ||
        !config->signaling_url[0])
        return nullptr;

    ensureInit();
    gInstanceCount++;

    auto *ctx = new FdcContext();
    ctx->config = *config;

    if (config->verbose_logging)
        rtc::InitLogger(rtc::LogLevel::Debug);

    if (modeAllowsServer(config->mode)) {
        std::string ollamaUrl = "http://127.0.0.1:11434";
        if (config->ollama_url && config->ollama_url[0])
            ollamaUrl = config->ollama_url;
        ctx->ollamaProxy = std::make_unique<OllamaProxy>(ollamaUrl);
    }

    ctx->signaling.setStateHandler([ctx](bool connected, const std::string &reason) {
        std::lock_guard<std::mutex> lock(ctx->mutex);
        if (connected) {
            ctx->emitRuntime(FDC_STATE_READY, "signaling connected");
        } else {
            ctx->emitRuntime(FDC_STATE_SIGNALING, reason);
        }
    });

    ctx->signaling.setMessageHandler([ctx](const std::string &json) {
        ctx->handleSignalingMessage(json);
    });

    return ctx;
}

void fdc_destroy(FdcContext *ctx) {
    if (!ctx)
        return;
    fdc_stop(ctx);
    delete ctx;
    gInstanceCount--;
}

void fdc_set_callbacks(FdcContext *ctx, FdcCallbacks callbacks, void *user_data) {
    if (!ctx)
        return;
    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->callbacks = callbacks;
    ctx->userData = user_data;
}

int fdc_start(FdcContext *ctx) {
    if (!ctx)
        return FDC_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(ctx->mutex);
    if (ctx->runtimeState != FDC_STATE_STOPPED && ctx->runtimeState != FDC_STATE_ERROR)
        return FDC_ERR_ALREADY_RUNNING;

    ctx->emitRuntime(FDC_STATE_STARTING, "starting");

    if (!ctx->signaling.connect(ctx->config.signaling_url, ctx->config.peer_id,
                                static_cast<int>(ctx->config.mode))) {
        ctx->setError("failed to connect to signaling server");
        return FDC_ERR_SIGNALING;
    }

    ctx->emitRuntime(FDC_STATE_SIGNALING, "connecting to signaling");
    return FDC_OK;
}

int fdc_stop(FdcContext *ctx) {
    if (!ctx)
        return FDC_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(ctx->mutex);
    ctx->signaling.disconnect();
    for (auto &kv : ctx->peers) {
        if (kv.second.dc)
            kv.second.dc->close();
        if (kv.second.pc)
            kv.second.pc->close();
    }
    ctx->peers.clear();
    ctx->emitRuntime(FDC_STATE_STOPPED, "stopped");
    return FDC_OK;
}

FdcRuntimeState fdc_get_runtime_state(FdcContext *ctx) {
    if (!ctx)
        return FDC_STATE_STOPPED;
    std::lock_guard<std::mutex> lock(ctx->mutex);
    return ctx->runtimeState;
}

FdcMode fdc_get_mode(FdcContext *ctx) {
    if (!ctx)
        return FDC_MODE_CLIENT;
    return ctx->config.mode;
}

int fdc_connect(FdcContext *ctx, const char *remote_peer_id) {
    if (!ctx || !remote_peer_id || !remote_peer_id[0])
        return FDC_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(ctx->mutex);
    if (!modeAllowsClient(ctx->config.mode))
        return FDC_ERR_MODE_DENIED;

    if (!ctx->signaling.isConnected())
        return FDC_ERR_NOT_RUNNING;

    std::string remoteId(remote_peer_id);
    if (ctx->peers.count(remoteId))
        return FDC_OK;

    ctx->createPeer(remoteId, true);

    nlohmann::json req;
    req["type"] = "connect_request";
    req["from"] = ctx->config.peer_id ? ctx->config.peer_id : "";
    req["to"] = remoteId;
    ctx->signaling.sendJson(req.dump());

    return FDC_OK;
}

int fdc_refresh_peers(FdcContext *ctx) {
    if (!ctx)
        return FDC_ERR_INVALID_ARG;
    std::lock_guard<std::mutex> lock(ctx->mutex);
    if (!modeAllowsClient(ctx->config.mode))
        return FDC_ERR_MODE_DENIED;
    if (!ctx->signaling.isConnected())
        return FDC_ERR_NOT_RUNNING;

    nlohmann::json req;
    req["type"] = "list_peers";
    req["from"] = ctx->config.peer_id ? ctx->config.peer_id : "";
    return ctx->signaling.sendJson(req.dump()) ? FDC_OK : FDC_ERR_SIGNALING;
}

int fdc_send(FdcContext *ctx, const char *peer_id, const uint8_t *data, size_t len) {
    if (!ctx || !peer_id || !data || len == 0)
        return FDC_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(ctx->mutex);
    auto it = ctx->peers.find(peer_id);
    if (it == ctx->peers.end())
        return FDC_ERR_PEER_NOT_FOUND;
    if (!it->second.dc || !it->second.dc->isOpen())
        return FDC_ERR_CHANNEL_CLOSED;

    try {
        it->second.dc->send(reinterpret_cast<const std::byte *>(data), len);
        return FDC_OK;
    } catch (...) {
        return FDC_ERR_SEND_FAILED;
    }
}

int fdc_send_text(FdcContext *ctx, const char *peer_id, const char *text) {
    if (!text)
        return FDC_ERR_INVALID_ARG;
    return fdc_send(ctx, peer_id, reinterpret_cast<const uint8_t *>(text), std::strlen(text));
}

int fdc_send_ollama_request(FdcContext *ctx, const char *peer_id, const char *method,
                            const char *path, const char *body_json) {
    if (!ctx || !peer_id)
        return FDC_ERR_INVALID_ARG;

    nlohmann::json envelope;
    envelope["type"] = "ollama_request";
    envelope["method"] = method ? method : "POST";
    envelope["path"] = path ? path : "/api/generate";
    if (body_json && body_json[0]) {
        try {
            envelope["body"] = nlohmann::json::parse(body_json);
        } catch (...) {
            envelope["body"] = body_json;
        }
    } else {
        envelope["body"] = nlohmann::json::object();
    }

    std::string payload = envelope.dump();
    return fdc_send(ctx, peer_id, reinterpret_cast<const uint8_t *>(payload.data()),
                    payload.size());
}

int fdc_disconnect_peer(FdcContext *ctx, const char *peer_id) {
    if (!ctx || !peer_id)
        return FDC_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(ctx->mutex);
    auto it = ctx->peers.find(peer_id);
    if (it == ctx->peers.end())
        return FDC_ERR_PEER_NOT_FOUND;

    if (it->second.dc)
        it->second.dc->close();
    if (it->second.pc)
        it->second.pc->close();
    ctx->peers.erase(it);
    ctx->emitPeer(peer_id, FDC_PEER_DISCONNECTED);
    return FDC_OK;
}

int fdc_get_last_error(FdcContext *ctx, char *buffer, size_t size) {
    if (!ctx || !buffer || size == 0)
        return FDC_ERR_INVALID_ARG;
    std::lock_guard<std::mutex> lock(ctx->mutex);
    std::strncpy(buffer, ctx->lastError.c_str(), size - 1);
    buffer[size - 1] = '\0';
    return FDC_OK;
}

} // extern "C"
