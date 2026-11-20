#pragma once
#include "levelcontrol.h"
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
  std::array<int, 512> paintedInput{}, paintedOutput{};
  QLabel *sourceHint;
  quint64 sent = 0;
  bool workspaceSaved = false;
  bool autoSelectPending = true;
  void selectUniverse(int protocolIndex, int address);
  QByteArray output = QByteArray(512, 0);
  QComboBox *iface, *protocol;
  QSpinBox *universe, *channel, *value, *priority;
  QLineEdit *destination;
  QPushButton *listen, *send;
  QListWidget *sources;
  QTableWidget *grid;
  QStackedWidget *gridStack;
  QLabel *emptyGrid;
  QAction *activeChannelsOnly;
  QVector<int> visibleChannels;
  void syncChannelSelection();
  QLabel *status, *metrics, *signal, *universeTitle;
  QSplitter *split;
  int columns = 32;
  QWidget *side, *destinationField, *priorityField;
  // The editor that opens beside a clicked channel: the network's level and
  // the level to send
  QFrame *editor;
  QLabel *editorTitle, *editorState;
  QPushButton *releaseChannel;
  QWidget *canvas;
  int editorChannel = 0;
  void openEditor(int channelNumber);
  void closeEditor();
  void placeEditor();
  void release(const QVector<int> &numbers);
  QVector<int> selectedChannels() const; // DMX numbers, ascending
  bool pressedOnlyOpen = false;
  void refreshEditor(const QByteArray &received, bool hasInput);
  QAction *showDiagnostics, *showSources, *resumeReceive, *resumeSend;
  std::unique_ptr<QSettings> settings;
  void restoreSettings();
  void saveSettings();
  void updateControls();

protected:
  void closeEvent(QCloseEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  LevelControl *encoder;
  QLabel *diagnostics;
  bool updating = false;
  void start();
  void stop();
  void join(int);
  void render();
  pxs::dmx::Frame outputFrame();
  void transmit();
  void endSend();
  void edit(int);
};
