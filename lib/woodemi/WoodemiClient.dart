import 'dart:typed_data';

import 'package:sample/models.dart';

import '../Notepad.dart';
import '../NotepadClient.dart';
import '../method_channel.dart';
import 'Woodemi.dart';

class WoodemiClient extends NotepadClient {
  static Uint8List get prefix => Uint8List.fromList([0x57, 0x44, 0x4d]); // 'WDM'

  @override
  Tuple2<String, String> get commandRequestCharacteristic => const Tuple2(SERV__COMMAND, CHAR__COMMAND_REQUEST);

  @override
  Tuple2<String, String> get commandResponseCharacteristic => const Tuple2(SERV__COMMAND, CHAR__COMMAND_RESPONSE);

  @override
  Tuple2<String, String> get syncInputCharacteristic => const Tuple2(SERV__SYNC, CHAR__SYNC_INPUT);

  @override
  List<Tuple2<String, String>> get inputIndicationCharacteristics => [
    commandResponseCharacteristic,
  ];

  @override
  List<Tuple2<String, String>> get inputNotificationCharacteristics => [
    syncInputCharacteristic,
  ];

  @override
  Future<void> completeConnection(void awaitConfirm(bool)) async {
    var accessResult = await _checkAccess(defaultAuthToken, 10, awaitConfirm);
    switch(accessResult) {
      case AccessResult.Denied:
        throw AccessException.Denied;
      case AccessResult.Unconfirmed:
        throw AccessException.Unconfirmed;
      default:
        break;
    }

    await super.completeConnection(awaitConfirm);
  }

  //#region authorization
  Future<AccessResult> _checkAccess(Uint8List authToken, int seconds, void awaitConfirm(bool)) async {
    var command = WoodemiCommand(
      request: Uint8List.fromList([0x01, seconds] + authToken),
      intercept: (data) => data.first == 0x02,
      handle: (data) => data[1],
    );
    var response = await notepadType.executeCommand(command);
    switch(response) {
      case 0x00:
        return AccessResult.Denied;
      case 0x01:
        awaitConfirm(true);
        var receiveConfirmAsync = notepadType.receiveResponseAsync('Confirm',
          commandResponseCharacteristic, (value) => value.first == 0x03
        ).then((value) => value[1] == 0x00);
        var confirm = await receiveConfirmAsync.timeout(Duration(seconds: seconds), onTimeout: () => false);
        return confirm ? AccessResult.Confirmed : AccessResult.Unconfirmed;
      case 0x02:
        return AccessResult.Approved;
      default:
        throw Exception('Unknown error');
    }
  }
  //#endregion

  //#region SyncInput
  @override
  Future<void> setMode(NotepadMode notepadMode) async {
    var mode = notepadMode == NotepadMode.Sync ? 0x00 : 0x01;
    await notepadType.executeCommand(
      WoodemiCommand(
        request: Uint8List.fromList([0x05, mode]),
      ),
    );
  }

  @override
  List<NotePenPointer> parseSyncData(Uint8List value) {
    return parseSyncPointer(value).where((pointer) {
      return 0 <= pointer.x && pointer.x <= A1_WIDTH
          && 0<= pointer.y && pointer.y <= A1_HEIGHT;
    }).toList();
  }
  //#endregion
}