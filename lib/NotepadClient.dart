import 'dart:typed_data';

import 'NotepadType.dart';
import 'method_channel.dart';
import 'models.dart';
import 'utils.dart';
import 'woodemi/WoodemiClient.dart';

abstract class NotepadClientCallback {
  void handlePointer(List<NotePenPointer> list);
}

abstract class NotepadClient {
  static bool support(NotepadScanResult scanResult) {
    return startWith(scanResult.manufacturerData, WoodemiClient.prefix);
  }

  static NotepadClient create(dynamic scanResult) {
    // FIXME Support both native & web
    return WoodemiClient();
  }

  Tuple2<String, String> get commandRequestCharacteristic;

  Tuple2<String, String> get commandResponseCharacteristic;

  Tuple2<String, String> get syncInputCharacteristic;

  List<Tuple2<String, String>> get inputIndicationCharacteristics;

  List<Tuple2<String, String>> get inputNotificationCharacteristics;

  NotepadType notepadType;

  Future<void> completeConnection(void awaitConfirm(bool)) {
    // TODO Cancel
    notepadType.receiveSyncInput().listen((value) {
      callback?.handlePointer(parseSyncData(value));
    });
  }

  NotepadClientCallback callback;

  //#region SyncInput
  Future<void> setMode(NotepadMode notepadMode);

  List<NotePenPointer> parseSyncData(Uint8List value);
  //#endregion
}