import '../datachannel_config.dart';

/// Event emitted when the engine runtime state changes.
class RuntimeStateEvent {
  const RuntimeStateEvent(this.state, [this.message]);

  final DataChannelRuntimeState state;
  final String? message;
}

/// Event emitted when a remote peer's state changes.
class PeerStateEvent {
  const PeerStateEvent(this.peerId, this.state);

  final String peerId;
  final DataChannelPeerState state;
}

/// Raw message received from a peer over the data channel.
class PeerMessageEvent {
  const PeerMessageEvent(this.peerId, this.data);

  final String peerId;
  final List<int> data;

  String get text => String.fromCharCodes(data);
}

/// Updated list of available server peers (JSON payload from signaling).
class PeersUpdatedEvent {
  const PeersUpdatedEvent(this.peersJson);

  final String peersJson;
}

/// Log line from native layer.
class NativeLogEvent {
  const NativeLogEvent(this.level, this.message);

  final int level;
  final String message;
}
