import 'dart:async';
import 'dart:typed_data';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:sample/sample.dart';

import 'PaintArea.dart';
import 'stylus_paint/stylus_paint.dart';

class NotepadDetailPage extends StatefulWidget {
  final scanResult;

  // FIXME close
  final notePenPointerController = StreamController<NotePenPointer>.broadcast();

  final stylusPaintController = StylusPainterController();

  NotepadDetailPage(this.scanResult);

  @override
  State<StatefulWidget> createState() => _NotepadDetailPageState();
}

final GlobalKey<ScaffoldState> _scaffoldKey = GlobalKey<ScaffoldState>();

_toast(String msg) => _scaffoldKey.currentState
  .showSnackBar(SnackBar(content: Text(msg), duration: Duration(seconds: 2)));

class _NotepadDetailPageState extends State<NotepadDetailPage> implements NotepadClientCallback {
  @override
  void initState() {
    super.initState();
    notepadConnector.connectionChangeHandler = _handleConnectionChange;
  }

  @override
  void dispose() {
    super.dispose();
    notepadConnector.connectionChangeHandler = null;
  }

  NotepadClient _notepadClient;
  
  void _handleConnectionChange(NotepadClient client, NotepadConnectionState state) {
    print('_handleConnectionChange $client $state');
    if (state == NotepadConnectionState.connected) {
      _notepadClient = client;
      _notepadClient.callback = this;
    } else {
      _notepadClient?.callback = null;
      _notepadClient = null;
    }
  }

  @override
  void handlePointer(List<NotePenPointer> list) {
    print('handlePointer ${list.length}');
    widget.notePenPointerController.addStream(Stream.fromIterable(list));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      key: _scaffoldKey,
      appBar: AppBar(
        title: Text('NotepadDetailPage'),
        actions: <Widget>[
          FlatButton(
            child: Text('connect'),
            onPressed: () {
              notepadConnector.connect(widget.scanResult, Uint8List.fromList([0x00, 0x00, 0x00, 0x02]));
            },
          ),
          FlatButton(
            child: Text('setMode'),
            onPressed: () async {
              await _notepadClient.setMode(NotepadMode.Sync);
            },
          ),
          FlatButton(
            child: Text('disconnect'),
            onPressed: () {
              notepadConnector.disconnect();
            },
          ),
        ],
      ),
      body: Center(
        child: PaintArea.of(
          stream: widget.notePenPointerController.stream.map((p) => StylusPointer.fromMap(p.toMap())),
          controller: widget.stylusPaintController,
          srcSize: Size(14800, 21000),
          dstSize: ui.window.physicalSize,
          backgroundColor: Color(0xFFFEFEFE),
        ),
      ),
    );
  }
}
