#pragma once
#include <pxs/ChannelGrid.h>
#include <pxs/DmxNet.h>
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
  quint16 artPort, sacnPort;
  pxs::dmx::Monitor monitor;
  pxs::dmx::Transmitter tx;
  QTimer refresh, sendTimer;
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
  QByteArray merged(bool externalOnly, bool *live = nullptr);
  void prepareOutput();
  void log(const QString &message);
  QLabel *sourceHint;
  quint64 sent = 0;
  bool workspaceSaved = false;
  bool autoSelectPending = true;
  void selectUniverse(int protocolIndex, int address);
  QByteArray output = QByteArray(512, 0);
  QComboBox *iface, *protocol;
  QSpinBox *universe, *priority;
  QLineEdit *destination;
  QPushButton *listen, *send;
  QListWidget *sources;
  pxs::dmx::ChannelGrid *grid;
  QAction *activeChannelsOnly;
  QLabel *status, *metrics, *signal, *universeTitle;
  QSplitter *split;
  QWidget *side, *destinationField, *priorityField;
  void applyOutput(const QVector<int> &channels, int level);
  void release(const QVector<int> &numbers);
  QAction *showDiagnostics, *showSources, *resumeReceive, *resumeSend;
  std::unique_ptr<QSettings> settings;
  void restoreSettings();
  void saveSettings();
  void updateControls();

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  QLabel *diagnostics;
  void start();
  void stop();
  void join(int);
  void render();
  pxs::dmx::Frame outputFrame();
  void transmit();
  void endSend();
};
