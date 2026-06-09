# flutter_datachannel

Thin FFI layer over [libdatachannel](https://github.com/paullouisageneau/libdatachannel) for Flutter. Connect clients on **iOS, Android, Windows, macOS, and Linux** to a desktop **Ollama server** over WebRTC data channels.

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
  flutter_datachannel:
    git:
      url: https://github.com/idrto/flutter_datachannel
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

### 4. Connect from Flutter client

```dart
import 'package:flutter_datachannel/flutter_datachannel.dart';

final client = DataChannelClient(
  signalingUrl: 'ws://YOUR_LAN_IP:8765',
  peerId: 'laptop-client',
);

await client.start();
await client.refreshServers();
await client.connectToServer('m4-ollama');

client.onMessage.listen((event) {
  final response = parseOllamaResponse(event);
  if (response != null) print(response['body']);
});

await client.queryOllama('m4-ollama', body: {
  'model': 'llama3',
  'prompt': 'Hello from Flutter',
  'stream': false,
});
```

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

| Platform | Client | Server (`fdc-server`) | Flutter plugin |
|----------|--------|----------------------|----------------|
| Linux | ✅ | ✅ | ✅ |
| macOS | ✅ | ✅ | ✅ |
| Windows | ✅ | ✅ | ✅ |
| Android | ✅ | optional | ✅ |
| iOS | ✅ | optional | ✅ |

## Building native code

```bash
./scripts/build_native.sh
# Produces:
#   native/build/libflutter_datachannel.so
#   native/build/fdc-server
```

Flutter apps build the shared library automatically via the plugin CMake hooks.

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
