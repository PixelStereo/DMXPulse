#include "levelcontrol.h"
#include <QtTest>
class LevelTest : public QObject {
  Q_OBJECT
private slots:
  void bounds() {
    LevelControl control;
    control.show();
    QSignalSpy changes(&control, &LevelControl::valueChanged);
    control.setValue(400);
    QCOMPARE(control.value(), 255);
    control.setValue(255);
    QCOMPARE(changes.count(), 1);
    control.setValue(-10);
    QCOMPARE(control.value(), 0);
  }
  void networkIsReadOnly() {
    LevelControl control;
    control.setValue(40);
    control.setNetwork(200, 120);
    QCOMPARE(control.network(), 200);
    QCOMPARE(control.others(), 120);
    QCOMPARE(control.value(), 40);
    control.setNetwork(-5);
    QCOMPARE(control.network(), -1);
    QCOMPARE(control.others(), -1);
  }
  void keyboard() {
    LevelControl control;
    control.show();
    control.setFocus();
    QTest::keyClick(&control, Qt::Key_Up);
    QCOMPARE(control.value(), 1);
    QTest::keyClick(&control, Qt::Key_PageUp);
    QCOMPARE(control.value(), 11);
    QTest::keyClick(&control, Qt::Key_End);
    QCOMPARE(control.value(), 255);
    QTest::keyClick(&control, Qt::Key_Home);
    QCOMPARE(control.value(), 0);
  }
  void clickAndDrag() {
    LevelControl control;
    control.resize(280, control.height());
    control.show();
    const int y = control.height() - 14; // the output bar is the lower one
    // Left end: zero; right end of the bar: full
    QTest::mouseClick(&control, Qt::LeftButton, Qt::NoModifier,
                      QPoint(74, y));
    QCOMPARE(control.value(), 0);
    QTest::mouseClick(&control, Qt::LeftButton, Qt::NoModifier,
                      QPoint(280 - 34 - 8, y));
    QCOMPARE(control.value(), 255);
    const int middle = 74 + (280 - 74 - 34 - 8) / 2;
    QTest::mousePress(&control, Qt::LeftButton, Qt::NoModifier,
                      QPoint(middle, y));
    QVERIFY(qAbs(control.value() - 128) <= 2);
    QTest::mouseMove(&control, QPoint(74, y));
    QTest::mouseRelease(&control, Qt::LeftButton, Qt::NoModifier,
                        QPoint(74, y));
    QCOMPARE(control.value(), 0);
  }
  void wheel() {
    LevelControl control;
    control.setValue(100);
    QWheelEvent coarse(QPointF(110, 50), QPointF(110, 50), QPoint(),
                       QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                       Qt::ScrollUpdate, false);
    QCoreApplication::sendEvent(&control, &coarse);
    QCOMPARE(control.value(), 104);
    QWheelEvent fine(QPointF(110, 50), QPointF(110, 50), QPoint(),
                     QPoint(0, 120), Qt::NoButton, Qt::ShiftModifier,
                     Qt::ScrollUpdate, false);
    QCoreApplication::sendEvent(&control, &fine);
    QCOMPARE(control.value(), 105);
  }
};
QTEST_MAIN(LevelTest)
#include "levelcontrol_test.moc"
