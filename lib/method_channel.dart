import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/services.dart';

const _method = const MethodChannel('sample_plugin');
const _message_scanResult = const BasicMessageChannel('sample_plugin/message.scanResult', StandardMessageCodec());
const _message_connector = BasicMessageChannel('sample_plugin/message.connector', StandardMessageCodec());
const _message_client = BasicMessageChannel('sample_plugin/message.client', StandardMessageCodec());

class NotepadCorePlatform {
  static NotepadCorePlatform _instance = NotepadCorePlatform._(); 

  static NotepadCorePlatform get instance => _instance; 

  NotepadCorePlatform._() {
    _message_scanResult.setMessageHandler(_handleScanResultMessage);
    _message_connector.setMessageHandler(_handleConnectorMessage);
    _message_client.setMessageHandler(_handleClientMessage);
  }

  void startScan() {
    _method
        .invokeMethod('startScan')
        .then((_) => print('startScan invokeMethod success'));
  }
  
  void stopScan() {
    _method
        .invokeMethod('stopScan')
        .then((_) => print('stopScan invokeMethod success'));
  }

  final _scanResultStreamController = StreamController<dynamic>.broadcast();

  Stream<dynamic> get scanResultStream => _scanResultStreamController.stream;

  Future<dynamic> _handleScanResultMessage(dynamic message) async {
    print('_handleScanResultMessage $message');
    _scanResultStreamController.add(message);
  }

  void connect(scanResult) {
    _method.invokeMethod('connect', {
      'deviceId': scanResult.deviceId,
    }).then((_) => print('connect invokeMethod success'));
    if (messageHandler != null) messageHandler(NotepadConnectionState.connecting);
  }

  void disconnect() {
    _method.invokeMethod('disconnect').then((_) {
      print('disconnect invokeMethod success');
    });
  }

  Future<dynamic> _handleConnectorMessage(dynamic message) async {
    print('_handleConnectorMessage $message');
    if (message['ConnectionState'] != null) {
      var connectionState = NotepadConnectionState.parse(message['ConnectionState']);
      if (connectionState == NotepadConnectionState.connected) {
        _method.invokeMethod('discoverServices').then((_) => print('discoverServices invokeMethod success'));
      } else {
        if (messageHandler != null) messageHandler(connectionState);
      }
    } else if (message['ServiceState'] != null) {
      if (message['ServiceState'] == 'discovered')
        if (messageHandler != null) messageHandler(NotepadConnectionState.connected);
    }
  }

  MessageHandler messageHandler;

  // FIXME Close
  final _characteristicConfigController = StreamController<String>.broadcast();

  Future<void> setNotifiable(Tuple2<String, String> serviceCharacteristic, BleInputProperty bleInputProperty) async {
    _method.invokeMethod('setNotifiable', {
      'service': serviceCharacteristic.item1,
      'characteristic': serviceCharacteristic.item2,
      'bleInputProperty': bleInputProperty.value,
    }).then((_) => print('setNotifiable invokeMethod success'));
    // TODO Timeout
    await _characteristicConfigController.stream.any((c) => c == serviceCharacteristic.item2);
  }

  Future<void> writeValue(Tuple2<String, String> serviceCharacteristic, Uint8List value, BleOutputProperty bleOutputProperty) async {
    _method.invokeMethod('writeValue', {
      'service': serviceCharacteristic.item1,
      'characteristic': serviceCharacteristic.item2,
      'value': value,
      'bleOutputProperty': bleOutputProperty.value,
    }).then((_) {
      print('writeValue invokeMethod success');
    }).catchError((onError) {
      // Characteristic sometimes unavailable on Android
      throw onError;
    });
  }

  // FIXME Close
  final _characteristicValueController = StreamController<Tuple2<String, Uint8List>>.broadcast();

  Stream<Tuple2<String, Uint8List>> get inputValueStream => _characteristicValueController.stream;

  Future<dynamic> _handleClientMessage(dynamic message) {
    print('_handleClientMessage $message');
    if (message['characteristicConfig'] != null) {
      _characteristicConfigController.add(message['characteristicConfig']);
    } else if (message['characteristicValue'] != null) {
      var characteristicValue = message['characteristicValue'];
      var value = Uint8List.fromList(characteristicValue['value']); // In case of _Uint8ArrayView
      _characteristicValueController.add(Tuple2(characteristicValue['characteristic'], value));
    }
  }
}

/// FIXME `import 'package:tuple/tuple.dart';` fails
class Tuple2<T1, T2> {
  /// Returns the first item of the tuple
  final T1 item1;

  /// Returns the second item of the tuple
  final T2 item2;

  /// Creates a new tuple value with the specified items.
  const Tuple2(this.item1, this.item2);
}

class BleInputProperty {
  static const disabled = BleInputProperty._('disabled');
  static const notification = BleInputProperty._('notification');
  static const indication = BleInputProperty._('indication');

  final String value;

  const BleInputProperty._(this.value);
}

class BleOutputProperty {
  static const withResponse = BleOutputProperty._('withResponse');
  static const withoutResponse = BleOutputProperty._('withoutResponse');

  final String value;

  const BleOutputProperty._(this.value);
}

typedef MessageHandler = Future<dynamic> Function(NotepadCoreMessage message);

class NotepadCoreMessage {
  const NotepadCoreMessage._();
}

class NotepadConnectionState extends NotepadCoreMessage {
  static const disconnected = NotepadConnectionState._('disconnected');
  static const connecting = NotepadConnectionState._('connecting');
  static const awaitConfirm = NotepadConnectionState._('awaitConfirm');
  static const connected = NotepadConnectionState._('connected');

  final String value;

  const NotepadConnectionState._(this.value) : super._();

  static NotepadConnectionState parse(String value) {
    if (value == disconnected.value) {
      return disconnected;
    } else if (value == connecting.value) {
      return connecting;
    } else if (value == connected.value) {
      return connected;
    }
    throw ArgumentError.value(value);
  }
}