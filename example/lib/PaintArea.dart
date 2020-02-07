import 'dart:async';
import 'dart:math';
import 'dart:ui' as ui;

import 'package:flutter/material.dart';

import 'stylus_paint/stylus_paint.dart';

class PaintArea extends StatefulWidget {
  final Stream<StylusPointer> stream;

  final StylusPainterController controller;

  final double scaleRatio;

  final Size paintSize;

  final Color backgroundColor;

  PaintArea({
    this.stream,
    this.controller,
    this.scaleRatio,
    this.paintSize,
    this.backgroundColor,
  })
      : assert(scaleRatio != null),
        assert(paintSize != null);

  factory PaintArea.of({
    Stream<StylusPointer> stream,
    StylusPainterController controller,
    Size srcSize,
    Size dstSize,
    Color backgroundColor,
  }) {
    final paintScale = min(dstSize.width / srcSize.width, dstSize.height / srcSize.height);
    final scaleRatio = paintScale / ui.window.devicePixelRatio;
    final paintSize = srcSize * scaleRatio;
    return PaintArea(
      stream: stream,
      controller: controller,
      scaleRatio: scaleRatio,
      paintSize: paintSize,
      backgroundColor: backgroundColor,
    );
  }

  @override
  State<StatefulWidget> createState() => _PaintAreaState();
}

class _PaintAreaState extends State<PaintArea> {
  StreamSubscription<StylusPointer> _streamSubscription;

  @override
  void initState() {
    super.initState();
    widget.controller.paint.strokeWidth = 0.5;
    _streamSubscription = widget.stream.listen((onData) {
      setState(() => widget.controller.append(onData));
    });
  }

  @override
  void dispose() {
    super.dispose();
    _streamSubscription?.cancel();
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      color: widget.backgroundColor,
      constraints: BoxConstraints.loose(widget.paintSize),
      child: Stack(
        children: <Widget>[
          CustomPaint(
            size: widget.paintSize,
            painter: LineStrokePainter(
              widget.controller.strokes,
              widget.scaleRatio,
            ),
          ),
          IndicatorLayer(
            widget.stream,
            widget.scaleRatio,
            widget.paintSize,
          ),
        ],
      ),
    );
  }
}

class IndicatorLayer extends StatefulWidget {
  final Stream<StylusPointer> stream;

  // TODO IndicatePainterController
  final StylusPainterController controller = StylusPainterController();

  final double scaleRatio;

  final Size paintSize;

  IndicatorLayer(this.stream, this.scaleRatio, this.paintSize);

  @override
  State<StatefulWidget> createState() => _IndicatorLayerState();
}

class _IndicatorLayerState extends State<IndicatorLayer> {
  final List<StylusPointer> _pointers = <StylusPointer>[];

  ui.Image _indicator;

  bool visible = false;

  StreamSubscription _streamSubscription;

  @override
  void initState() {
    super.initState();
    loadImage(AssetImage('images/indicator_pen.png')).then((onValue) {
      setState(() => _indicator = onValue);
    });
    var stream = widget.stream.timeout(Duration(milliseconds: 100));
    _streamSubscription = stream.listen((onData) {
      setState(() {
        _pointers.add(onData);
        if (!visible) setState(() => visible = true);
      });
    }, onError: (error) {
      if (error is TimeoutException) {
        if (visible) setState(() => visible = false);
      }
    });
  }

  @override
  void dispose() {
    super.dispose();
    _streamSubscription?.cancel();
  }

  @override
  Widget build(BuildContext context) {
    return visible
        ? _buildIndicator()
        : AnimatedOpacity(
            opacity: 0.0,
            duration: Duration(milliseconds: 200),
            child: _buildIndicator(),
          );
  }

  Stack _buildIndicator() {
    return Stack(
      children: <Widget>[
        CustomPaint(
          size: widget.paintSize,
          painter: CircleIndicatePainter(
            _pointers,
            widget.scaleRatio,
          )..uiPaint.style = PaintingStyle.stroke,
        ),
        if (_indicator != null)
          CustomPaint(
            size: widget.paintSize,
            painter: ImageIndicatePainter(
              _pointers,
              widget.scaleRatio,
              _indicator,
            ),
          ),
      ],
    );
  }
}

Future<ui.Image> loadImage<T>(ImageProvider<T> imageProvider) async {
  var stream = imageProvider.resolve(ImageConfiguration.empty);
  var completer = Completer<ui.Image>();
  var listener = ImageStreamListener((frame, sync) => completer.complete(frame.image));
  stream.addListener(listener);
  var image = await completer.future;
  stream.removeListener(listener);
  return image;
}