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
    bool own = false;
    std::array<quint64, 512> changed{};
  };
  quint16 artPort, sacnPort;
  QUdpSocket art, artUnicast, sacn, tx;
  QTimer refresh, sendTimer;
  QElapsedTimer clock;
  QMap<QString, Stream> streams;
  quint64 arrival = 0;
  QMap<QString, QByteArray> heldInput, heldExternal;
  QBitArray overrides = QBitArray(512, false);
  QCheckBox *followInput;
  QComboBox *mergeMode;
  QMap<QString, int> universeMergeModes;
  QString currentUniverseKey() const;
  void syncMergeMode();
  QDockWidget *logDock;
  QPlainTextEdit *eventLog;
  QStringList pendingLog;
  QCheckBox *pauseLog;
  quint16 lastTxPort = 0;
  QHostAddress lastTxAddress;
  QByteArray merged(bool externalOnly, bool *live = nullptr);
  void prepareOutput();
  void log(const QString &message);
  std::array<int, 512> paintedInput{}, paintedOutput{};
  QLabel *sourceHint;
  quint64 sent = 0, reordered = 0;
  QSet<QUdpSocket *> pendingDrains;
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
  QWidget *side, *outputPanel;
  QAction *outputToggle;
  QAction *showDiagnostics, *showSources, *resumeReceive, *resumeSend;
  std::unique_ptr<QSettings> settings;
  void restoreSettings();
  void saveSettings();
  void updateControls();

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  LevelControl *encoder;
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
};
