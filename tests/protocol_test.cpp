#include "protocol.h"
#include <QtTest>
class ProtocolTest : public QObject {
  Q_OBJECT
private slots:
  void roundTrip() {
    for (bool sacn : {false, true}) {
      Frame f;
      f.sacn = sacn;
      f.universe = sacn ? 63999 : 32767;
      f.sequence = 255;
      f.cid = QUuid::createUuid().toRfc4122();
      f.name = "Test";
      f.values = QByteArray(512, 0);
      f.values[0] = char(255);
      f.values[511] = 123;
      Frame result;
      auto d = Protocol::encode(f);
      QVERIFY(Protocol::decode(d, sacn, result));
      QCOMPARE(result.values, f.values);
      QCOMPARE(result.universe, f.universe);
      QCOMPARE(result.sequence, f.sequence);
      if (sacn)
        QCOMPARE(result.cid, f.cid);
      for (int n = 0; n < d.size(); n++)
        QVERIFY(!Protocol::decode(d.left(n), sacn, result));
    }
  }
  void malformed() {
    Frame f;
    f.sacn = true;
    f.universe = 1;
    auto d = Protocol::encode(f);
    Frame out;
    d[123] = 3;
    QVERIFY(!Protocol::decode(d, true, out));
    d = Protocol::encode(f);
    d[112] = char(0x80);
    QVERIFY(!Protocol::decode(d, true, out));
    d = Protocol::encode(f);
    d[109] = 1;
    QVERIFY(!Protocol::decode(d, true, out));
  }
  void invalidOutputRanges() {
    Frame f;
    f.universe = -1;
    QVERIFY(Protocol::encode(f).isEmpty());
    f.universe = 32768;
    QVERIFY(Protocol::encode(f).isEmpty());
    f.sacn = true;
    f.universe = 0;
    QVERIFY(Protocol::encode(f).isEmpty());
    f.universe = 64000;
    QVERIFY(Protocol::encode(f).isEmpty());
    f.universe = 1;
    f.priority = 201;
    QVERIFY(Protocol::encode(f).isEmpty());
  }
  void malformedCorpus() {
    // Deterministic byte mutations and arbitrary buffers exercise decoder
    // bounds.
    quint32 state = 12345;
    auto next = [&state] {
      state = 1664525 * state + 1013904223;
      return state;
    };
    Frame out;
    for (int i = 0; i < 20000; ++i) {
      QByteArray d(int(next() % 1024), 0);
      for (auto &byte : d)
        byte = char(next() >> 24);
      Protocol::decode(d, false, out);
      Protocol::decode(d, true, out);
    }
    for (bool sacn : {false, true}) {
      Frame f;
      f.sacn = sacn;
      f.universe = 1;
      const auto valid = Protocol::encode(f);
      for (int i = 0; i < valid.size(); ++i) {
        auto changed = valid;
        changed[i] = char(quint8(changed[i]) ^ 0xff);
        if (Protocol::decode(changed, sacn, out))
          QCOMPARE(out.values.size(), 512);
      }
    }
  }
};
QTEST_GUILESS_MAIN(ProtocolTest)
#include "protocol_test.moc"
