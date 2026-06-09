/// Dart FFI bindings for libdatachannel WebRTC data channels.
///
/// Pure Dart — no Flutter SDK dependency. Usable from CLI, server, or
/// Flutter apps (Flutter consumers must bundle the native library themselves;
/// see `docs/FLUTTER_EMBEDDING.md`).
///
/// Modes: [DataChannelClient], [DataChannelServer], [DataChannelHybrid].
library;

export 'src/datachannel_client.dart';
export 'src/datachannel_config.dart';
export 'src/datachannel_engine.dart';
export 'src/datachannel_hybrid.dart';
export 'src/datachannel_server.dart';
export 'src/events/datachannel_events.dart';
export 'src/ffi/native_library.dart'
    show
        configureNativeLibrary,
        resetNativeLibraryConfiguration,
        defaultNativeLibraryName,
        nativeLibrarySearchPaths,
        nativeLibraryEnvVar,
        openNativeLibrary;
