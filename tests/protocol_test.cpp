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
};
QTEST_GUILESS_MAIN(ProtocolTest)
#include "protocol_test.moc"
