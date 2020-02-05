import 'models.dart';
import 'utils.dart';
import 'woodemi/WoodemiClient.dart';

abstract class NotepadClient {
  static bool support(NotepadScanResult scanResult) {
    return startWith(scanResult.manufacturerData, WoodemiClient.prefix);
  }
}