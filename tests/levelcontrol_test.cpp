#include "levelcontrol.h"
#include <QPushButton>
#include <QtTest>
class LevelTest : public QObject {
  Q_OBJECT
private slots:
  void boundsAndSteps() {
    LevelControl control;
    control.show();
    QSignalSpy changes(&control, &LevelControl::valueChanged);
    control.setValue(400);
    QCOMPARE(control.value(), 255);
    control.setValue(255);
    QCOMPARE(changes.count(), 1);
    control.findChild<QPushButton *>("levelStep-10")->click();
    QCOMPARE(control.value(), 245);
    control.findChild<QPushButton *>("levelPreset128")->click();
    QCOMPARE(control.value(), 128);
    control.setValue(-10);
    QCOMPARE(control.value(), 0);
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
  void drag() {
    LevelControl control;
    control.resize(220, 200);
    control.show();
    control.setValue(100);
    QTest::mousePress(&control, Qt::LeftButton, Qt::NoModifier,
                      QPoint(110, 70));
    QTest::mouseMove(&control, QPoint(110, 40));
    QTest::mouseRelease(&control, Qt::LeftButton, Qt::NoModifier,
                        QPoint(110, 40));
    QCOMPARE(control.value(), 130);
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
