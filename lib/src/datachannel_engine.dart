import 'dart:async';
import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:meta/meta.dart';

import 'datachannel_config.dart';
import 'events/datachannel_events.dart';
import 'ffi/bindings.g.dart';
import 'ffi/native_library.dart';

Pointer<Char> _cstr(String value) => value.toNativeUtf8().cast<Char>();

/// Core WebRTC data-channel engine backed by libdatachannel via FFI.
///
/// Use [DataChannelClient], [DataChannelServer], or [DataChannelHybrid]
/// for mode-specific facades.
class DataChannelEngine {
  DataChannelEngine._(this.config);

  final DataChannelConfig config;
  final FlutterDatachannelBindings _bindings =
      FlutterDatachannelBindings(nativeLibrary);

  Pointer<FdcContext>? _ctx;
  bool _started = false;

  final StreamController<RuntimeStateEvent> _runtimeController =
      StreamController<RuntimeStateEvent>.broadcast();
  final StreamController<PeerStateEvent> _peerController =
      StreamController<PeerStateEvent>.broadcast();
  final StreamController<PeerMessageEvent> _messageController =
      StreamController<PeerMessageEvent>.broadcast();
  final StreamController<PeersUpdatedEvent> _peersController =
      StreamController<PeersUpdatedEvent>.broadcast();
  final StreamController<NativeLogEvent> _logController =
      StreamController<NativeLogEvent>.broadcast();

  NativeCallable<FdcOnRuntimeStateFunction>? _runtimeCallable;
  NativeCallable<FdcOnPeerStateFunction>? _peerCallable;
  NativeCallable<FdcOnMessageFunction>? _messageCallable;
  NativeCallable<FdcOnPeersUpdatedFunction>? _peersCallable;
  NativeCallable<FdcOnLogFunction>? _logCallable;

  /// Factory for any mode.
  factory DataChannelEngine.create(DataChannelConfig config) {
    return DataChannelEngine._(config);
  }

  Stream<RuntimeStateEvent> get onRuntimeState => _runtimeController.stream;
  Stream<PeerStateEvent> get onPeerState => _peerController.stream;
  Stream<PeerMessageEvent> get onMessage => _messageController.stream;
  Stream<PeersUpdatedEvent> get onPeersUpdated => _peersController.stream;
  Stream<NativeLogEvent> get onLog => _logController.stream;

  DataChannelRuntimeState get runtimeState {
    final ctx = _ctx;
    if (ctx == null) return DataChannelRuntimeState.stopped;
    return _mapRuntimeState(_bindings.fdc_get_runtime_state(ctx));
  }

  static DataChannelRuntimeState _mapRuntimeState(FdcRuntimeState state) {
    return switch (state) {
      FdcRuntimeState.FDC_STATE_STOPPED => DataChannelRuntimeState.stopped,
      FdcRuntimeState.FDC_STATE_STARTING => DataChannelRuntimeState.starting,
      FdcRuntimeState.FDC_STATE_SIGNALING => DataChannelRuntimeState.signaling,
      FdcRuntimeState.FDC_STATE_READY => DataChannelRuntimeState.ready,
      FdcRuntimeState.FDC_STATE_ERROR => DataChannelRuntimeState.error,
    };
  }

  static DataChannelPeerState _mapPeerState(FdcPeerState state) {
    return switch (state) {
      FdcPeerState.FDC_PEER_DISCONNECTED => DataChannelPeerState.disconnected,
      FdcPeerState.FDC_PEER_CONNECTING => DataChannelPeerState.connecting,
      FdcPeerState.FDC_PEER_CONNECTED => DataChannelPeerState.connected,
      FdcPeerState.FDC_PEER_FAILED => DataChannelPeerState.failed,
    };
  }

  bool get isStarted => _started;

  /// Initialize native context and register callbacks. Idempotent.
  void initialize() {
    if (_ctx != null) return;

    _bindings.fdc_init();

    final nativeConfig = calloc<FdcConfig>();
    nativeConfig.ref
      ..modeAsInt = config.mode.value
      ..signaling_url = _cstr(config.signalingUrl)
      ..peer_id = _cstr(config.peerId)
      ..stun_server = _cstr(config.stunServer)
      ..turn_server = _cstr(config.turnServer ?? '')
      ..turn_username = _cstr(config.turnUsername ?? '')
      ..turn_password = _cstr(config.turnPassword ?? '')
      ..ollama_url = _cstr(config.ollamaUrl)
      ..data_channel_label = _cstr(config.dataChannelLabel)
      ..verbose_logging = config.verboseLogging ? 1 : 0;

    _ctx = _bindings.fdc_create(nativeConfig);

    calloc.free(nativeConfig.ref.signaling_url);
    calloc.free(nativeConfig.ref.peer_id);
    calloc.free(nativeConfig.ref.stun_server);
    calloc.free(nativeConfig.ref.turn_server);
    calloc.free(nativeConfig.ref.turn_username);
    calloc.free(nativeConfig.ref.turn_password);
    calloc.free(nativeConfig.ref.ollama_url);
    calloc.free(nativeConfig.ref.data_channel_label);
    calloc.free(nativeConfig);

    if (_ctx == nullptr) {
      throw StateError('fdc_create failed — check config (peerId, signalingUrl)');
    }

    _registerCallbacks();
  }

  void _registerCallbacks() {
    _runtimeCallable = NativeCallable<FdcOnRuntimeStateFunction>.listener(
      (Pointer<Void> _, int state, Pointer<Char> message) {
        _runtimeController.add(
          RuntimeStateEvent(
            _mapRuntimeState(FdcRuntimeState.fromValue(state)),
            message == nullptr ? null : message.cast<Utf8>().toDartString(),
          ),
        );
      },
    );

    _peerCallable = NativeCallable<FdcOnPeerStateFunction>.listener(
      (Pointer<Void> _, Pointer<Char> peerId, int state) {
        _peerController.add(
          PeerStateEvent(
            peerId.cast<Utf8>().toDartString(),
            _mapPeerState(FdcPeerState.fromValue(state)),
          ),
        );
      },
    );

    _messageCallable = NativeCallable<FdcOnMessageFunction>.listener(
      (Pointer<Void> _, Pointer<Char> peerId, Pointer<Uint8> data, int len) {
        final bytes = data.asTypedList(len);
        _messageController.add(
          PeerMessageEvent(peerId.cast<Utf8>().toDartString(), List<int>.from(bytes)),
        );
      },
    );

    _peersCallable = NativeCallable<FdcOnPeersUpdatedFunction>.listener(
      (Pointer<Void> _, Pointer<Char> json) {
        _peersController.add(PeersUpdatedEvent(json.cast<Utf8>().toDartString()));
      },
    );

    _logCallable = NativeCallable<FdcOnLogFunction>.listener(
      (Pointer<Void> _, int level, Pointer<Char> message) {
        _logController.add(
          NativeLogEvent(level, message.cast<Utf8>().toDartString()),
        );
      },
    );

    final callbacks = calloc<FdcCallbacks>();
    callbacks.ref
      ..on_runtime_state = _runtimeCallable!.nativeFunction
      ..on_peer_state = _peerCallable!.nativeFunction
      ..on_message = _messageCallable!.nativeFunction
      ..on_peers_updated = _peersCallable!.nativeFunction
      ..on_log = _logCallable!.nativeFunction;

    _bindings.fdc_set_callbacks(_ctx!, callbacks.ref, nullptr);
    calloc.free(callbacks);
  }

  /// Connect to signaling and begin operation.
  Future<void> start() async {
    initialize();
    final code = _bindings.fdc_start(_ctx!);
    _check(code, 'start');
    _started = true;
  }

  /// Stop and disconnect all peers.
  Future<void> stop() async {
    if (_ctx == null) return;
    _bindings.fdc_stop(_ctx!);
    _started = false;
  }

  /// Initiate outgoing connection (client / hybrid only).
  Future<void> connect(String remotePeerId) async {
    _ensureStarted();
    final id = _cstr(remotePeerId);
    final code = _bindings.fdc_connect(_ctx!, id);
    calloc.free(id);
    _check(code, 'connect');
  }

  /// Refresh available server peers from signaling (client / hybrid only).
  Future<void> refreshPeers() async {
    _ensureStarted();
    _check(_bindings.fdc_refresh_peers(_ctx!), 'refreshPeers');
  }

  /// Send raw bytes to a connected peer.
  Future<void> send(String peerId, List<int> data) async {
    _ensureStarted();
    final id = _cstr(peerId);
    final buffer = calloc<Uint8>(data.length);
    buffer.asTypedList(data.length).setAll(0, data);
    final code = _bindings.fdc_send(_ctx!, id, buffer, data.length);
    calloc.free(id);
    calloc.free(buffer);
    _check(code, 'send');
  }

  /// Send UTF-8 text to a connected peer.
  Future<void> sendText(String peerId, String text) async {
    _ensureStarted();
    final id = _cstr(peerId);
    final body = _cstr(text);
    final code = _bindings.fdc_send_text(_ctx!, id, body);
    calloc.free(id);
    calloc.free(body);
    _check(code, 'sendText');
  }

  /// Send an Ollama API request envelope to a server peer.
  Future<void> sendOllamaRequest(
    String peerId, {
    String method = 'POST',
    String path = '/api/generate',
    Map<String, dynamic>? body,
    String? bodyJson,
  }) async {
    _ensureStarted();
    final jsonBody = bodyJson ?? (body != null ? jsonEncode(body) : '{}');
    final id = _cstr(peerId);
    final m = _cstr(method);
    final p = _cstr(path);
    final b = _cstr(jsonBody);
    final code = _bindings.fdc_send_ollama_request(_ctx!, id, m, p, b);
    calloc.free(id);
    calloc.free(m);
    calloc.free(p);
    calloc.free(b);
    _check(code, 'sendOllamaRequest');
  }

  /// Disconnect a specific peer.
  Future<void> disconnectPeer(String peerId) async {
    if (_ctx == null) return;
    final id = _cstr(peerId);
    _bindings.fdc_disconnect_peer(_ctx!, id);
    calloc.free(id);
  }

  /// Last native error message, if any.
  String get lastError {
    if (_ctx == null) return '';
    final buf = calloc<Char>(512);
    _bindings.fdc_get_last_error(_ctx!, buf, 512);
    final msg = buf.cast<Utf8>().toDartString();
    calloc.free(buf);
    return msg;
  }

  void _ensureStarted() {
    if (!_started || _ctx == null) {
      throw StateError('Engine not started — call start() first');
    }
  }

  void _check(int code, String op) {
    if (code == DataChannelError.ok) return;
    throw DataChannelException(op, code, lastError);
  }

  /// Release native resources. Engine cannot be reused after dispose.
  @mustCallSuper
  Future<void> dispose() async {
    await stop();
    _runtimeCallable?.close();
    _peerCallable?.close();
    _messageCallable?.close();
    _peersCallable?.close();
    _logCallable?.close();
    if (_ctx != null) {
      _bindings.fdc_destroy(_ctx!);
      _ctx = null;
    }
    _bindings.fdc_cleanup();
    await _runtimeController.close();
    await _peerController.close();
    await _messageController.close();
    await _peersController.close();
    await _logController.close();
  }
}

/// Thrown when a native FFI call returns a non-zero error code.
class DataChannelException implements Exception {
  DataChannelException(this.operation, this.code, [this.nativeMessage]);

  final String operation;
  final int code;
  final String? nativeMessage;

  @override
  String toString() =>
      'DataChannelException($operation, code=$code${nativeMessage != null ? ', $nativeMessage' : ''})';
}

/// Parse [PeersUpdatedEvent.peersJson] into a list of server peer IDs.
List<String> parseServerPeers(PeersUpdatedEvent event) {
  final map = jsonDecode(event.peersJson) as Map<String, dynamic>;
  final servers = map['servers'];
  if (servers is List) {
    return servers.map((e) => e.toString()).toList();
  }
  return const [];
}

/// Parse an Ollama response envelope from [PeerMessageEvent].
Map<String, dynamic>? parseOllamaResponse(PeerMessageEvent event) {
  try {
    final map = jsonDecode(event.text) as Map<String, dynamic>;
    if (map['type'] == 'ollama_response') return map;
  } catch (_) {}
  return null;
}
