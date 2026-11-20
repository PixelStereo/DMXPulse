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
  bool autoSelectPending = true;
  void selectUniverse(int protocolIndex, int address);
  QByteArray output = QByteArray(512, 0), cid = QUuid::createUuid().toRfc4122();
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
