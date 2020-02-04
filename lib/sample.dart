import 'dart:async';

import 'package:flutter/services.dart';

class Sample {
  static const MethodChannel _channel =
      const MethodChannel('sample_plugin');

  static Future<String> get platformVersion async {
    final String version = await _channel.invokeMethod('getPlatformVersion');
    return version;
  }
  
  static void startScan() {
    _channel
        .invokeMethod('startScan')
        .then((_) => print('startScan invokeMethod success'));
  }
  
  static void stopScan() {
    _channel
        .invokeMethod('stopScan')
        .then((_) => print('stopScan invokeMethod success'));
  }
}
