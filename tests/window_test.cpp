#include "window.h"
#include <QtTest>
class WindowTest : public QObject {
  Q_OBJECT
private slots:
  void receiveAndEdit() {
    // Exact duplicate unicast binds are intentionally indeterminate on Windows.
    // Keep the receive regression isolated there; exercise Unix port sharing
    // here.
#ifndef Q_OS_WIN
    QUdpSocket competitor;
    QVERIFY(competitor.bind(QHostAddress::LocalHost, 16454,
                            QUdpSocket::ShareAddress |
                                QUdpSocket::ReuseAddressHint));
#endif
    QTemporaryDir settingsDir;
    Window w(16454, 15568, settingsDir.filePath("settings.ini"));
    w.show();
    for (auto combo : w.findChildren<QComboBox *>())
      for (int i = 0; i < combo->count(); ++i)
        if (combo->itemText(i).contains("[local]"))
          combo->setCurrentIndex(i);
    auto grid = w.findChild<QTableWidget *>();
    QVERIFY(grid);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n—"));
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
    for (int i = 3; i < 29; ++i)
      f.values[i] = char((i - 3) * 255 / 25);
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
    QVERIFY(w.findChildren<QTabWidget *>().isEmpty());
    w.findChild<QComboBox *>("mergeMode")->setCurrentIndex(1);
    auto boxes = w.findChildren<QSpinBox *>();
    QSpinBox *value = nullptr;
    for (auto b : boxes)
      if (b->maximum() == 255)
        value = b;
    QVERIFY(value);
    w.findChild<LevelControl *>("outputEncoder")->setValue(127);
    QCOMPARE(value->value(), 127);
    // Editing alone must never impersonate a received DMX packet.
    QTest::qWait(150);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n231"));
    QCOMPARE(grid->item(0, 0)->background().color(), QColor(231, 231, 231));
    QTRY_VERIFY(grid->item(0, 0)->toolTip().contains("Output: 127"));
    auto destination = w.findChild<QLineEdit *>("destination");
    destination->setText("127.0.0.1");
    send->click();
    QVERIFY(send->isChecked());
    QTRY_VERIFY_WITH_TIMEOUT(grid->item(0, 0)->text().contains("127"), 2000);
    QCOMPARE(sources->count(), 1);
    QVERIFY(sources->item(0)->text().contains("2 sources"));
    if (!qEnvironmentVariable("DMXSCOPE_SCREENSHOT").isEmpty())
      w.grab().save(qEnvironmentVariable("DMXSCOPE_SCREENSHOT"));
    send->click();
    QVERIFY(!send->isChecked());
    w.findChild<LevelControl *>("outputEncoder")->setValue(64);
    QTest::qWait(150);
    QCOMPARE(value->value(), 64);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n127"));
    QCOMPARE(grid->item(0, 0)->background().color(), QColor(127, 127, 127));
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
    // sACN accepts large forward sequence jumps and rollover, but ignores
    // recent duplicates.
    f.sequence = 200;
    f.values[0] = 100;
    socket.writeDatagram(Protocol::encode(f), local, 15568);
    QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n100"));
    f.sequence = 1;
    f.values[0] = 101;
    socket.writeDatagram(Protocol::encode(f), local, 15568);
    QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n101"));
    f.sequence = 0;
    f.values[0] = 102;
    socket.writeDatagram(Protocol::encode(f), local, 15568);
    QTest::qWait(100);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n101"));
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
      w.findChild<QComboBox *>("mergeMode")->setCurrentIndex(1);
      w.findChild<QPushButton *>("zeroOutput")->click();
      w.findChild<QSpinBox *>("channel")->setValue(47);
      w.findChild<QSpinBox *>("outputValue")->setValue(201);
      for (auto action : w.findChildren<QAction *>()) {
        if (action->text() == "Network diagnostics")
          action->setChecked(true);
        if (action->text() == "Universes")
          action->setChecked(false);
      }
      w.findChild<QAction *>("outputToggle")->setChecked(false);
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
      QCOMPARE(w.findChild<QComboBox *>("mergeMode")->currentIndex(), 1);
      QCOMPARE(w.findChild<QSpinBox *>("channel")->value(), 47);
      QSettings saved(file, QSettings::IniFormat);
      QCOMPARE(saved.value("output/overrides").toByteArray(),
               QByteArray(512, 1));
      QCOMPARE(w.findChild<QSpinBox *>("outputValue")->value(), 201);
      QVERIFY(w.findChildren<QTabWidget *>().isEmpty());
      QVERIFY(w.findChild<QPushButton *>("listen")->isChecked());
      QVERIFY(!w.findChild<QPushButton *>("transmit")->isChecked());
      QVERIFY(!w.findChild<QAction *>("outputToggle")->isChecked());
      for (auto action : w.findChildren<QAction *>()) {
        if (action->text() == "Network diagnostics")
          QVERIFY(action->isChecked());
        if (action->text() == "Universes")
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
  void cachedFramesAndSteadyRefresh() {
    QTemporaryDir dir;
    Window w(19454, 19568, dir.filePath("settings.ini"));
    w.show();
    auto iface = w.findChild<QComboBox *>("networkInterface");
    for (int i = 0; i < iface->count(); ++i)
      if (iface->itemText(i).contains("[local]"))
        iface->setCurrentIndex(i);
    w.findChild<QPushButton *>("listen")->setChecked(true);
    QVERIFY(w.findChild<QPushButton *>("listen")->isChecked());
    QUdpSocket socket;
    Frame f;
    f.universe = 0;
    f.sequence = 0;
    f.values = QByteArray(512, 0);
    f.values[0] = 55;
    auto grid = w.findChild<QTableWidget *>();
    socket.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost, 19454);
    QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n55"));
    f.sequence = 200;
    f.values[0] = 66;
    socket.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost, 19454);
    QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n66"));
    f.sequence = 199;
    f.values[0] = 77;
    socket.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost, 19454);
    QTest::qWait(100);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n66"));
    f.sequence = 201;
    f.universe = 1;
    f.values[0] = 88;
    socket.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost, 19454);
    QTRY_COMPARE(w.findChild<QListWidget *>()->count(), 2);
    w.findChild<QSpinBox *>("universe")->setValue(1);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n88"));
    QSignalSpy updates(grid->model(), &QAbstractItemModel::dataChanged);
    QTest::qWait(150);
    QCOMPARE(updates.count(), 0);
    // A bounded incoming burst must reach the final value while UI remains
    // alive.
    for (int i = 0; i < 400; ++i) {
      f.sequence = 0;
      f.values[0] = char(i % 256);
      socket.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost, 19454);
      if (i % 40 == 0)
        QCoreApplication::processEvents();
    }
    QTRY_COMPARE_WITH_TIMEOUT(grid->item(0, 0)->text(), QString("1\n143"),
                              3000);
    w.findChild<QPushButton *>("listen")->setChecked(false);
  }
  void channel268OnSelectedAdapter() {
    QTemporaryDir dir;
    Window w(20454, 20568, dir.filePath("settings.ini"));
    auto iface = w.findChild<QComboBox *>("networkInterface");
    QHostAddress address;
    for (int i = 0; i < iface->count(); ++i) {
      auto n =
          QNetworkInterface::interfaceFromIndex(iface->itemData(i).toInt());
      if (n.flags().testFlag(QNetworkInterface::IsLoopBack))
        continue;
      for (auto entry : n.addressEntries())
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
          iface->setCurrentIndex(i);
          address = entry.ip();
          break;
        }
      if (!address.isNull())
        break;
    }
    if (address.isNull()) {
      QTest::qSkip("No physical IPv4 adapter available", __FILE__, __LINE__);
      return;
    }
    w.findChild<QPushButton *>("listen")->setChecked(true);
    QVERIFY(w.findChild<QPushButton *>("listen")->isChecked());
    QUdpSocket other;
    Frame f;
    f.universe = 0;
    f.sequence = 1;
    f.values = QByteArray(512, 0);
    other.writeDatagram(Protocol::encode(f), address, 20454);
    auto grid = w.findChild<QTableWidget *>();
    QTRY_COMPARE(grid->item(8, 11)->text(), QString("268\n0"));
    w.findChild<QSpinBox *>("channel")->setValue(268);
    w.findChild<LevelControl *>("outputEncoder")->setValue(76);
    QTest::qWait(100);
    QCOMPARE(grid->item(8, 11)->text(), QString("268\n0"));
    w.findChild<QLineEdit *>("destination")->setText(address.toString());
    w.findChild<QPushButton *>("transmit")->setChecked(true);
    QTRY_COMPARE_WITH_TIMEOUT(grid->item(8, 11)->text(), QString("268\n76"),
                              2000);
    QCOMPARE(w.findChild<QListWidget *>()->count(), 1);
    w.findChild<QPushButton *>("transmit")->setChecked(false);
  }
  void onlyIncomingUniverseIsSelected_data() {
    QTest::addColumn<bool>("sacn");
    QTest::newRow("Art-Net") << false;
    QTest::newRow("sACN") << true;
  }
  void onlyIncomingUniverseIsSelected() {
    QFETCH(bool, sacn);
    QTemporaryDir dir;
    Window w(25454, 25568, dir.filePath("settings.ini"));
    auto iface = w.findChild<QComboBox *>("networkInterface");
    for (int i = 0; i < iface->count(); ++i)
      if (iface->itemText(i).contains("[local]"))
        iface->setCurrentIndex(i);
    auto protocol = w.findChild<QComboBox *>("protocol");
    auto universe = w.findChild<QSpinBox *>("universe");
    universe->setValue(511); // Previously saved universe does not match input.
    auto listen = w.findChild<QPushButton *>("listen");
    listen->setChecked(true);
    QVERIFY(listen->isChecked());
    QUdpSocket source;
    Frame f;
    f.sacn = sacn;
    f.universe = 37;
    f.cid = QUuid::createUuid().toRfc4122();
    f.values = QByteArray(512, 0);
    f.values[0] = 101;
    source.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost,
                         sacn ? 25568 : 25454);
    auto grid = w.findChild<QTableWidget *>();
    QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n101"));
    QCOMPARE(protocol->currentIndex(), int(sacn));
    QCOMPARE(universe->value(), 37);
    auto panel = w.findChild<QWidget *>("outputPanel");
    QCOMPARE(w.findChild<QLineEdit *>("destination")->isVisibleTo(panel),
             !sacn);
    QCOMPARE(w.findChild<QSpinBox *>("priority")->isVisibleTo(panel), sacn);
    auto list = w.findChild<QListWidget *>("sources");
    QVERIFY(list->currentItem());
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toString(),
             QString("%1/37").arg(int(sacn)));
    QVERIFY(list->currentItem()->isSelected());
    // Further arrivals must not keep moving the user's view.
    f.universe = 38;
    f.values[0] = 77;
    source.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost,
                         sacn ? 25568 : 25454);
    QTRY_COMPARE(list->count(), 2);
    QCOMPARE(universe->value(), 37);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n101"));
    // Restarting reception allows a new sole universe to be selected.
    listen->setChecked(false);
    listen->setChecked(true);
    f.universe = 39;
    source.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost,
                         sacn ? 25568 : 25454);
    QTRY_COMPARE(universe->value(), 39);
    QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n77"));
  }
  void universeListSelectsIndependentMergeModes() {
    QTemporaryDir dir;
    Window w(27454, 27568, dir.filePath("settings.ini"));
    w.show();
    auto iface = w.findChild<QComboBox *>("networkInterface");
    for (int i = 0; i < iface->count(); ++i)
      if (iface->itemText(i).contains("[local]"))
        iface->setCurrentIndex(i);
    w.findChild<QPushButton *>("listen")->setChecked(true);
    QUdpSocket a, b;
    Frame f;
    f.values = QByteArray(512, 0);
    for (int u : {1, 2}) {
      f.universe = u;
      f.values[0] = char(200);
      a.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost, 27454);
      f.values[0] = 40;
      b.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost, 27454);
    }
    auto list = w.findChild<QListWidget *>("sources");
    QTRY_COMPARE(list->count(), 2);
    auto universe = w.findChild<QSpinBox *>("universe");
    auto mode = w.findChild<QComboBox *>("mergeMode");
    auto grid = w.findChild<QTableWidget *>();
    auto clickUniverse = [&](int u) {
      for (int row = 0; row < list->count(); ++row) {
        auto item = list->item(row);
        if (item->data(Qt::UserRole).toString() == QString("0/%1").arg(u)) {
          QTest::mouseClick(list->viewport(), Qt::LeftButton, {},
                            list->visualItemRect(item).center());
          return;
        }
      }
      QFAIL("Universe not in list");
    };
    clickUniverse(1);
    QCOMPARE(universe->value(), 1);
    mode->setFocus();
    QTest::keyClick(mode, Qt::Key_End); // Select LTP using the real control.
    QCOMPARE(mode->currentIndex(), 1);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n40"));
    clickUniverse(2);
    QCOMPARE(universe->value(), 2);
    QCOMPARE(mode->currentIndex(), 0);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n200"));
    // Navigating the list with arrows must switch the actual monitored
    // universe.
    list->setFocus();
    QTest::keyClick(list, Qt::Key_Up);
    QCOMPARE(universe->value(), 1);
    QCOMPARE(mode->currentIndex(), 1);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n40"));
    QTest::keyClick(list, Qt::Key_Down);
    QCOMPARE(universe->value(), 2);
    QCOMPARE(mode->currentIndex(), 0);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n200"));
    clickUniverse(1);
    QVERIFY(list->item(0)->text().contains("LTP"));
    QVERIFY(list->item(1)->text().contains("HTP"));
  }
  void mergeModePerUniverse() {
    QTemporaryDir dir;
    const auto file = dir.filePath("settings.ini");
    {
      QSettings legacy(file, QSettings::IniFormat);
      legacy.setValue("view/mergeMode", 1);
      legacy.setValue("network/protocol", 0);
      legacy.setValue("network/universe", 0);
    }
    {
      Window w(24454, 24568, file);
      auto mode = w.findChild<QComboBox *>("mergeMode");
      auto protocol = w.findChild<QComboBox *>("protocol");
      auto universe = w.findChild<QSpinBox *>("universe");
      auto grid = w.findChild<QTableWidget *>();
      // Migrate the old global mode only onto the last selected universe.
      QCOMPARE(mode->currentIndex(), 1);
      universe->setValue(1);
      QCOMPARE(mode->currentIndex(), 0);
      mode->setCurrentIndex(1);
      universe->setValue(0);
      QCOMPARE(mode->currentIndex(), 1);
      mode->setCurrentIndex(0);
      auto iface = w.findChild<QComboBox *>("networkInterface");
      for (int i = 0; i < iface->count(); ++i)
        if (iface->itemText(i).contains("[local]"))
          iface->setCurrentIndex(i);
      auto listen = w.findChild<QPushButton *>("listen");
      listen->setChecked(true);
      QVERIFY(listen->isChecked());
      QUdpSocket a, b;
      Frame x, y;
      x.values = QByteArray(512, 0);
      x.values[0] = char(200);
      y = x;
      y.values[0] = 40;
      for (int u : {0, 1}) {
        universe->setValue(u);
        QCOMPARE(mode->currentIndex(), u);
        x.universe = y.universe = u;
        a.writeDatagram(Protocol::encode(x), QHostAddress::LocalHost, 24454);
        QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n200"));
        b.writeDatagram(Protocol::encode(y), QHostAddress::LocalHost, 24454);
        // Wait for B to be processed, even when HTP keeps the same level.
        QTRY_VERIFY(
            w.findChild<QPlainTextEdit *>("networkLog")
                ->toPlainText()
                .contains(
                    QString("U%1 · 127.0.0.1:%2").arg(u).arg(b.localPort())));
        QCOMPARE(grid->item(0, 0)->text(),
                 u == 0 ? QString("1\n200") : QString("1\n40"));
      }
      universe->setValue(0);
      QCOMPARE(grid->item(0, 0)->text(), QString("1\n200"));
      universe->setValue(1);
      QCOMPARE(grid->item(0, 0)->text(), QString("1\n40"));
      protocol->setCurrentIndex(1);
      QCOMPARE(mode->currentIndex(),
               0); // sACN U1 is independent of Art-Net U1.
      x.sacn = y.sacn = true;
      x.cid = QUuid::createUuid().toRfc4122();
      y.cid = QUuid::createUuid().toRfc4122();
      a.writeDatagram(Protocol::encode(x), QHostAddress::LocalHost, 24568);
      QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n200"));
      b.writeDatagram(Protocol::encode(y), QHostAddress::LocalHost, 24568);
      QTRY_VERIFY(w.findChild<QPlainTextEdit *>("networkLog")
                      ->toPlainText()
                      .contains(QString::fromLatin1(y.cid.toHex())));
      QCOMPARE(grid->item(0, 0)->text(), QString("1\n200"));
      universe->setValue(2);
      mode->setCurrentIndex(1);
      universe->setValue(1);
      QCOMPARE(mode->currentIndex(), 0);
      listen->setChecked(false);
    }
    {
      Window restored(24454, 24568, file);
      auto mode = restored.findChild<QComboBox *>("mergeMode");
      auto protocol = restored.findChild<QComboBox *>("protocol");
      auto universe = restored.findChild<QSpinBox *>("universe");
      QCOMPARE(protocol->currentIndex(), 1);
      QCOMPARE(universe->value(), 1);
      QCOMPARE(mode->currentIndex(), 0);
      universe->setValue(2);
      QCOMPARE(mode->currentIndex(), 1);
      protocol->setCurrentIndex(0);
      QCOMPARE(mode->currentIndex(), 0); // New Art-Net U2 defaults to HTP.
      universe->setValue(1);
      QCOMPARE(mode->currentIndex(), 1);
      universe->setValue(0);
      QCOMPARE(mode->currentIndex(), 0);
    }
    QSettings saved(file, QSettings::IniFormat);
    QVERIFY(!saved.contains("view/mergeMode"));
    QCOMPARE(saved.value("view/mergeModes/0/0").toInt(), 0);
    QCOMPARE(saved.value("view/mergeModes/0/1").toInt(), 1);
    QCOMPARE(saved.value("view/mergeModes/1/2").toInt(), 1);
  }
  void mergeAndLog() {
    QTemporaryDir dir;
    Window w(22454, 22568, dir.filePath("settings.ini"));
    auto iface = w.findChild<QComboBox *>("networkInterface");
    for (int i = 0; i < iface->count(); ++i)
      if (iface->itemText(i).contains("[local]"))
        iface->setCurrentIndex(i);
    w.findChild<QPushButton *>("listen")->setChecked(true);
    auto grid = w.findChild<QTableWidget *>();
    auto mode = w.findChild<QComboBox *>("mergeMode");
    QUdpSocket a, b;
    Frame x, y;
    x.values = QByteArray(512, 0);
    x.values[0] = char(200);
    x.values[1] = 50;
    y = x;
    y.values[0] = 40;
    y.values[1] = 100;
    auto sendFrame = [&](QUdpSocket &socket, const Frame &frame) {
      socket.writeDatagram(Protocol::encode(frame), QHostAddress::LocalHost,
                           22454);
    };
    sendFrame(a, x);
    QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n200"));
    sendFrame(b, y);
    QTRY_COMPARE(grid->item(0, 1)->text(), QString("2\n100"));
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n200"));
    QCOMPARE(w.findChild<QListWidget *>()->count(), 1);
    mode->setCurrentIndex(1);
    QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n40"));
    // An unchanged keepalive must not take LTP ownership back.
    sendFrame(a, x);
    QTest::qWait(100);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n40"));
    x.values[0] = 30;
    sendFrame(a, x);
    QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n30"));
    QCOMPARE(grid->item(0, 1)->text(), QString("2\n100"));
    auto log = w.findChild<QPlainTextEdit *>("networkLog");
    QTRY_VERIFY(log->toPlainText().contains(
        QString("127.0.0.1:%1").arg(a.localPort())));
    QVERIFY(log->toPlainText().contains(
        QString("127.0.0.1:%1").arg(b.localPort())));
    QVERIFY(log->toPlainText().contains(
        QDate::currentDate().toString("yyyy-MM-dd")));
    QVERIFY(log->toPlainText().contains("1=30"));
    QCOMPARE(log->maximumBlockCount(), 2000);
    // sACN priority is applied before either merge algorithm.
    w.findChild<QComboBox *>("protocol")->setCurrentIndex(1);
    x.sacn = y.sacn = true;
    x.universe = y.universe = 1;
    x.cid = QUuid::createUuid().toRfc4122();
    y.cid = QUuid::createUuid().toRfc4122();
    x.priority = 120;
    y.priority = 100;
    x.values[0] = 10;
    y.values[0] = char(250);
    a.writeDatagram(Protocol::encode(x), QHostAddress::LocalHost, 22568);
    b.writeDatagram(Protocol::encode(y), QHostAddress::LocalHost, 22568);
    QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n10"));
    mode->setCurrentIndex(0);
    QCOMPARE(grid->item(0, 0)->text(), QString("1\n10"));
    // A terminated source immediately yields to the remaining one.
    x.terminated = true;
    a.writeDatagram(Protocol::encode(x), QHostAddress::LocalHost, 22568);
    QTRY_COMPARE(grid->item(0, 0)->text(), QString("1\n250"));
  }
  void outputFollowsExternalWithoutFeedback() {
    QTemporaryDir dir;
    Window w(23454, 23568, dir.filePath("settings.ini"));
    auto iface = w.findChild<QComboBox *>("networkInterface");
    for (int i = 0; i < iface->count(); ++i)
      if (iface->itemText(i).contains("[local]"))
        iface->setCurrentIndex(i);
    w.findChild<QComboBox *>("mergeMode")->setCurrentIndex(1);
    w.findChild<QPushButton *>("listen")->setChecked(true);
    QUdpSocket source;
    Frame f;
    f.values = QByteArray(512, 0);
    f.values[4] = 96;
    source.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost, 23454);
    auto grid = w.findChild<QTableWidget *>();
    QTRY_COMPARE(grid->item(0, 4)->text(), QString("5\n96"));
    w.findChild<QSpinBox *>("channel")->setValue(268);
    w.findChild<LevelControl *>("outputEncoder")->setValue(76);
    QCOMPARE(grid->item(8, 11)->text(), QString("268\n0"));
    w.findChild<QLineEdit *>("destination")->setText("127.0.0.1");
    w.findChild<QPushButton *>("transmit")->setChecked(true);
    QTRY_COMPARE(grid->item(8, 11)->text(), QString("268\n76"));
    QCOMPARE(grid->item(0, 4)->text(), QString("5\n96"));
    f.values[4] = 20;
    source.writeDatagram(Protocol::encode(f), QHostAddress::LocalHost, 23454);
    QTRY_VERIFY(grid->item(0, 4)->toolTip().contains("Output: 20"));
    QCOMPARE(grid->item(8, 11)->text(), QString("268\n76"));
    w.findChild<QPushButton *>("releaseOverrides")->click();
    QTRY_COMPARE(grid->item(8, 11)->text(), QString("268\n0"));
    QCOMPARE(w.findChild<QListWidget *>()->count(), 1);
    w.findChild<QPushButton *>("transmit")->setChecked(false);
    // Returning output is excluded from the baseline even after TX stops.
    w.findChild<QPushButton *>("releaseOverrides")->click();
    QTRY_VERIFY(grid->item(8, 11)->toolTip().contains("Output: 0"));
  }
  void closePreservesSession() {
    QTemporaryDir dir;
    const auto file = dir.filePath("settings.ini");
    Window w(21454, 21568, file);
    w.show();
    auto iface = w.findChild<QComboBox *>("networkInterface");
    for (int i = 0; i < iface->count(); ++i)
      if (iface->itemText(i).contains("[local]"))
        iface->setCurrentIndex(i);
    w.findChild<QPushButton *>("listen")->setChecked(true);
    QVERIFY(w.findChild<QPushButton *>("listen")->isChecked());
    w.close();
    QVERIFY(QMetaObject::invokeMethod(
        qGuiApp, "applicationStateChanged", Qt::DirectConnection,
        Q_ARG(Qt::ApplicationState, Qt::ApplicationInactive)));
    QSettings saved(file, QSettings::IniFormat);
    QVERIFY(saved.value("session/listening").toBool());
  }
};
QTEST_MAIN(WindowTest)
#include "window_test.moc"
