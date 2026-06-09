/**
 * fdc-server — standalone server-only WebRTC + Ollama proxy service.
 *
 * Runs without Flutter. Connects to a signaling server, accepts client
 * connections, and proxies Ollama API requests from remote peers.
 *
 * Usage:
 *   fdc-server --signaling ws://localhost:8765 --peer-id my-m4-server \
 *              --ollama http://127.0.0.1:11434
 */

#include "fdc_ffi.h"

#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

namespace {

std::atomic<bool> gRunning{true};

void onSignal(int) { gRunning = false; }

void printUsage(const char *prog) {
    std::fprintf(stderr,
                "Usage: %s [options]\n"
                "\n"
                "Options:\n"
                "  --signaling URL    WebSocket signaling URL (required)\n"
                "  --peer-id ID       Unique server peer ID (required)\n"
                "  --ollama URL       Ollama base URL (default: http://127.0.0.1:11434)\n"
                "  --stun URI         STUN server (default: stun:stun.l.google.com:19302)\n"
                "  --turn URI         TURN server (optional)\n"
                "  --turn-user USER   TURN username (optional)\n"
                "  --turn-pass PASS   TURN password (optional)\n"
                "  --verbose          Enable verbose logging\n"
                "  -h, --help         Show this help\n",
                prog);
}

struct CliConfig {
    std::string signaling = "ws://127.0.0.1:8765";
    std::string peerId = "fdc-server";
    std::string ollama = "http://127.0.0.1:11434";
    std::string stun = "stun:stun.l.google.com:19302";
    std::string turn;
    std::string turnUser;
    std::string turnPass;
    bool verbose = false;
};

CliConfig parseArgs(int argc, char **argv) {
    CliConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 < argc)
                return argv[++i];
            return {};
        };
        if (arg == "--signaling")
            cfg.signaling = next();
        else if (arg == "--peer-id")
            cfg.peerId = next();
        else if (arg == "--ollama")
            cfg.ollama = next();
        else if (arg == "--stun")
            cfg.stun = next();
        else if (arg == "--turn")
            cfg.turn = next();
        else if (arg == "--turn-user")
            cfg.turnUser = next();
        else if (arg == "--turn-pass")
            cfg.turnPass = next();
        else if (arg == "--verbose")
            cfg.verbose = true;
        else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        } else {
            std::fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
            printUsage(argv[0]);
            std::exit(1);
        }
    }
    return cfg;
}

void onRuntimeState(void *, FdcRuntimeState state, const char *message) {
    const char *name = "unknown";
    switch (state) {
    case FDC_STATE_STOPPED:
        name = "stopped";
        break;
    case FDC_STATE_STARTING:
        name = "starting";
        break;
    case FDC_STATE_SIGNALING:
        name = "signaling";
        break;
    case FDC_STATE_READY:
        name = "ready";
        break;
    case FDC_STATE_ERROR:
        name = "error";
        break;
    }
    std::printf("[runtime] %s", name);
    if (message && message[0])
        std::printf(" — %s", message);
    std::printf("\n");
}

void onPeerState(void *, const char *peerId, FdcPeerState state) {
    const char *name = "unknown";
    switch (state) {
    case FDC_PEER_DISCONNECTED:
        name = "disconnected";
        break;
    case FDC_PEER_CONNECTING:
        name = "connecting";
        break;
    case FDC_PEER_CONNECTED:
        name = "connected";
        break;
    case FDC_PEER_FAILED:
        name = "failed";
        break;
    }
    std::printf("[peer] %s → %s\n", peerId, name);
}

void onMessage(void *, const char *peerId, const uint8_t *data, size_t len) {
    std::string preview(reinterpret_cast<const char *>(data),
                        len > 120 ? 120 : len);
    std::printf("[message] from %s (%zu bytes): %s%s\n", peerId, len,
                preview.c_str(), len > 120 ? "…" : "");
}

void onLog(void *, int level, const char *message) {
    if (level <= 3)
        std::printf("[log] %s\n", message);
}

} // namespace

int main(int argc, char **argv) {
    CliConfig cli = parseArgs(argc, argv);

    if (cli.signaling.empty() || cli.peerId.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    std::signal(SIGINT, onSignal);
#ifndef _WIN32
    std::signal(SIGTERM, onSignal);
#endif

    fdc_init();

    FdcConfig config{};
    config.mode = FDC_MODE_SERVER;
    config.signaling_url = cli.signaling.c_str();
    config.peer_id = cli.peerId.c_str();
    config.stun_server = cli.stun.c_str();
    config.ollama_url = cli.ollama.c_str();
    config.data_channel_label = "ollama";
    config.verbose_logging = cli.verbose ? 1 : 0;
    if (!cli.turn.empty()) {
        config.turn_server = cli.turn.c_str();
        config.turn_username = cli.turnUser.c_str();
        config.turn_password = cli.turnPass.c_str();
    }

    FdcContext *ctx = fdc_create(&config);
    if (!ctx) {
        std::fprintf(stderr, "Failed to create FDC context\n");
        return 1;
    }

    FdcCallbacks cbs{};
    cbs.on_runtime_state = onRuntimeState;
    cbs.on_peer_state = onPeerState;
    cbs.on_message = onMessage;
    cbs.on_log = onLog;
    fdc_set_callbacks(ctx, cbs, nullptr);

    std::printf("fdc-server v%s\n", fdc_version());
    std::printf("  signaling : %s\n", cli.signaling.c_str());
    std::printf("  peer-id   : %s\n", cli.peerId.c_str());
    std::printf("  ollama    : %s\n", cli.ollama.c_str());
    std::printf("Press Ctrl+C to stop.\n\n");

    if (fdc_start(ctx) != FDC_OK) {
        char err[512] = {};
        fdc_get_last_error(ctx, err, sizeof(err));
        std::fprintf(stderr, "Failed to start: %s\n", err);
        fdc_destroy(ctx);
        return 1;
    }

    while (gRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::printf("\nShutting down…\n");
    fdc_stop(ctx);
    fdc_destroy(ctx);
    fdc_cleanup();
    return 0;
}
