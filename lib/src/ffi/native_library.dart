import 'dart:ffi';
import 'dart:io';

/// Environment variable for the native shared library path.
const String nativeLibraryEnvVar = 'FDC_NATIVE_LIB';

String? _configuredLibraryPath;

/// Override the path used to load the native `libdart_datachannel` library.
///
/// Call this before creating a [DataChannelEngine] when the library is not on
/// the default search path (e.g. bundled inside a Flutter app).
void configureNativeLibrary(String path) {
  _configuredLibraryPath = path;
}

/// Clears a prior [configureNativeLibrary] override.
void resetNativeLibraryConfiguration() {
  _configuredLibraryPath = null;
}

/// For tests: inject a pre-opened or mock library.
DynamicLibrary? debugNativeLibraryOverride;

/// Default shared-library file name per platform.
String defaultNativeLibraryName() {
  if (Platform.isAndroid || Platform.isLinux) {
    return 'libdart_datachannel.so';
  }
  if (Platform.isMacOS) {
    return 'libdart_datachannel.dylib';
  }
  if (Platform.isWindows) {
    return 'dart_datachannel.dll';
  }
  if (Platform.isIOS) {
    // iOS: static link — symbols live in the process image.
    return '';
  }
  throw UnsupportedError(
    'dart_datachannel is not supported on ${Platform.operatingSystem}',
  );
}

/// Candidate paths checked when no explicit path is configured.
List<String> nativeLibrarySearchPaths() {
  final name = defaultNativeLibraryName();
  if (name.isEmpty) return const [];

  final candidates = <String>[
    name,
    // Legacy name from before dart_datachannel rename
    if (name.contains('dart_datachannel')) 'libflutter_datachannel.so',
    'native/build/$name',
    '../native/build/$name',
    '../../native/build/$name',
    'build/native/$name',
  ];

  final fromEnv = Platform.environment[nativeLibraryEnvVar];
  if (fromEnv != null && fromEnv.isNotEmpty) {
    candidates.insert(0, fromEnv);
  }

  final configured = _configuredLibraryPath;
  if (configured != null && configured.isNotEmpty) {
    candidates.insert(0, configured);
  }

  return candidates;
}

/// Loads the native library for the current platform.
DynamicLibrary openNativeLibrary() {
  if (debugNativeLibraryOverride != null) {
    return debugNativeLibraryOverride!;
  }

  if (Platform.isIOS) {
    return DynamicLibrary.process();
  }

  final tried = <String>[];
  for (final path in nativeLibrarySearchPaths()) {
    tried.add(path);
    final hasSlash = path.contains('/') || path.contains(r'\');
    if (hasSlash && !File(path).existsSync()) {
      continue;
    }
    try {
      return DynamicLibrary.open(path);
    } catch (_) {
      // try next candidate
    }
  }

  throw StateError(
    'Could not load native library. Tried: ${tried.join(', ')}. '
    'Build with ./scripts/build_native.sh or call configureNativeLibrary(path).',
  );
}

DynamicLibrary get nativeLibrary => openNativeLibrary();
