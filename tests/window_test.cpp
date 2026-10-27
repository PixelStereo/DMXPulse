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
    QTemporaryDir settingsDir;
    Window w(16454, 15568, settingsDir.filePath("settings.ini"));
    w.show();
    for (auto combo : w.findChildren<QComboBox *>())
      for (int i = 0; i < combo->count(); ++i)
        if (combo->itemText(i).contains("[local]"))
          combo->setCurrentIndex(i);
    auto grid = w.findChild<QTableWidget *>();
    QVERIFY(grid);
    QPushButton *listen = nullptr, *send = nullptr;
    for (auto b : w.findChildren<QPushButton *>()) {
      if (b->text().contains("Start listening"))
        listen = b;
      if (b->text().contains("Enable transmission"))
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
    for(int i=3;i<29;++i)f.values[i]=char((i-3)*255/25);
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
    QTRY_VERIFY(grid->item(0, 0)->toolTip().contains("Output: 127"));
    if (!qEnvironmentVariable("DMXSCOPE_SCREENSHOT").isEmpty())
      w.grab().save(qEnvironmentVariable("DMXSCOPE_SCREENSHOT"));
    auto destination = w.findChild<QLineEdit *>("destination");
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
  void settingsRoundTrip() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto file = directory.filePath("settings.ini");
    QString interfaceName;
    {
      Window w(17454, 17568, file);
      auto iface = w.findChild<QComboBox *>("networkInterface");
      for (int i = 0; i < iface->count(); ++i)
        if (iface->itemText(i).contains("[local]"))
          iface->setCurrentIndex(i);
      interfaceName = iface->currentData(Qt::UserRole + 1).toString();
      w.findChild<QComboBox *>("protocol")->setCurrentIndex(1);
      w.findChild<QSpinBox *>("universe")->setValue(514);
      w.findChild<QSpinBox *>("priority")->setValue(120);
      w.findChild<QLineEdit *>("destination")->setText("192.168.1.255");
      w.findChild<QSpinBox *>("channel")->setValue(47);
      w.findChild<QSpinBox *>("outputValue")->setValue(201);
      for (auto action : w.findChildren<QAction *>()) {
        if (action->text() == "Network diagnostics")
          action->setChecked(true);
        if (action->text() == "Sources and inspector")
          action->setChecked(false);
      }
      w.findChild<QToolButton *>("outputToggle")->setChecked(false);
      w.findChild<QPushButton *>("listen")->setChecked(true);
      QVERIFY(w.findChild<QPushButton *>("listen")->isChecked());
    }
    {
      Window w(17454, 17568, file);
      QCOMPARE(w.findChild<QComboBox *>("networkInterface")
                   ->currentData(Qt::UserRole + 1)
                   .toString(),
               interfaceName);
      QCOMPARE(w.findChild<QComboBox *>("protocol")->currentIndex(), 1);
      QCOMPARE(w.findChild<QSpinBox *>("universe")->value(), 514);
      QCOMPARE(w.findChild<QSpinBox *>("priority")->value(), 120);
      QCOMPARE(w.findChild<QLineEdit *>("destination")->text(),
               QString("192.168.1.255"));
      QCOMPARE(w.findChild<QSpinBox *>("channel")->value(), 47);
      QCOMPARE(w.findChild<QSpinBox *>("outputValue")->value(), 201);
      QVERIFY(w.findChild<QPushButton *>("listen")->isChecked());
      QVERIFY(!w.findChild<QPushButton *>("transmit")->isChecked());
      QVERIFY(!w.findChild<QToolButton *>("outputToggle")->isChecked());
      for (auto action : w.findChildren<QAction *>()) {
        if (action->text() == "Network diagnostics")
          QVERIFY(action->isChecked());
        if (action->text() == "Sources and inspector")
          QVERIFY(!action->isChecked());
      }
      w.findChild<QPushButton *>("listen")->setChecked(false);
    }
  }
  void transmissionRestoreIsOptIn() {
    QTemporaryDir directory;
    const auto file = directory.filePath("settings.ini");
    {
      Window w(18454, 18568, file);
      auto iface = w.findChild<QComboBox *>("networkInterface");
      for (int i = 0; i < iface->count(); ++i)
        if (iface->itemText(i).contains("[local]"))
          iface->setCurrentIndex(i);
      w.findChild<QLineEdit *>("destination")->setText("127.0.0.1");
      w.findChild<QPushButton *>("transmit")->setChecked(true);
      QVERIFY(w.findChild<QPushButton *>("transmit")->isChecked());
    }
    {
      Window w(18454, 18568, file);
      QVERIFY(!w.findChild<QPushButton *>("transmit")->isChecked());
      for (auto action : w.findChildren<QAction *>())
        if (action->text() == "Resume transmission")
          action->setChecked(true);
      w.findChild<QPushButton *>("transmit")->setChecked(true);
    }
    {
      Window w(18454, 18568, file);
      QVERIFY(w.findChild<QPushButton *>("transmit")->isChecked());
      w.findChild<QPushButton *>("transmit")->setChecked(false);
    }
    QSettings s(file, QSettings::IniFormat);
    s.setValue("network/interface", "missing-test-adapter");
    s.setValue("session/transmitting", true);
    s.setValue("session/listening", true);
    s.sync();
    Window w(18454, 18568, file);
    QVERIFY(!w.findChild<QPushButton *>("transmit")->isChecked());
    QVERIFY(!w.findChild<QPushButton *>("listen")->isChecked());
  }
};
QTEST_MAIN(WindowTest)
#include "window_test.moc"
