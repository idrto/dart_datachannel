import 'datachannel_config.dart';
import 'datachannel_engine.dart';
import 'events/datachannel_events.dart';

/// Server-only facade — registers with signaling and accepts incoming connections.
///
/// Proxies Ollama API requests from remote clients to the local Ollama instance.
/// For headless deployment without Flutter, use the `fdc-server` binary instead
/// (see `docs/DEVELOPER.md`).
class DataChannelServer {
  DataChannelServer({
    required String signalingUrl,
    required String peerId,
    String ollamaUrl = 'http://127.0.0.1:11434',
    String stunServer = 'stun:stun.l.google.com:19302',
    String? turnServer,
    String? turnUsername,
    String? turnPassword,
    bool verboseLogging = false,
  }) : _engine = DataChannelEngine.create(
          DataChannelConfig(
            mode: DataChannelMode.server,
            signalingUrl: signalingUrl,
            peerId: peerId,
            ollamaUrl: ollamaUrl,
            stunServer: stunServer,
            turnServer: turnServer,
            turnUsername: turnUsername,
            turnPassword: turnPassword,
            verboseLogging: verboseLogging,
          ),
        );

  final DataChannelEngine _engine;

  Stream<RuntimeStateEvent> get onRuntimeState => _engine.onRuntimeState;
  Stream<PeerStateEvent> get onPeerState => _engine.onPeerState;
  Stream<PeerMessageEvent> get onMessage => _engine.onMessage;
  Stream<NativeLogEvent> get onLog => _engine.onLog;

  DataChannelRuntimeState get runtimeState => _engine.runtimeState;
  bool get isStarted => _engine.isStarted;

  Future<void> start() => _engine.start();
  Future<void> stop() => _engine.stop();
  Future<void> dispose() => _engine.dispose();

  Future<void> disconnectClient(String clientPeerId) =>
      _engine.disconnectPeer(clientPeerId);
}
