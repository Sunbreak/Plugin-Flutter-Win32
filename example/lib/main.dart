// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
import 'dart:async';

import 'package:flutter/foundation.dart'
    show debugDefaultTargetPlatformOverride;
import 'package:flutter/material.dart';

import 'package:sample/sample.dart';
import 'package:sample/models.dart';

void main() {
  // See https://github.com/flutter/flutter/wiki/Desktop-shells#target-platform-override
  debugDefaultTargetPlatformOverride = TargetPlatform.fuchsia;

  runApp(new MobileApp());
}

class MobileApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: _MobileHomePage(),
    );
  }
}

class _MobileHomePage extends StatefulWidget {
  @override
  State<StatefulWidget> createState() => _MobileHomePageState();
}

final GlobalKey<ScaffoldState> _scaffoldKey = GlobalKey<ScaffoldState>();

_toast(String msg) => _scaffoldKey.currentState
  .showSnackBar(SnackBar(content: Text(msg), duration: Duration(seconds: 2)));

class _MobileHomePageState extends State<_MobileHomePage> {
  StreamSubscription<NotepadScanResult> _subscription;

  @override
  void initState() {
    super.initState();
    _subscription = sample.scanResultStream.listen((result) {
      if (!_scanResults.any((r) => r.deviceId == result.deviceId)) {
        setState(() => _scanResults.add(result));
      }
    });
  }

  @override
  void dispose() {
    super.dispose();
    _subscription?.cancel();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      key: _scaffoldKey,
      appBar: AppBar(
        title: Text('Plugin example app'),
      ),
      body: Column(
        children: <Widget>[
          _buildButtons(),
          Divider(color: Colors.blue,),
          _buildListView(),
        ],
      ),
    );
  }

  Widget _buildButtons() {
    return Row(
      mainAxisAlignment: MainAxisAlignment.spaceEvenly,
      children: <Widget>[
        RaisedButton(
          child: Text('startScan'),
          onPressed: () {
            sample.startScan();
          },
        ),
        RaisedButton(
          child: Text('stopScan'),
          onPressed: () {
            sample.stopScan();
          },
        ),
      ],
    );
  }

  var _scanResults = List<NotepadScanResult>();

  Widget _buildListView() {
    return Expanded(
      child: ListView.separated(
        itemBuilder: (context, index) =>
            ListTile(
              title: Text('${_scanResults[index].name}(${_scanResults[index].rssi})'),
              subtitle: Text(_scanResults[index].deviceId),
            ),
        separatorBuilder: (context, index) => Divider(),
        itemCount: _scanResults.length,
      ),
    );
  }
}
