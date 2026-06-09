/// Runtime mode for [DataChannelEngine].
enum DataChannelMode {
  /// Outgoing connections only (mobile / laptop clients).
  client(0),

  /// Registers with signaling and accepts incoming connections (desktop server).
  server(1),

  /// Both client and server capabilities.
  hybrid(2);

  const DataChannelMode(this.value);
  final int value;

  static DataChannelMode fromValue(int value) => DataChannelMode.values.firstWhere(
        (m) => m.value == value,
        orElse: () => DataChannelMode.client,
      );
}

/// Top-level engine state mirrored from native [FdcRuntimeState].
enum DataChannelRuntimeState {
  stopped(0),
  starting(1),
  signaling(2),
  ready(3),
  error(4);

  const DataChannelRuntimeState(this.value);
  final int value;

  static DataChannelRuntimeState fromValue(int value) =>
      DataChannelRuntimeState.values.firstWhere(
        (s) => s.value == value,
        orElse: () => DataChannelRuntimeState.stopped,
      );
}

/// Per-peer connection state.
enum DataChannelPeerState {
  disconnected(0),
  connecting(1),
  connected(2),
  failed(3);

  const DataChannelPeerState(this.value);
  final int value;

  static DataChannelPeerState fromValue(int value) =>
      DataChannelPeerState.values.firstWhere(
        (s) => s.value == value,
        orElse: () => DataChannelPeerState.disconnected,
      );
}

/// Configuration passed to native layer when creating a session.
class DataChannelConfig {
  const DataChannelConfig({
    required this.mode,
    required this.signalingUrl,
    required this.peerId,
    this.stunServer = 'stun:stun.l.google.com:19302',
    this.turnServer,
    this.turnUsername,
    this.turnPassword,
    this.ollamaUrl = 'http://127.0.0.1:11434',
    this.dataChannelLabel = 'ollama',
    this.verboseLogging = false,
  });

  final DataChannelMode mode;
  final String signalingUrl;
  final String peerId;
  final String stunServer;
  final String? turnServer;
  final String? turnUsername;
  final String? turnPassword;

  /// Used in server/hybrid mode to proxy Ollama requests locally.
  final String ollamaUrl;
  final String dataChannelLabel;
  final bool verboseLogging;

  DataChannelConfig copyWith({
    DataChannelMode? mode,
    String? signalingUrl,
    String? peerId,
    String? stunServer,
    String? turnServer,
    String? turnUsername,
    String? turnPassword,
    String? ollamaUrl,
    String? dataChannelLabel,
    bool? verboseLogging,
  }) {
    return DataChannelConfig(
      mode: mode ?? this.mode,
      signalingUrl: signalingUrl ?? this.signalingUrl,
      peerId: peerId ?? this.peerId,
      stunServer: stunServer ?? this.stunServer,
      turnServer: turnServer ?? this.turnServer,
      turnUsername: turnUsername ?? this.turnUsername,
      turnPassword: turnPassword ?? this.turnPassword,
      ollamaUrl: ollamaUrl ?? this.ollamaUrl,
      dataChannelLabel: dataChannelLabel ?? this.dataChannelLabel,
      verboseLogging: verboseLogging ?? this.verboseLogging,
    );
  }
}

/// Native error codes returned by FFI calls.
abstract final class DataChannelError {
  static const int ok = 0;
  static const int invalidArg = -1;
  static const int notInitialized = -2;
  static const int alreadyRunning = -3;
  static const int notRunning = -4;
  static const int modeDenied = -5;
  static const int peerNotFound = -6;
  static const int channelClosed = -7;
  static const int sendFailed = -8;
  static const int signaling = -9;
  static const int internal = -10;
}
