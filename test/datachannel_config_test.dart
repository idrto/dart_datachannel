import 'package:flutter_datachannel/flutter_datachannel.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  group('DataChannelConfig', () {
    test('client mode defaults', () {
      const config = DataChannelConfig(
        mode: DataChannelMode.client,
        signalingUrl: 'ws://localhost:8765',
        peerId: 'test-client',
      );
      expect(config.mode, DataChannelMode.client);
      expect(config.stunServer, 'stun:stun.l.google.com:19302');
      expect(config.ollamaUrl, 'http://127.0.0.1:11434');
    });

    test('copyWith preserves values', () {
      const config = DataChannelConfig(
        mode: DataChannelMode.server,
        signalingUrl: 'ws://localhost:8765',
        peerId: 'server-1',
        ollamaUrl: 'http://127.0.0.1:11434',
      );
      final copy = config.copyWith(peerId: 'server-2');
      expect(copy.peerId, 'server-2');
      expect(copy.mode, DataChannelMode.server);
    });
  });

  group('parseServerPeers', () {
    test('parses servers list', () {
      const event = PeersUpdatedEvent('{"type":"peers","servers":["a","b"]}');
      expect(parseServerPeers(event), ['a', 'b']);
    });
  });
}
