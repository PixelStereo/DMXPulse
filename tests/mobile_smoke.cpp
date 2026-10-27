#include "window.h"
#include <QTemporaryDir>
#include <cstdio>
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setOrganizationName("PixelStereo");
  app.setApplicationName("DMXPulseMobileTest");
  QTemporaryDir directory;
  Window window(26454, 26568, directory.filePath("workspace.ini"));
  window.show();
  QTimer::singleShot(600, &app, [&] {
    bool ok = true;
    for (bool sacn : {false, true}) {
      Frame f;
      f.sacn = sacn;
      f.universe = 1;
      f.values = QByteArray(512, 0);
      f.values[267] = 76;
      f.cid = QUuid::createUuid().toRfc4122();
      Frame received;
      ok &= Protocol::decode(Protocol::encode(f), sacn, received) &&
            received.values == f.values;
    }
    auto grid = window.findChild<QTableWidget *>("channelGrid");
    auto channel = window.findChild<QSpinBox *>("channel");
    auto encoder = window.findChild<LevelControl *>("outputEncoder");
    ok &= grid && channel && encoder;
    if (ok) {
      channel->setValue(268);
      encoder->setValue(76);
      ok &= grid->item(267 / grid->columnCount(), 267 % grid->columnCount())
                ->text() == QString("268\n—");
    }
    if (!qEnvironmentVariable("DMXPULSE_MOBILE_SCREENSHOT").isEmpty())
      window.grab().save(qEnvironmentVariable("DMXPULSE_MOBILE_SCREENSHOT"));
    qInfo("%s", ok ? "DMXPULSE_MOBILE_TEST_PASS" : "DMXPULSE_MOBILE_TEST_FAIL");
    std::fprintf(stderr, "%s\n",
                 ok ? "DMXPULSE_MOBILE_TEST_PASS"
                    : "DMXPULSE_MOBILE_TEST_FAIL");
    std::fflush(stderr);
    app.exit(ok ? 0 : 1);
  });
  return app.exec();
}
