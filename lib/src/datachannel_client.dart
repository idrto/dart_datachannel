import 'datachannel_config.dart';
import 'datachannel_engine.dart';
import 'events/datachannel_events.dart';

/// Client-only facade — can initiate outgoing connections but never accepts them.
///
/// Use on iOS, Android, Windows, macOS, and Linux to connect to a remote
/// Ollama server (e.g. an M4 Mac running [DataChannelServer] or `fdc-server`).
class DataChannelClient {
  DataChannelClient({
    required String signalingUrl,
    required String peerId,
    String stunServer = 'stun:stun.l.google.com:19302',
    String? turnServer,
    String? turnUsername,
    String? turnPassword,
    bool verboseLogging = false,
  }) : _engine = DataChannelEngine.create(
          DataChannelConfig(
            mode: DataChannelMode.client,
            signalingUrl: signalingUrl,
            peerId: peerId,
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

  /// Connect to a remote server peer and open a data channel.
  Future<void> connectToServer(String serverPeerId) =>
      _engine.connect(serverPeerId);

  /// Ask signaling for the current list of registered servers.
  Future<void> refreshServers() => _engine.refreshPeers();

  /// Send an Ollama generate/chat request to the connected server.
  Future<void> queryOllama(
    String serverPeerId, {
    required Map<String, dynamic> body,
    String path = '/api/generate',
  }) =>
      _engine.sendOllamaRequest(serverPeerId, path: path, body: body);

  Future<void> sendText(String peerId, String text) =>
      _engine.sendText(peerId, text);

  Future<void> disconnect(String peerId) => _engine.disconnectPeer(peerId);
}
