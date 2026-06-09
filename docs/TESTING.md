# Testing Guide

Platform test matrix for **flutter_datachannel** v0.1 (libdatachannel backend).

## Test environment setup

### Shared infrastructure

1. **Signaling server** on a LAN-accessible host:
   ```bash
   python3 tools/signaling_server.py --host 0.0.0.0 --port 8765
   ```

2. **Ollama** on the server machine:
   ```bash
   ollama serve
   ollama pull llama3
   ```

3. **fdc-server** on the Ollama host:
   ```bash
   ./native/build/fdc-server \
     --signaling ws://SIGNAL_HOST:8765 \
     --peer-id test-server \
     --ollama http://127.0.0.1:11434
   ```

4. Note your LAN IP for client `signalingUrl`.

---

## Server-only tests (desktop)

**Required platforms:** Windows, macOS, Linux

| # | Test | Pass criteria |
|---|------|---------------|
| S1 | Start `fdc-server` | Logs `ready`, registered on signaling |
| S2 | Client lists peers | `peers` contains `test-server` |
| S3 | Ollama generate | Client receives `ollama_response` with `ok: true` |
| S4 | Graceful shutdown | Ctrl+C closes peers cleanly |
| S5 | Ollama offline | `ollama_response` with `ok: false` |
| S6 | systemd / launchd | Service restarts after kill |

### Per-OS notes

**Linux**
```bash
./scripts/build_native.sh
./native/build/fdc-server --signaling ws://127.0.0.1:8765 --peer-id linux-srv
```

**macOS (including M4)**
```bash
./scripts/build_native.sh
./native/build/fdc-server --signaling ws://LAN_IP:8765 --peer-id m4-ollama
```

**Windows (PowerShell)**
```powershell
cmake -B native\build -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build native\build --config Release
.\native\build\Release\fdc-server.exe --signaling ws://LAN_IP:8765 --peer-id win-srv
```

---

## Client-only tests

**Required platforms:** iOS, Android, Windows, macOS, Linux

| # | Test | Pass criteria |
|---|------|---------------|
| C1 | `DataChannelClient.start()` | State → `ready` |
| C2 | `refreshServers()` | `onPeersUpdated` lists server |
| C3 | `connectToServer()` | Peer state → `connected` |
| C4 | `queryOllama()` | Valid JSON in `onMessage` |
| C5 | `dispose()` | No crash on exit |
| C6 | Mode denial | Server facade cannot `connect()` (N/A for client) |
| C7 | Reconnect | Disconnect + reconnect works |

### Example integration test (Linux client)

```bash
cd example
flutter run -d linux
# Use UI to connect and send prompt
```

### iOS

- Run on physical device (Simulator has limited UDP/WebRTC)
- Add local network permission in Info.plist if using LAN signaling
- Test on cellular + Wi‑Fi

### Android

- `minSdk 24` device or emulator
- Ensure `INTERNET` permission (Flutter default)

---

## Hybrid tests

| # | Test | Pass criteria |
|---|------|---------------|
| H1 | Start hybrid node | Registers as `hybrid` |
| H2 | Outgoing connect | Can connect to remote server |
| H3 | Incoming accept | Remote client can connect to hybrid |
| H4 | Ollama proxy | Local Ollama proxied when acting as server |

---

## Optional mobile server tests

Running `fdc-server` or `DataChannelServer` on mobile is not required but supported for experimentation.

| Platform | Feasibility |
|----------|-------------|
| Android | Build via NDK cross-compile; background restrictions apply |
| iOS | Not recommended — background + App Store policy |

---

## Automated tests

```bash
# Dart unit tests (no native lib required)
flutter test

# Native build smoke test
./scripts/build_native.sh
test -f native/build/fdc-server
```

---

## Manual end-to-end checklist (Ollama on M4)

- [ ] Signaling running on LAN
- [ ] `fdc-server` on M4 with `--peer-id m4-ollama`
- [ ] Linux laptop client connects and generates text
- [ ] Windows laptop client connects
- [ ] Android phone on same Wi‑Fi connects
- [ ] iPhone on same Wi‑Fi connects
- [ ] macOS Flutter client connects

---

## webrtc.rs backend (future)

When the alternate backend lands, re-run this entire matrix with:

```
cmake -DFDC_BACKEND=webrtc-rs ...
```

Dart API and test cases should pass unchanged.

---

## Reporting issues

Include:
- Platform + OS version
- `fdc-server --verbose` or `verboseLogging: true` logs
- Signaling server logs
- ICE failure? STUN/TURN config used

File at: https://github.com/idrto/flutter_datachannel/issues
