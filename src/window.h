#pragma once
#include "levelcontrol.h"
#include "protocol.h"
#include <QtNetwork>
#include <QtWidgets>
#include <array>
#include <memory>
class Window : public QMainWindow {
  Q_OBJECT
public:
  Window(quint16 artPort = 6454, quint16 sacnPort = 5568,
         const QString &settingsFile = {});
  ~Window();

private:
  struct Stream {
    Frame frame;
    QString ip;
    qint64 seen = 0;
    quint64 packets = 0;
  };
  quint16 artPort, sacnPort;
  QUdpSocket art, artUnicast, sacn, tx;
  QTimer refresh, sendTimer;
  QElapsedTimer clock;
  QMap<QString, Stream> streams;
  QString selected;
  std::array<int, 512> paintedInput{}, paintedOutput{};
  QLabel *sourceHint;
  QPushButton *copyInput;
  quint64 sent = 0, reordered = 0;
  QSet<QUdpSocket *> pendingDrains;
  bool awaitingOutputReturn = false;
  bool workspaceSaved = false;
  QByteArray output = QByteArray(512, 0), cid = QUuid::createUuid().toRfc4122();
  QComboBox *iface, *protocol;
  QSpinBox *universe, *channel, *value, *priority;
  QLineEdit *destination;
  QPushButton *listen, *send;
  QListWidget *sources;
  QTableWidget *grid;
  QLabel *status, *metrics, *signal, *universeTitle, *inputValue;
  QSplitter *split;
  int columns = 32;
  bool compact = false;
  QTabWidget *mobileTabs = nullptr;
  QWidget *side, *outputPanel;
  QToolButton *outputToggle;
  QAction *showDiagnostics, *showSources, *resumeReceive, *resumeSend;
  std::unique_ptr<QSettings> settings;
  void restoreSettings();
  void saveSettings();
  void updateControls();

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  LevelControl *encoder;
  QTabWidget *inspectorTabs;
  QSet<int> groups;
  QNetworkInterface network;
  quint8 sequence = 0;
  quint64 received = 0, invalid = 0, datagrams = 0, otherInterface = 0;
  QLabel *diagnostics;
  bool updating = false;
  void start();
  void stop();
  void receive(QUdpSocket *, bool);
  void join(int);
  void render();
  void transmit(bool terminated = false);
  void endSend();
  void edit(int);
  void selectStream();
};
