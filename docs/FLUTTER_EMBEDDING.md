# Embedding in Flutter Apps

`dart_datachannel` is a **pure Dart** package (no Flutter SDK dependency).  
Flutter apps depend on it like any other Dart package and **bundle the native library themselves**.

## 1. Add dependency

```yaml
dependencies:
  dart_datachannel:
    git:
      url: https://github.com/idrto/dart_datachannel
      ref: main
```

## 2. Build the native library

```bash
git clone https://github.com/idrto/dart_datachannel.git
cd dart_datachannel
./scripts/build_native.sh
```

Artifacts:

| Platform | File |
|----------|------|
| Linux | `native/build/libdart_datachannel.so` |
| macOS | `native/build/libdart_datachannel.dylib` |
| Windows | `native/build/dart_datachannel.dll` |

## 3. Bundle native lib in your Flutter app

### Linux (`linux/CMakeLists.txt`)

Add after `add_subdirectory("flutter")`:

```cmake
# Path to prebuilt or submodule native lib
set(FDC_NATIVE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../native/build")
set(FDC_BUNDLED_LIBRARIES
  "${FDC_NATIVE_DIR}/libdart_datachannel.so"
  PARENT_SCOPE
)

# Install beside the app binary
install(FILES "${FDC_NATIVE_DIR}/libdart_datachannel.so"
        DESTINATION "${INSTALL_BUNDLE_LIB_DIR}"
        COMPONENT Runtime)
```

Alternatively, add `native/` as a CMake subdirectory (same as the old plugin layout).

### macOS / Windows

Install the `.dylib` / `.dll` into the app bundle `Frameworks` or `lib` folder via your platform `CMakeLists.txt`.

### Android

Place `libdart_datachannel.so` under `android/src/main/jniLibs/<abi>/`.

### iOS

Static-link `fdc_ffi` into the Runner target (see `native/CMakeLists.txt` static build).

## 4. Configure library path at runtime (if needed)

If the loader cannot find the library by default:

```dart
import 'package:dart_datachannel/dart_datachannel.dart';

void main() {
  // Optional — only if not on default LD_LIBRARY_PATH / beside executable
  configureNativeLibrary('/path/to/libdart_datachannel.so');

  runApp(const MyApp());
}
```

Or set environment variable:

```bash
FDC_NATIVE_LIB=/path/to/libdart_datachannel.so flutter run
```

## 5. Use the Dart API

```dart
import 'package:dart_datachannel/dart_datachannel.dart';

final client = DataChannelClient(
  signalingUrl: 'ws://192.168.1.42:8765',
  peerId: 'my-app',
);

await client.start();
await client.connectToServer('m4-ollama');
await client.queryOllama('m4-ollama', body: {
  'model': 'llama3',
  'prompt': 'Hello',
  'stream': false,
});
```

## Demo Flutter app

A reference UI lives in `demos/flutter_example/` (not published to pub.dev).

```bash
cd demos/flutter_example
flutter pub get
flutter run -d linux
```

## Headless server

For server-only deployment without any Dart UI, use the `fdc-server` binary — see [DEVELOPER.md](DEVELOPER.md).
