import 'dart:ffi';
import 'dart:io';

import 'package:flutter/foundation.dart';

/// Loads the native `flutter_datachannel` shared library for the current platform.
DynamicLibrary openNativeLibrary() {
  if (Platform.isAndroid) {
    return DynamicLibrary.open('libflutter_datachannel.so');
  }
  if (Platform.isIOS) {
    // iOS links statically via the plugin build.
    return DynamicLibrary.process();
  }
  if (Platform.isLinux) {
    return DynamicLibrary.open('libflutter_datachannel.so');
  }
  if (Platform.isMacOS) {
    return DynamicLibrary.open('libflutter_datachannel.dylib');
  }
  if (Platform.isWindows) {
    return DynamicLibrary.open('flutter_datachannel.dll');
  }
  throw UnsupportedError(
    'flutter_datachannel is not supported on ${Platform.operatingSystem}',
  );
}

/// Singleton accessor used by generated bindings.
@visibleForTesting
DynamicLibrary? debugNativeLibraryOverride;

DynamicLibrary get nativeLibrary =>
    debugNativeLibraryOverride ?? openNativeLibrary();
