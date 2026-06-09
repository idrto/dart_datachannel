import 'dart:io';

import 'package:flutter/material.dart';
import 'package:dart_datachannel/dart_datachannel.dart';

void main() {
  // When running from the repo, use the prebuilt native library.
  for (final relative in [
    '../../native/build/libdart_datachannel.so',
    '../../native/build/libdart_datachannel.dylib',
    '../../native/build/dart_datachannel.dll',
  ]) {
    final file = File(relative);
    if (file.existsSync()) {
      configureNativeLibrary(file.absolute.path);
      break;
    }
  }
  runApp(const ExampleApp());
}

class ExampleApp extends StatelessWidget {
  const ExampleApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'flutter_datachannel example',
      theme: ThemeData(colorSchemeSeed: Colors.indigo, useMaterial3: true),
      home: const ClientDemoPage(),
    );
  }
}

class ClientDemoPage extends StatefulWidget {
  const ClientDemoPage({super.key});

  @override
  State<ClientDemoPage> createState() => _ClientDemoPageState();
}

class _ClientDemoPageState extends State<ClientDemoPage> {
  final _signalingCtrl =
      TextEditingController(text: 'ws://127.0.0.1:8765');
  final _serverIdCtrl = TextEditingController(text: 'm4-ollama');
  final _promptCtrl =
      TextEditingController(text: 'Say hello in one short sentence.');

  DataChannelClient? _client;
  String _status = 'idle';
  String _output = '';
  List<String> _servers = [];

  @override
  void dispose() {
    _client?.dispose();
    _signalingCtrl.dispose();
    _serverIdCtrl.dispose();
    _promptCtrl.dispose();
    super.dispose();
  }

  Future<void> _connect() async {
    setState(() {
      _status = 'starting…';
      _output = '';
    });

    final client = DataChannelClient(
      signalingUrl: _signalingCtrl.text.trim(),
      peerId: 'flutter-demo-${DateTime.now().millisecondsSinceEpoch}',
      verboseLogging: true,
    );

    client.onRuntimeState.listen((e) {
      setState(() => _status = '${e.state}${e.message != null ? ': ${e.message}' : ''}');
    });

    client.onPeersUpdated.listen((e) {
      setState(() => _servers = parseServerPeers(e));
    });

    client.onMessage.listen((e) {
      final resp = parseOllamaResponse(e);
      setState(() {
        _output = resp != null ? resp.toString() : e.text;
      });
    });

    _client = client;
    await client.start();
    await client.refreshServers();
  }

  Future<void> _query() async {
    final client = _client;
    if (client == null) return;

    final serverId = _serverIdCtrl.text.trim();
    await client.connectToServer(serverId);
    await client.queryOllama(
      serverId,
      body: {
        'model': 'llama3',
        'prompt': _promptCtrl.text,
        'stream': false,
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('DataChannel Client Demo')),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            TextField(
              controller: _signalingCtrl,
              decoration: const InputDecoration(labelText: 'Signaling URL'),
            ),
            TextField(
              controller: _serverIdCtrl,
              decoration: const InputDecoration(labelText: 'Server peer ID'),
            ),
            Text('Status: $_status'),
            if (_servers.isNotEmpty) Text('Servers: ${_servers.join(', ')}'),
            const SizedBox(height: 8),
            ElevatedButton(onPressed: _connect, child: const Text('Connect')),
            const SizedBox(height: 8),
            TextField(
              controller: _promptCtrl,
              maxLines: 3,
              decoration: const InputDecoration(labelText: 'Prompt'),
            ),
            ElevatedButton(onPressed: _query, child: const Text('Query Ollama')),
            const SizedBox(height: 16),
            Expanded(
              child: SingleChildScrollView(
                child: Text(_output.isEmpty ? '(response appears here)' : _output),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
