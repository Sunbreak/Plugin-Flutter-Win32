import 'dart:async';

import 'package:flutter/services.dart';

import 'models.dart';
import 'NotepadClient.dart';

final sample = Sample._();

const _channel = const MethodChannel('sample_plugin');
const _message_scanResult = const BasicMessageChannel('sample_plugin/message.scanResult', StandardMessageCodec());

class Sample {
  Sample._() {
    _message_scanResult.setMessageHandler(_handleScanResultMessage);
  }

  void startScan() {
    _channel
        .invokeMethod('startScan')
        .then((_) => print('startScan invokeMethod success'));
  }
  
  void stopScan() {
    _channel
        .invokeMethod('stopScan')
        .then((_) => print('stopScan invokeMethod success'));
  }

  final _scanResultStreamController = StreamController<NotepadScanResult>.broadcast();

  Stream<NotepadScanResult> get scanResultStream => _scanResultStreamController.stream;

  Future<dynamic> _handleScanResultMessage(dynamic message) {
    print('_handleScanResultMessage $message');
    var notepadScanResult = NotepadScanResult.fromMap(message);
    if (NotepadClient.support(notepadScanResult))
      _scanResultStreamController.add(notepadScanResult);
  }
}
