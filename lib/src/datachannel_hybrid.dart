import 'datachannel_config.dart';
import 'datachannel_engine.dart';
import 'events/datachannel_events.dart';

/// Hybrid facade — both client and server capabilities in one process.
class DataChannelHybrid {
  DataChannelHybrid({
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
            mode: DataChannelMode.hybrid,
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
  Stream<PeersUpdatedEvent> get onPeersUpdated => _engine.onPeersUpdated;
  Stream<NativeLogEvent> get onLog => _engine.onLog;

  DataChannelRuntimeState get runtimeState => _engine.runtimeState;
  bool get isStarted => _engine.isStarted;

  Future<void> start() => _engine.start();
  Future<void> stop() => _engine.stop();
  Future<void> dispose() => _engine.dispose();

  Future<void> connect(String remotePeerId) => _engine.connect(remotePeerId);
  Future<void> refreshPeers() => _engine.refreshPeers();
  Future<void> queryOllama(
    String peerId, {
    required Map<String, dynamic> body,
    String path = '/api/generate',
  }) =>
      _engine.sendOllamaRequest(peerId, path: path, body: body);
  Future<void> sendText(String peerId, String text) =>
      _engine.sendText(peerId, text);
  Future<void> disconnect(String peerId) => _engine.disconnectPeer(peerId);
}
