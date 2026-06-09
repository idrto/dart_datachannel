import 'dart:async';
import 'dart:io';

import 'package:flutter_datachannel/flutter_datachannel.dart';

/// Pure Dart CLI client — no Flutter SDK required.
///
/// Usage:
///   dart run example/bin/client.dart \
///     --signaling ws://127.0.0.1:8765 \
///     --server test-server \
///     --prompt "Hello"
Future<void> main(List<String> args) async {
  final opts = _parseArgs(args);

  // Point at repo native build when running from source tree.
  final repoLib = File('native/build/libflutter_datachannel.so');
  if (repoLib.existsSync()) {
    configureNativeLibrary(repoLib.absolute.path);
  }

  final client = DataChannelClient(
    signalingUrl: opts.signaling,
    peerId: 'dart-cli-${DateTime.now().millisecondsSinceEpoch}',
    verboseLogging: opts.verbose,
  );

  client.onRuntimeState.listen((e) {
    stderr.writeln('[runtime] ${e.state}${e.message != null ? ': ${e.message}' : ''}');
  });
  client.onPeersUpdated.listen((e) {
    stderr.writeln('[peers] ${parseServerPeers(e)}');
  });
  client.onPeerState.listen((e) {
    stderr.writeln('[peer] ${e.peerId} → ${e.state}');
  });

  final completer = Completer<void>();
  client.onMessage.listen((e) {
    final resp = parseOllamaResponse(e);
    stdout.writeln(resp ?? e.text);
    if (!completer.isCompleted) completer.complete();
  });

  await client.start();
  await client.refreshServers();
  await client.connectToServer(opts.server);

  // Brief wait for data channel to open.
  await Future<void>.delayed(const Duration(seconds: 2));

  await client.queryOllama(
    opts.server,
    path: '/api/generate',
    body: {
      'model': opts.model,
      'prompt': opts.prompt,
      'stream': false,
    },
  );

  await completer.future.timeout(
    const Duration(seconds: 60),
    onTimeout: () => stderr.writeln('timeout waiting for response'),
  );

  await client.dispose();
  exit(0);
}

class _Opts {
  _Opts({
    required this.signaling,
    required this.server,
    required this.prompt,
    required this.model,
    required this.verbose,
  });

  final String signaling;
  final String server;
  final String prompt;
  final String model;
  final bool verbose;
}

_Opts _parseArgs(List<String> args) {
  var signaling = 'ws://127.0.0.1:8765';
  var server = 'test-server';
  var prompt = 'Say hello in one short sentence.';
  var model = 'llama3';
  var verbose = false;

  for (var i = 0; i < args.length; i++) {
    switch (args[i]) {
      case '--signaling':
        signaling = args[++i];
      case '--server':
        server = args[++i];
      case '--prompt':
        prompt = args[++i];
      case '--model':
        model = args[++i];
      case '--verbose':
        verbose = true;
      case '-h':
      case '--help':
        stderr.writeln(
          'Usage: dart run bin/client.dart '
          '[--signaling URL] [--server ID] [--prompt TEXT] [--model NAME]',
        );
        exit(0);
    }
  }

  return _Opts(
    signaling: signaling,
    server: server,
    prompt: prompt,
    model: model,
    verbose: verbose,
  );
}
