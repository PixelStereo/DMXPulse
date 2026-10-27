#include "window.h"
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setOrganizationName("PixelStereo");
  app.setApplicationName("DMXPulse");
  app.setApplicationVersion(DMXPULSE_VERSION);
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addVersionOption();
  parser.addOption(
      {"smoke-test",
       "Start with temporary settings and exit without network activity."});
  parser.process(app);
  QTemporaryDir smokeSettings;
  if (parser.isSet("smoke-test")) {
    Window probe(0, 0, smokeSettings.filePath("workspace.ini"));
    probe.show();
    QTimer::singleShot(500, &app, &QApplication::quit);
    return app.exec();
  }
  Window w;
  w.show();
  return app.exec();
}
