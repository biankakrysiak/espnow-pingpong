import 'dart:async';
import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

const serviceUuid = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const charUuid = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
const wsUrl = 'ws://192.168.4.1:81';

// ---------- dane ----------
class GameState {
  final int seq, ballX, ballY, p1X, p2X, score1, score2, status, hostMs;
  GameState(this.seq, this.ballX, this.ballY, this.p1X, this.p2X, this.score1,
      this.score2, this.status, this.hostMs);

  // StatePacket: 18 B, little-endian
  static GameState? parse(Uint8List d) {
    if (d.length != 18 || d[0] != 2) return null;
    final b = ByteData.sublistView(d);
    return GameState(
      b.getUint16(1, Endian.little),
      b.getInt16(3, Endian.little),
      b.getInt16(5, Endian.little),
      b.getInt16(7, Endian.little),
      b.getInt16(9, Endian.little),
      d[11], d[12], d[13],
      b.getUint32(14, Endian.little),
    );
  }
}

class Stats {
  int received = 0, lost = 0;
  int? _lastSeq;
  final _sw = Stopwatch()..start();
  int _lastUs = 0;
  double jitterMs = 0, _meanMs = 0;

  void onPacket(GameState s) {
    if (_lastSeq != null) lost += (s.seq - _lastSeq! - 1) & 0xFFFF;
    _lastSeq = s.seq;
    received++;
    final now = _sw.elapsedMicroseconds;
    if (_lastUs != 0) {
      final dt = (now - _lastUs) / 1000.0;
      _meanMs += (dt - _meanMs) * 0.05;
      jitterMs += ((dt - _meanMs).abs() - jitterMs) * 0.05;
    }
    _lastUs = now;
  }

  double get meanMs => _meanMs;
}

// ---------- połączenia ----------
class Link {
  final Stream<Uint8List> data;
  final Future<void> Function() close;
  Link(this.data, this.close);
}

Future<Link> connectWifi() async {
  final ch = WebSocketChannel.connect(Uri.parse(wsUrl));
  await ch.ready.timeout(const Duration(seconds: 8));
  final stream = ch.stream
      .where((e) => e is List<int>)
      .map((e) => Uint8List.fromList(e as List<int>));
  return Link(stream, () async => ch.sink.close());
}

Future<Link> connectBle() async {
  await [Permission.bluetoothScan, Permission.bluetoothConnect, Permission.locationWhenInUse]
      .request();
  await FlutterBluePlus.startScan(
      withServices: [Guid(serviceUuid)], timeout: const Duration(seconds: 8));
  final results = await FlutterBluePlus.scanResults
      .where((l) => l.isNotEmpty)
      .first
      .timeout(const Duration(seconds: 10));
  await FlutterBluePlus.stopScan();
  final dev = results.first.device;
  await dev.connect();
  final services = await dev.discoverServices();
  final chr = services
      .firstWhere((s) => s.uuid == Guid(serviceUuid))
      .characteristics
      .firstWhere((c) => c.uuid == Guid(charUuid));
  await chr.setNotifyValue(true);
  return Link(chr.onValueReceived.map((v) => Uint8List.fromList(v)),
      () async => dev.disconnect());
}

// ---------- UI ----------
void main() => runApp(const MaterialApp(home: Home(), debugShowCheckedModeBanner: false));

class Home extends StatefulWidget {
  const Home({super.key});
  @override
  State<Home> createState() => _HomeState();
}

class _HomeState extends State<Home> {
  String msg = '';
  bool busy = false;

  Future<void> go(Future<Link> Function() connect) async {
    setState(() { busy = true; msg = 'Łączenie...'; });
    try {
      final link = await connect();
      if (!mounted) return;
      setState(() { busy = false; msg = ''; });
      await Navigator.push(context, MaterialPageRoute(builder: (_) => GameScreen(link)));
    } catch (e) {
      if (mounted) setState(() { busy = false; msg = 'Błąd: $e'; });
    }
  }

  @override
  Widget build(BuildContext context) => Scaffold(
        body: Center(
          child: Column(mainAxisSize: MainAxisSize.min, children: [
            const Text('Ping-Pong', style: TextStyle(fontSize: 32)),
            const SizedBox(height: 24),
            ElevatedButton(onPressed: busy ? null : () => go(connectWifi), child: const Text('Wi-Fi')),
            const SizedBox(height: 12),
            ElevatedButton(onPressed: busy ? null : () => go(connectBle), child: const Text('Bluetooth')),
            const SizedBox(height: 24),
            Padding(padding: const EdgeInsets.all(16), child: Text(msg)),
          ]),
        ),
      );
}

class GameScreen extends StatefulWidget {
  final Link link;
  const GameScreen(this.link, {super.key});
  @override
  State<GameScreen> createState() => _GameScreenState();
}

class _GameScreenState extends State<GameScreen> {
  GameState? s;
  final stats = Stats();
  StreamSubscription? sub;

  @override
  void initState() {
    super.initState();
    sub = widget.link.data.listen((d) {
      final g = GameState.parse(d);
      if (g == null) return;
      stats.onPacket(g);
      setState(() => s = g);
    });
  }

  @override
  void dispose() {
    sub?.cancel();
    widget.link.close();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final g = s;
    return Scaffold(
      backgroundColor: Colors.black,
      body: SafeArea(
        child: Column(children: [
          Padding(
            padding: const EdgeInsets.all(8),
            child: Text(
              g == null
                  ? 'Czekam na dane...'
                  : '${g.score2} : ${g.score1}${g.status == 2 ? "   KONIEC GRY" : ""}\n'
                      'pakiety ${stats.received}  utracone ${stats.lost}  '
                      'odstęp ${stats.meanMs.toStringAsFixed(1)} ms  jitter ${stats.jitterMs.toStringAsFixed(1)} ms',
              textAlign: TextAlign.center,
              style: const TextStyle(color: Colors.white, fontSize: 16),
            ),
          ),
          Expanded(child: Center(
            child: AspectRatio(
              aspectRatio: 400 / 600,
              child: CustomPaint(painter: BoardPainter(g), child: const SizedBox.expand()),
            ),
          )),
        ]),
      ),
    );
  }
}

class BoardPainter extends CustomPainter {
  final GameState? s;
  BoardPainter(this.s);

  @override
  void paint(Canvas c, Size size) {
    final k = size.width / 400; // plansza 400x600
    c.drawRect(Offset.zero & size, Paint()..color = const Color(0xFF111133));
    c.drawLine(Offset(0, size.height / 2), Offset(size.width, size.height / 2),
        Paint()..color = Colors.white24..strokeWidth = 2);
    final g = s;
    if (g == null) return;
    final pad = Paint()..color = Colors.white;
    // PADDLE_W=80, PADDLE_H=10, paletka 2 y=20, paletka 1 y=570
    c.drawRect(Rect.fromLTWH((g.p2X - 40) * k, 20 * k, 80 * k, 10 * k), pad);
    c.drawRect(Rect.fromLTWH((g.p1X - 40) * k, 570 * k, 80 * k, 10 * k), pad);
    c.drawCircle(Offset(g.ballX * k, g.ballY * k), 6 * k, Paint()..color = Colors.orange);
  }

  @override
  bool shouldRepaint(BoardPainter old) => true;
}