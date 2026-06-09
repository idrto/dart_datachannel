# Developer Guide

This document covers building, integrating, and extending **flutter_datachannel**.

## Table of contents

1. [Repository layout](#repository-layout)
2. [Prerequisites](#prerequisites)
3. [Building](#building)
4. [Signaling server](#signaling-server)
5. [Standalone server service](#standalone-server-service)
6. [Flutter integration](#flutter-integration)
7. [Dart API reference](#dart-api-reference)
8. [C FFI reference](#c-ffi-reference)
9. [Ollama request protocol](#ollama-request-protocol)
10. [Mode enforcement](#mode-enforcement)
11. [Platform notes](#platform-notes)
12. [Troubleshooting](#troubleshooting)

---

## Repository layout

```
flutter_datachannel/
├── lib/                    # Dart package (pub.dev ready)
│   ├── flutter_datachannel.dart
│   └── src/
│       ├── datachannel_client.dart   # Client-only facade
│       ├── datachannel_server.dart   # Server-only facade
│       ├── datachannel_hybrid.dart   # Hybrid facade
│       ├── datachannel_engine.dart   # Core FFI wrapper
│       └── ffi/
│           ├── bindings.g.dart       # Generated (ffigen)
│           └── native_library.dart
├── native/                 # C++ FFI over libdatachannel
│   ├── include/fdc_ffi.h
│   ├── src/
│   └── server/src/main.cpp # Standalone fdc-server binary
├── tools/
│   └── signaling_server.py # Reference signaling broker
├── linux/ windows/ macos/ android/ ios/  # Flutter plugin CMake
├── docs/
├── example/
└── scripts/build_native.sh
```

---

## Prerequisites

### All platforms

- CMake ≥ 3.16
- C++17 compiler (GCC, Clang, MSVC)
- OpenSSL development headers
- Git (for FetchContent dependencies)

### Flutter development

- Flutter SDK ≥ 3.24
- Dart SDK ≥ 3.5

### Signaling (Python reference server)

```bash
pip install -r tools/requirements.txt
```

### Ollama (server machine)

```bash
# macOS / Linux
curl -fsSL https://ollama.com/install.sh | sh
ollama serve   # listens on http://127.0.0.1:11434
```

---

## Building

### Native library + server binary

```bash
./scripts/build_native.sh
```

Artifacts:

| File | Purpose |
|------|---------|
| `native/build/libflutter_datachannel.so` | Linux shared lib |
| `native/build/libflutter_datachannel.dylib` | macOS shared lib |
| `native/build/flutter_datachannel.dll` | Windows DLL |
| `native/build/fdc-server` | Standalone server-only service |

### Flutter plugin (builds native code automatically)

```bash
cd example
flutter pub get
flutter run -d linux   # or macos, windows, android, ios
```

### Regenerate Dart FFI bindings

When `native/include/fdc_ffi.h` changes:

```bash
dart pub get
dart run ffigen --config ffigen.yaml
```

---

## Signaling server

WebRTC requires a signaling channel to exchange SDP offers/answers and ICE candidates. This repo ships a minimal Python broker.

```bash
python3 tools/signaling_server.py --host 0.0.0.0 --port 8765
```

### Protocol (JSON over WebSocket)

| Message | Direction | Payload |
|---------|-----------|---------|
| `register` | client → server | `{type, peer_id, role: client\|server\|hybrid}` |
| `registered` | server → client | `{type, peer_id}` |
| `list_peers` | client → server | `{type, from}` |
| `peers` | server → client | `{type, servers: [peer_id, ...]}` |
| `connect_request` | client → server → target | `{type, from, to}` |
| `offer` / `answer` | bidirectional relay | `{type, from, to, sdp}` |
| `candidate` | bidirectional relay | `{type, from, to, candidate, mid}` |

The signaling server does **not** carry application data — only session negotiation.

---

## Standalone server service

`fdc-server` runs **without Flutter**. Use it on desktop machines hosting Ollama.

```bash
./native/build/fdc-server \
  --signaling ws://192.168.1.10:8765 \
  --peer-id m4-ollama \
  --ollama http://127.0.0.1:11434 \
  --stun stun:stun.l.google.com:19302
```

### systemd example (Linux)

```ini
[Unit]
Description=FDC Ollama WebRTC Server
After=network.target ollama.service

[Service]
ExecStart=/usr/local/bin/fdc-server \
  --signaling ws://192.168.1.1:8765 \
  --peer-id prod-ollama \
  --ollama http://127.0.0.1:11434
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

### launchd example (macOS)

Create `~/Library/LaunchAgents/com.idrto.fdc-server.plist` pointing to the binary and signaling URL.

---

## Flutter integration

### pubspec.yaml (Git dependency)

```yaml
dependencies:
  flutter_datachannel:
    git:
      url: https://github.com/idrto/flutter_datachannel
      ref: main
```

### Client-only (mobile / laptop)

```dart
final client = DataChannelClient(
  signalingUrl: 'ws://192.168.1.10:8765',
  peerId: 'iphone-12',
);

client.onRuntimeState.listen((e) => print('state: ${e.state}'));
client.onPeerState.listen((e) => print('peer ${e.peerId}: ${e.state}'));
client.onPeersUpdated.listen((e) {
  print('servers: ${parseServerPeers(e)}');
});

await client.start();
await client.refreshServers();
await client.connectToServer('m4-ollama');

await client.queryOllama('m4-ollama', body: {
  'model': 'llama3',
  'prompt': 'Explain WebRTC in one sentence',
  'stream': false,
});
```

### Server-only (inside Flutter — desktop)

For embedding server mode in a Flutter desktop app:

```dart
final server = DataChannelServer(
  signalingUrl: 'ws://127.0.0.1:8765',
  peerId: 'desktop-server',
  ollamaUrl: 'http://127.0.0.1:11434',
);
await server.start();
```

For production headless deployment, prefer `fdc-server` instead.

### Hybrid

```dart
final node = DataChannelHybrid(
  signalingUrl: 'ws://192.168.1.10:8765',
  peerId: 'edge-node',
  ollamaUrl: 'http://127.0.0.1:11434',
);
await node.start();
await node.connect('remote-server');
```

### Resource cleanup

Always call `dispose()` when done:

```dart
await client.dispose();
```

---

## Dart API reference

### `DataChannelClient`

| Method | Description |
|--------|-------------|
| `start()` | Connect to signaling |
| `stop()` | Disconnect |
| `refreshServers()` | Fetch server peer list |
| `connectToServer(id)` | Initiate WebRTC to server |
| `queryOllama(id, body:)` | Send Ollama API request |
| `sendText(id, text)` | Send raw text |
| `disconnect(id)` | Close peer connection |
| `dispose()` | Release native resources |

### Streams

| Stream | Event |
|--------|-------|
| `onRuntimeState` | `RuntimeStateEvent` |
| `onPeerState` | `PeerStateEvent` |
| `onMessage` | `PeerMessageEvent` |
| `onPeersUpdated` | `PeersUpdatedEvent` |
| `onLog` | `NativeLogEvent` |

### Helpers

- `parseServerPeers(PeersUpdatedEvent)` → `List<String>`
- `parseOllamaResponse(PeerMessageEvent)` → `Map<String, dynamic>?`

---

## C FFI reference

Header: `native/include/fdc_ffi.h`

```c
FdcContext *fdc_create(const FdcConfig *config);
void fdc_set_callbacks(FdcContext *ctx, FdcCallbacks cbs, void *user_data);
int fdc_start(FdcContext *ctx);
int fdc_connect(FdcContext *ctx, const char *remote_peer_id);  // client/hybrid
int fdc_send_ollama_request(FdcContext *ctx, const char *peer_id,
                            const char *method, const char *path,
                            const char *body_json);
int fdc_stop(FdcContext *ctx);
void fdc_destroy(FdcContext *ctx);
```

Error codes: `FDC_OK` (0) through `FDC_ERR_INTERNAL` (-10).  
Mode denial returns `FDC_ERR_MODE_DENIED` (-5).

---

## Ollama request protocol

Application messages use JSON envelopes over the `"ollama"` data channel.

### Client → Server

```json
{
  "type": "ollama_request",
  "method": "POST",
  "path": "/api/generate",
  "body": {
    "model": "llama3",
    "prompt": "Hello",
    "stream": false
  }
}
```

### Server → Client

```json
{
  "type": "ollama_response",
  "ok": true,
  "body": "{ ... raw Ollama JSON response ... }"
}
```

On error:

```json
{
  "type": "ollama_response",
  "ok": false,
  "error": "connect() failed to 127.0.0.1"
}
```

---

## Mode enforcement

| Operation | Client | Server | Hybrid |
|-----------|--------|--------|--------|
| `fdc_connect` | ✅ | ❌ `FDC_ERR_MODE_DENIED` | ✅ |
| `fdc_refresh_peers` | ✅ | ❌ | ✅ |
| Accept incoming SDP | ❌ | ✅ | ✅ |
| Ollama proxy | ❌ | ✅ | ✅ |

---

## Platform notes

### Linux

- Requires `libssl-dev` (OpenSSL)
- Flutter bundles `libflutter_datachannel.so` via `linux/CMakeLists.txt`

### macOS

- Xcode command-line tools required
- Code signing may be needed for iOS; macOS desktop works with ad-hoc signing

### Windows

- Visual Studio 2019+ with C++ workload
- OpenSSL: install via vcpkg or prebuilt binaries; CMake must find OpenSSL

### Android

- `minSdk 24`
- NDK r25+ recommended
- Uses `c++_shared` STL

### iOS

- Static linking of `fdc_ffi` into app binary
- Background modes: WebRTC may need appropriate entitlements for persistent connections

### NAT / TURN

STUN alone works on many LAN setups. For remote access across restrictive NAT, configure TURN:

```dart
DataChannelClient(
  signalingUrl: 'wss://signal.example.com',
  peerId: 'remote-client',
  turnServer: 'turn:turn.example.com:3478',
  turnUsername: 'user',
  turnPassword: 'pass',
);
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| `fdc_create failed` | Missing `peerId` or `signalingUrl` | Check config |
| Stuck in `signaling` state | Signaling server unreachable | Verify URL, firewall |
| `FDC_ERR_MODE_DENIED` | Wrong mode for operation | Use correct facade |
| ICE failed | NAT traversal | Add TURN server |
| Ollama proxy error | Ollama not running | `ollama serve` on server |
| Library not found (Flutter) | Native build failed | Rebuild app; check CMake logs |

Enable verbose logging:

```dart
DataChannelClient(..., verboseLogging: true);
```

Or for `fdc-server`:

```bash
fdc-server --verbose ...
```

---

## Contributing

1. Change `native/include/fdc_ffi.h`
2. Run `dart run ffigen --config ffigen.yaml`
3. Update Dart wrappers in `lib/src/`
4. Run `./scripts/build_native.sh` and `flutter test`
5. Update docs in `docs/`

## webrtc.rs (future)

This release uses **libdatachannel** only. A future `FDC_BACKEND=webrtc-rs` switch will expose the same `fdc_ffi.h` surface with an alternate Rust implementation. The Dart API will remain unchanged.
