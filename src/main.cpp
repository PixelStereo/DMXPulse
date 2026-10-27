#include "window.h"
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setApplicationName("DMXPulse");
  Window w;
  w.show();
  return app.exec();
}
