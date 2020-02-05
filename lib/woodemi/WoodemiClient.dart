import 'dart:typed_data';

import '../NotepadClient.dart';

class WoodemiClient extends NotepadClient {
  static Uint8List get prefix => Uint8List.fromList([0x57, 0x44, 0x4d]); // 'WDM'
}