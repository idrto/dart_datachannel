/// flutter_datachannel — WebRTC data channels for Flutter via libdatachannel.
///
/// Supports three runtime modes:
/// - [DataChannelClient] — outgoing connections only
/// - [DataChannelServer] — accepts incoming connections, proxies Ollama
/// - [DataChannelHybrid] — both
///
/// See `docs/DEVELOPER.md` for setup, testing, and architecture.
export 'src/datachannel_client.dart';
export 'src/datachannel_config.dart';
export 'src/datachannel_engine.dart';
export 'src/datachannel_hybrid.dart';
export 'src/datachannel_server.dart';
export 'src/events/datachannel_events.dart';
