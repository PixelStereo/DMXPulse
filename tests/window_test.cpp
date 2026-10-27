#include "window.h"
#include <QtTest>
class WindowTest : public QObject {
  Q_OBJECT
private slots:
  void receiveAndEdit() {
    // Reproduce a lighting sender that also listens on the local Art-Net port.
    QUdpSocket competitor;
    QVERIFY(competitor.bind(QHostAddress::LocalHost, 16454,
                            QUdpSocket::ShareAddress |
                                QUdpSocket::ReuseAddressHint));
    Window w(16454, 15568);
    w.show();
    for (auto combo : w.findChildren<QComboBox *>())
      for (int i = 0; i < combo->count(); ++i)
        if (combo->itemText(i).contains("[local]"))
          combo->setCurrentIndex(i);
    auto grid = w.findChild<QTableWidget *>();
    QVERIFY(grid);
    QPushButton *listen = nullptr, *send = nullptr;
    for (auto b : w.findChildren<QPushButton *>()) {
      if (b->text().contains("Démarrer"))
        listen = b;
      if (b->text().contains("Activer"))
        send = b;
    }
    QVERIFY(listen);
    QVERIFY(send);
    listen->click();
    QVERIFY(listen->isChecked());
    QUdpSocket socket;
    Frame f;
    f.universe = 0;
    f.sequence = 1;
    f.values = QByteArray(512, 0);
    f.values[0] = char(231);
    auto iface = w.findChildren<QComboBox *>().first();
    auto net =
        QNetworkInterface::interfaceFromIndex(iface->currentData().toInt());
    QHostAddress local;
    for (auto entry : net.addressEntries())
      if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
        local = entry.ip();
        break;
      }
    QVERIFY(!local.isNull());
    socket.writeDatagram(Protocol::encode(f), local, 16454);
    QTRY_VERIFY_WITH_TIMEOUT(grid->item(0, 0)->text().contains("231"), 2000);
    auto sources = w.findChild<QListWidget *>();
    QCOMPARE(sources->count(), 1);
    QCOMPARE(grid->rowCount(), 16);
    QCOMPARE(grid->columnCount(), 32);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n231"));
    QCOMPARE(grid->item(0, 0)->background().color(), QColor(231, 231, 231));
    QCOMPARE(grid->item(0, 0)->foreground().color(), QColor(Qt::black));
    QCOMPARE(grid->item(0, 1)->background().color(), QColor(Qt::black));
    auto boxes = w.findChildren<QSpinBox *>();
    QSpinBox *value = nullptr;
    for (auto b : boxes)
      if (b->maximum() == 255)
        value = b;
    QVERIFY(value);
    value->setValue(127);
    QTRY_VERIFY(grid->item(0, 0)->toolTip().contains("Sortie : 127"));
    if (!qEnvironmentVariable("DMXSCOPE_SCREENSHOT").isEmpty())
      w.grab().save(qEnvironmentVariable("DMXSCOPE_SCREENSHOT"));
    auto destination = w.findChild<QLineEdit *>();
    destination->setText("127.0.0.1");
    send->click();
    QVERIFY(send->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(grid->item(0, 0)->text().contains("127"), 2000);
    send->click();
    QVERIFY(!send->isChecked());
    QComboBox *protocol = nullptr;
    for (auto combo : w.findChildren<QComboBox *>())
      if (combo->findText("sACN") >= 0)
        protocol = combo;
    QVERIFY(protocol);
    protocol->setCurrentIndex(1);
    f.sacn = true;
    f.universe = 1;
    f.cid = QUuid::createUuid().toRfc4122();
    f.sequence = 1;
    f.values[0] = char(99);
    socket.writeDatagram(Protocol::encode(f), local, 15568);
    QTRY_VERIFY_WITH_TIMEOUT(grid->item(0, 0)->text().contains("\n99"), 2000);
    listen->click();
    QVERIFY(!listen->isChecked());
  }
};
QTEST_MAIN(WindowTest)
#include "window_test.moc"
