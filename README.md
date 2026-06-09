# dart_datachannel

**Pure Dart** FFI package over [libdatachannel](https://github.com/paullouisageneau/libdatachannel) — no Flutter SDK dependency. Use from CLI, servers, or Flutter apps (Flutter consumers bundle the native `.so`/`.dylib`/`.dll` themselves; see [docs/FLUTTER_EMBEDDING.md](docs/FLUTTER_EMBEDDING.md)).

## Modes

| Mode | Dart API | Standalone | Description |
|------|----------|------------|-------------|
| **Client** | `DataChannelClient` | — | Initiate outgoing connections only |
| **Server** | `DataChannelServer` | `fdc-server` binary | Register with signaling, accept clients, proxy Ollama |
| **Hybrid** | `DataChannelHybrid` | — | Both client and server in one process |

## Quick start

### 1. Add dependency (GitHub import)

```yaml
dependencies:
  dart_datachannel:
    git:
      url: https://github.com/idrto/dart_datachannel
      ref: main
```

### 2. Start signaling

```bash
pip install -r tools/requirements.txt
python3 tools/signaling_server.py --port 8765
```

### 3. Run Ollama server (desktop — no Flutter)

On the machine running Ollama (e.g. M4 Mac):

```bash
# Build once
./scripts/build_native.sh

# Run server-only service
./native/build/fdc-server \
  --signaling ws://YOUR_LAN_IP:8765 \
  --peer-id m4-ollama \
  --ollama http://127.0.0.1:11434
```

### 4. Connect from Dart client (CLI — no Flutter)

```bash
./scripts/build_native.sh
dart run example/bin/client.dart \
  --signaling ws://127.0.0.1:8765 \
  --server m4-ollama \
  --prompt "Hello"
```

Or embed in your app — see [docs/FLUTTER_EMBEDDING.md](docs/FLUTTER_EMBEDDING.md) for Flutter.

## Architecture

```
┌─────────────┐     WebSocket      ┌──────────────────┐
│ Flutter     │◄────────────────►│ Signaling server │
│ Client      │   SDP / ICE      │ (tools/*.py)     │
└──────┬──────┘                  └────────┬─────────┘
       │ WebRTC P2P                       │
       │ Data Channel                     │
       ▼                                  ▼
┌─────────────┐                  ┌──────────────────┐
│ libdatachannel                  │ fdc-server       │
│ (FFI)       │                  │ (server-only)    │
└─────────────┘                  └────────┬─────────┘
                                          │ HTTP
                                          ▼
                                   ┌──────────────┐
                                   │ Ollama       │
                                   │ :11434       │
                                   └──────────────┘
```

## Documentation

- [Developer guide](docs/DEVELOPER.md) — setup, API, FFI, contributing
- [Architecture](docs/ARCHITECTURE.md) — design decisions and protocol
- [Testing matrix](docs/TESTING.md) — per-platform test plan

## Platform support

| Platform | Client | Server (`fdc-server`) | Dart package |
|----------|--------|----------------------|--------------|
| Linux | ✅ | ✅ | ✅ |
| macOS | ✅ | ✅ | ✅ |
| Windows | ✅ | ✅ | ✅ |
| Android | ✅ | optional | ✅ (bundle `.so`) |
| iOS | ✅ | optional | ✅ (static link) |

## Building native code

```bash
./scripts/build_native.sh
# Produces:
#   native/build/libdart_datachannel.so
#   native/build/fdc-server
```

Flutter apps must bundle the native library — see [docs/FLUTTER_EMBEDDING.md](docs/FLUTTER_EMBEDDING.md). Optional UI demo: `demos/flutter_example/`.

## Regenerate FFI bindings

After changing `native/include/fdc_ffi.h`:

```bash
dart run ffigen --config ffigen.yaml
```

## License

MIT — see [LICENSE](LICENSE).

## Roadmap

- [ ] webrtc.rs backend (alternate to libdatachannel)
- [ ] TURN deployment guide for NAT traversal
- [ ] Streaming Ollama responses over data channel chunks
- [ ] pub.dev publish
