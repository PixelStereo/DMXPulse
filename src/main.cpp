#include "window.h"
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setOrganizationName("PixelStereo");
  app.setApplicationName("DMXPulse");
  Window w;
  w.show();
  return app.exec();
}
