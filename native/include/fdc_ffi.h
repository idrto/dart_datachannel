/**
 * dart_datachannel — thin C FFI layer over libdatachannel.
 *
 * Supports three runtime modes:
 *   FDC_MODE_CLIENT  — initiate outgoing connections only
 *   FDC_MODE_SERVER  — register with signaling, accept incoming connections
 *   FDC_MODE_HYBRID  — both client and server capabilities
 */

#ifndef FDC_FFI_H
#define FDC_FFI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#if defined(FDC_FFI_EXPORTS)
#define FDC_API __declspec(dllexport)
#else
#define FDC_API __declspec(dllimport)
#endif
#else
#define FDC_API __attribute__((visibility("default")))
#endif

/* ── Version ─────────────────────────────────────────────────────────── */

#define FDC_VERSION_MAJOR 0
#define FDC_VERSION_MINOR 1
#define FDC_VERSION_PATCH 0

FDC_API const char *fdc_version(void);

/* ── Error codes ─────────────────────────────────────────────────────── */

#define FDC_OK 0
#define FDC_ERR_INVALID_ARG -1
#define FDC_ERR_NOT_INITIALIZED -2
#define FDC_ERR_ALREADY_RUNNING -3
#define FDC_ERR_NOT_RUNNING -4
#define FDC_ERR_MODE_DENIED -5       /* operation not allowed in current mode */
#define FDC_ERR_PEER_NOT_FOUND -6
#define FDC_ERR_CHANNEL_CLOSED -7
#define FDC_ERR_SEND_FAILED -8
#define FDC_ERR_SIGNALING -9
#define FDC_ERR_INTERNAL -10

/* ── Runtime mode ────────────────────────────────────────────────────── */

typedef enum {
    FDC_MODE_CLIENT = 0,
    FDC_MODE_SERVER = 1,
    FDC_MODE_HYBRID = 2
} FdcMode;

/* ── Connection / session state ──────────────────────────────────────── */

typedef enum {
    FDC_STATE_STOPPED = 0,
    FDC_STATE_STARTING = 1,
    FDC_STATE_SIGNALING = 2,
    FDC_STATE_READY = 3,
    FDC_STATE_ERROR = 4
} FdcRuntimeState;

typedef enum {
    FDC_PEER_DISCONNECTED = 0,
    FDC_PEER_CONNECTING = 1,
    FDC_PEER_CONNECTED = 2,
    FDC_PEER_FAILED = 3
} FdcPeerState;

/* ── Configuration ───────────────────────────────────────────────────── */

typedef struct {
    FdcMode mode;

    /** WebSocket URL of the signaling server, e.g. ws://host:8765 */
    const char *signaling_url;

    /** Unique peer identifier (required) */
    const char *peer_id;

    /** STUN server URI, e.g. stun:stun.l.google.com:19302 */
    const char *stun_server;

    /** Optional TURN server URI */
    const char *turn_server;

    /** Optional TURN username */
    const char *turn_username;

    /** Optional TURN password */
    const char *turn_password;

    /**
     * Base URL for Ollama on the server machine.
     * Used in FDC_MODE_SERVER and FDC_MODE_HYBRID when proxying requests.
     * Default: http://127.0.0.1:11434
     */
    const char *ollama_url;

    /** Label for the primary data channel. Default: "ollama" */
    const char *data_channel_label;

    /** Enable verbose libdatachannel logging */
    int verbose_logging;
} FdcConfig;

/* ── Event callbacks ───────────────────────────────────────────────────── */

/**
 * Called when the runtime state changes.
 * `message` may be NULL; valid only for the duration of the callback.
 */
typedef void (*FdcOnRuntimeState)(void *user_data, FdcRuntimeState state,
                                  const char *message);

/** Called when a remote peer's connection state changes. */
typedef void (*FdcOnPeerState)(void *user_data, const char *peer_id,
                               FdcPeerState state);

/**
 * Called when a data-channel message arrives from `peer_id`.
 * `data` is valid only for the duration of the callback.
 */
typedef void (*FdcOnMessage)(void *user_data, const char *peer_id,
                             const uint8_t *data, size_t len);

/** Called when the list of available server peers is updated (client/hybrid). */
typedef void (*FdcOnPeersUpdated)(void *user_data, const char *peers_json);

/** Called for log lines from the native layer. */
typedef void (*FdcOnLog)(void *user_data, int level, const char *message);

typedef struct {
    FdcOnRuntimeState on_runtime_state;
    FdcOnPeerState on_peer_state;
    FdcOnMessage on_message;
    FdcOnPeersUpdated on_peers_updated;
    FdcOnLog on_log;
} FdcCallbacks;

/* ── Opaque handle ─────────────────────────────────────────────────────── */

typedef struct FdcContext FdcContext;

/* ── Lifecycle ─────────────────────────────────────────────────────────── */

/** Create a context. Returns NULL on invalid config. */
FDC_API FdcContext *fdc_create(const FdcConfig *config);

/** Destroy context; stops if still running. */
FDC_API void fdc_destroy(FdcContext *ctx);

/**
 * Register event callbacks. Must be called before fdc_start().
 * Pass NULL for individual callbacks you do not need.
 */
FDC_API void fdc_set_callbacks(FdcContext *ctx, FdcCallbacks callbacks,
                               void *user_data);

/** Connect to signaling and begin accepting / discovering peers. */
FDC_API int fdc_start(FdcContext *ctx);

/** Disconnect all peers and leave signaling. */
FDC_API int fdc_stop(FdcContext *ctx);

/** Current runtime state. */
FDC_API FdcRuntimeState fdc_get_runtime_state(FdcContext *ctx);

/** Configured mode. */
FDC_API FdcMode fdc_get_mode(FdcContext *ctx);

/* ── Client / hybrid operations ──────────────────────────────────────── */

/**
 * Initiate an outgoing connection to `remote_peer_id`.
 * Allowed in FDC_MODE_CLIENT and FDC_MODE_HYBRID only.
 */
FDC_API int fdc_connect(FdcContext *ctx, const char *remote_peer_id);

/**
 * Request refreshed list of available server peers from signaling.
 * Allowed in FDC_MODE_CLIENT and FDC_MODE_HYBRID only.
 */
FDC_API int fdc_refresh_peers(FdcContext *ctx);

/* ── Messaging ─────────────────────────────────────────────────────────── */

/**
 * Send raw bytes to a connected peer over the primary data channel.
 */
FDC_API int fdc_send(FdcContext *ctx, const char *peer_id, const uint8_t *data,
                     size_t len);

/** Convenience wrapper for UTF-8 text. */
FDC_API int fdc_send_text(FdcContext *ctx, const char *peer_id,
                          const char *text);

/**
 * Send an Ollama API request (HTTP-style envelope) to a server peer.
 * The server proxies this to its local Ollama instance and responds asynchronously
 * via the on_message callback.
 *
 * Example path: "/api/generate"
 * Example body: {"model":"llama3","prompt":"Hello","stream":false}
 */
FDC_API int fdc_send_ollama_request(FdcContext *ctx, const char *peer_id,
                                    const char *method, const char *path,
                                    const char *body_json);

/** Disconnect a specific peer. */
FDC_API int fdc_disconnect_peer(FdcContext *ctx, const char *peer_id);

/* ── Utilities ─────────────────────────────────────────────────────────── */

/** Last error message for this context; NULL if none. Thread-safe copy into buffer. */
FDC_API int fdc_get_last_error(FdcContext *ctx, char *buffer, size_t size);

/** Initialize global libdatachannel resources (optional, called automatically). */
FDC_API void fdc_init(void);

/** Release global libdatachannel resources. */
FDC_API void fdc_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* FDC_FFI_H */
