#include <pxs/Theme.h>
#include "window.h"

Window::Window(quint16 artPort, quint16 sacnPort, const QString &settingsFile)
    : artPort(artPort), sacnPort(sacnPort) {
  monitor.setPorts(artPort, sacnPort);
  tx.setPorts(artPort, sacnPort);
  monitor.setTransmitter(&tx);
  monitor.setOwnLabel("DMXPulse output");
  connect(&monitor, &pxs::dmx::Monitor::logged, this, [this](const QString &line) { log(line); });
  connect(&monitor, &pxs::dmx::Monitor::problem, this, [this](const QString &text) { status->setText(text); });
  settings =
      settingsFile.isEmpty()
          ? std::make_unique<QSettings>(QSettings::IniFormat,
                                        QSettings::UserScope, "PixelStereo",
                                        "DMXPulse")
          : std::make_unique<QSettings>(settingsFile, QSettings::IniFormat);
  setWindowTitle("DMXPulse");
  resize(1440, 900);
  auto root = new QWidget;
  setCentralWidget(root);
  auto layout = new QVBoxLayout(root);
  layout->setContentsMargins(20, 18, 20, 16);
  layout->setSpacing(14);
  auto header = new QHBoxLayout;
  auto title = new QLabel("DMXPulse");
  title->setObjectName("title");
  header->addWidget(title);
  auto subtitle = new QLabel("Network DMX monitor");
  subtitle->setObjectName("muted");
  header->addWidget(subtitle);
  header->addStretch();
  signal = new QLabel("No signal");
  signal->setObjectName("signal");
  header->addWidget(signal);
  layout->addLayout(header);
  auto field = [](const QString &label, QWidget *control) {
    auto box = new QWidget;
    box->setObjectName("field");
    auto v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    auto text = new QLabel(label);
    text->setObjectName("muted");
    v->addWidget(text);
    v->addWidget(control);
    return box;
  };
  auto toolbar = new QGridLayout;
  toolbar->setSpacing(12);
  iface = new QComboBox;
  iface->setObjectName("networkInterface");
  iface->setMinimumWidth(280);
  auto populate = [this] {
    QString previous = iface->currentData(Qt::UserRole + 1).toString();
    iface->clear();
    for (const auto &n : QNetworkInterface::allInterfaces())
      if (n.flags().testFlag(QNetworkInterface::IsUp)) {
        QStringList addresses;
        for (const auto &entry : n.addressEntries())
          if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
            addresses << entry.ip().toString();
        if (addresses.isEmpty())
          continue;
        iface->addItem(
            QString("%1 · %2%3")
                .arg(n.humanReadableName(), addresses.join(", "),
                     n.flags().testFlag(QNetworkInterface::IsLoopBack)
                         ? " [local]"
                         : ""),
            n.index());
        iface->setItemData(iface->count() - 1, n.name(), Qt::UserRole + 1);
      }
    int index = iface->findData(previous, Qt::UserRole + 1);
    if (index >= 0)
      iface->setCurrentIndex(index);
    else
      for (int i = 0; i < iface->count(); ++i)
        if (!QNetworkInterface::interfaceFromIndex(iface->itemData(i).toInt())
                 .flags()
                 .testFlag(QNetworkInterface::IsLoopBack)) {
          iface->setCurrentIndex(i);
          break;
        }
  };
  populate();
  auto rescan = new QPushButton("Refresh");
  rescan->setToolTip("Find available network adapters");
  connect(rescan, &QPushButton::clicked, this, populate);
  protocol = new QComboBox;
  protocol->setObjectName("protocol");
  protocol->addItems({"Art-Net", "sACN"});
  universe = new QSpinBox;
  universe->setObjectName("universe");
  universe->setRange(0, 32767);
  universe->setMinimumWidth(100);
  listen = new QPushButton("Start listening");
  listen->setObjectName("listen");
  listen->setCheckable(true);
  listen->setMinimumWidth(140);
  listen->setShortcut(QKeySequence("Ctrl+L"));
  listen->setToolTip("Start or stop receiving DMX (Ctrl+L)");
  toolbar->addWidget(field("Network interface", iface), 0, 0);
  toolbar->setColumnStretch(0, 1);
  toolbar->addWidget(field("Adapters", rescan), 0, 1);
  toolbar->addWidget(field("Protocol", protocol), 0, 2);
  toolbar->addWidget(field("Universe", universe), 0, 3);
  toolbar->addWidget(field("Input", listen), 0, 4);
  destination = new QLineEdit;
  destination->setObjectName("destination");
  destination->setPlaceholderText("IPv4 address, e.g. 127.0.0.1");
  destination->setMinimumWidth(170);
  priority = new QSpinBox;
  priority->setObjectName("priority");
  priority->setRange(0, 200);
  priority->setValue(100);
  send = new QPushButton("Enable transmission");
  send->setObjectName("transmit");
  send->setCheckable(true);
  send->setMinimumWidth(170);
  send->setToolTip("Send all 512 channels of this universe at about 30 Hz. "
                   "Click a channel to set its output level.");
  destinationField = field("Art-Net destination", destination);
  priorityField = field("sACN priority", priority);
  toolbar->addWidget(destinationField, 0, 5);
  toolbar->addWidget(priorityField, 0, 5);
  toolbar->addWidget(field("Output", send), 0, 6);
  layout->addLayout(toolbar);
  split = new QSplitter;
  split->setObjectName("workspace");
  split->setHandleWidth(12);
  side = new QWidget;
  side->setMinimumWidth(210);
  side->setMaximumWidth(360);
  auto sideLayout = new QVBoxLayout(side);
  sideLayout->setContentsMargins(0, 0, 0, 0);
  sideLayout->setSpacing(0);
  auto universeLabel = new QLabel("Universes");
  universeLabel->setObjectName("section");
  sideLayout->addWidget(universeLabel);
  sideLayout->setSpacing(10);
  sources = new QListWidget;
  sources->setObjectName("sources");
  sources->setMinimumHeight(120);
  sources->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  sideLayout->addWidget(sources, 1);
  sourceHint = new QLabel("Universes appear as network data arrives.");
  sourceHint->setObjectName("muted");
  sourceHint->setWordWrap(true);
  sideLayout->addWidget(sourceHint);
  split->addWidget(side);
  auto canvas = new QWidget;
  auto canvasLayout = new QVBoxLayout(canvas);
  canvasLayout->setContentsMargins(0, 0, 0, 0);
  canvasLayout->setSpacing(10);
  auto canvasHeader = new QHBoxLayout;
  universeTitle = new QLabel("Universe 0");
  universeTitle->setObjectName("section");
  canvasHeader->addWidget(universeTitle);
  canvasHeader->addStretch();
  mergeMode = new QComboBox;
  mergeMode->setObjectName("mergeMode");
  mergeMode->addItems({"HTP", "LTP"});
  mergeMode->setToolTip(
      "Applies to this protocol and universe only. HTP: highest "
      "received level. LTP: last source to change each channel. "
      "sACN priority is applied first.");
  canvasHeader->addWidget(mergeMode);
  connect(mergeMode, &QComboBox::currentIndexChanged, this, [this](int mode) {
    universeMergeModes[currentUniverseKey()] = mode;
    log(QString("CONFIG · %1 · U%2 · merge %3")
            .arg(protocol->currentText())
            .arg(universe->value())
            .arg(mergeMode->currentText()));
    render();
  });
  auto legend = new QWidget;
  auto legendLayout = new QHBoxLayout(legend);
  legendLayout->setContentsMargins(0, 0, 0, 0);
  legendLayout->setSpacing(8);
  auto legendText = new QLabel("RECEIVED VALUES");
  legendText->setObjectName("muted");
  legendLayout->addWidget(legendText);
  auto black = new QLabel("0");
  black->setObjectName("muted");
  legendLayout->addWidget(black);
  auto ramp = new QFrame;
  ramp->setFixedSize(68, 7);
  ramp->setStyleSheet("border-radius:2px;background:qlineargradient(x1:0,y1:0,"
                      "x2:1,y2:0,stop:0 black,stop:1 white)");
  legendLayout->addWidget(ramp);
  auto white = new QLabel("255");
  white->setObjectName("muted");
  legendLayout->addWidget(white);
  canvasHeader->addWidget(legend);
  canvasLayout->addLayout(canvasHeader);
  activeChannelsOnly = new QAction("Active channels only", this);
  activeChannelsOnly->setObjectName("activeChannelsOnly");
  activeChannelsOnly->setCheckable(true);
  activeChannelsOnly->setShortcut(QKeySequence("Ctrl+Shift+A"));
  activeChannelsOnly->setToolTip(
      "Show only received channels above zero, keeping their DMX addresses");
  auto filterButton = new QToolButton;
  filterButton->setObjectName("activeChannelsButton");
  filterButton->setDefaultAction(activeChannelsOnly);
  auto filterRow = new QHBoxLayout;
  filterRow->addWidget(filterButton);
  filterRow->addStretch();
  followInput = new QCheckBox("Follow received input");
  followInput->setObjectName("followInput");
  followInput->setChecked(true);
  followInput->setToolTip("Channels you have not set follow the received data. "
                          "The ones you set keep their output level.");
  filterRow->addWidget(followInput);
  auto copy = new QPushButton("Release overrides");
  copy->setObjectName("releaseOverrides");
  copy->setToolTip(
      "Release all edits and follow the external received signal again");
  filterRow->addWidget(copy);
  auto zero = new QPushButton("Zero all");
  zero->setObjectName("zeroOutput");
  zero->setToolTip("Set all 512 output channels to zero");
  filterRow->addWidget(zero);
  canvasLayout->addLayout(filterRow);
  grid = new pxs::dmx::ChannelGrid;
  canvasLayout->addWidget(grid, 1);
  connect(activeChannelsOnly, &QAction::toggled, this, [this](bool active) {
    grid->setActiveOnly(active);
    render();
  });
  split->addWidget(canvas);
  split->setStretchFactor(1, 1);
  split->setSizes({210, 1230});
  layout->addWidget(split, 1);
  logDock = new QDockWidget("Network log", this);
  logDock->setObjectName("networkLogDock");
  auto logPanel = new QWidget;
  auto logLayout = new QVBoxLayout(logPanel);
  auto logTools = new QHBoxLayout;
  auto logHint = new QLabel("Latest 2,000 entries · timestamp · direction · "
                            "universe · source · changes");
  logHint->setObjectName("muted");
  logTools->addWidget(logHint, 1);
  pauseLog = new QCheckBox("Pause");
  pauseLog->setObjectName("pauseLog");
  logTools->addWidget(pauseLog);
  auto saveLog = new QPushButton("Save…");
  auto clearLog = new QPushButton("Clear");
  logTools->addWidget(saveLog);
  logTools->addWidget(clearLog);
  logLayout->addLayout(logTools);
  eventLog = new QPlainTextEdit;
  eventLog->setObjectName("networkLog");
  eventLog->setReadOnly(true);
  eventLog->setMaximumBlockCount(2000);
  eventLog->setLineWrapMode(QPlainTextEdit::NoWrap);
  eventLog->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  logLayout->addWidget(eventLog);
  logDock->setWidget(logPanel);
  addDockWidget(Qt::BottomDockWidgetArea, logDock);
  resizeDocks({logDock}, {160}, Qt::Vertical);
  logDock->setMinimumHeight(110);
  connect(clearLog, &QPushButton::clicked, this, [this] {
    pendingLog.clear();
    eventLog->clear();
  });
  connect(saveLog, &QPushButton::clicked, this, [this] {
    auto path =
        QFileDialog::getSaveFileName(this, "Save network log", "DMXPulse.log",
                                     "Log files (*.log);;All files (*)");
    if (path.isEmpty())
      return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      status->setText("Could not save log: " + file.errorString());
      return;
    }
    file.write(
        (eventLog->toPlainText() + "\n" + pendingLog.join('\n')).toUtf8());
  });
  metrics = new QLabel;
  metrics->setObjectName("muted");
  layout->addWidget(metrics);
  diagnostics = new QLabel;
  diagnostics->setObjectName("diagnostics");
  diagnostics->setWordWrap(true);
  layout->addWidget(diagnostics);
  status = new QLabel("Input stopped");
  status->setObjectName("muted");
  layout->addWidget(status);
  auto view = menuBar()->addMenu("View");
  view->addAction(activeChannelsOnly);
  view->addSeparator();
  showSources = view->addAction("Universes");
  showSources->setCheckable(true);
  showSources->setChecked(true);
  connect(showSources, &QAction::toggled, side, &QWidget::setVisible);
  view->addAction(logDock->toggleViewAction());
  showDiagnostics = view->addAction("Network diagnostics");
  showDiagnostics->setCheckable(true);
  showDiagnostics->setChecked(false);
  connect(showDiagnostics, &QAction::toggled, diagnostics,
          &QWidget::setVisible);
  diagnostics->hide();
  auto help = menuBar()->addMenu("Help");
  auto about = help->addAction("About DMXPulse");
  connect(about, &QAction::triggered, this, [this] {
    QMessageBox::about(this, "About DMXPulse",
                       "<b>DMXPulse</b><br>Art-Net and sACN network DMX "
                       "monitor.<br>PixelStereo · AGPL-3.0 license<br><a "
                       "href=\"https://github.com/PixelStereo/"
                       "DMXPulse\">Project and source code</a>");
  });
  auto legal = help->addAction("Licenses");
  connect(legal, &QAction::triggered, this, [this] {
    QDialog dialog(this);
    dialog.setWindowTitle("Licenses");
    dialog.resize(620, 480);
    auto layout = new QVBoxLayout(&dialog);
    auto text = new QTextBrowser;
    QString contents;
    for (const auto &path :
         {":/legal/LICENSE", ":/legal/THIRD-PARTY-NOTICES.md",
          ":/legal/licenses/LGPL-3.0.txt", ":/legal/licenses/GPL-3.0.txt"}) {
      QFile file(path);
      if (file.open(QIODevice::ReadOnly))
        contents += QString::fromUtf8(file.readAll()) + "\n\n";
    }
    text->setPlainText(contents);
    layout->addWidget(text);
    dialog.exec();
  });
  auto startup = menuBar()->addMenu("Startup");
  resumeReceive = startup->addAction("Resume listening");
  resumeReceive->setCheckable(true);
  resumeReceive->setChecked(true);
  resumeSend = startup->addAction("Resume transmission");
  resumeSend->setCheckable(true);
  resumeSend->setToolTip(
      "Restore the last transmission state, including output values");
  // The graphite look shared by every Pixel Stereo application (palette, Fusion, check boxes, tabs). This window's own
  // structure keeps its scope: written with the same colour tokens, on the window
  const pxs::Palette palette = pxs::graphite();
  pxs::apply(*static_cast<QApplication *>(QCoreApplication::instance()), palette);
  setStyleSheet(pxs::resolve(QStringLiteral(R"(
    QWidget { background:@base; color:@text; font-size:12px; }
    QLabel, QWidget#field { background:transparent; }
    QLabel#title { font-size:23px; font-weight:600; color:@text; }
    QLabel#section { font-size:13px; font-weight:600; color:@text; }
    QLabel#muted { color:@muted; }
    QLabel#reading { font-size:22px; font-weight:600; }
    QLabel#signal { background:@window; border-radius:10px; padding:6px 12px; color:@muted; }
    QLabel#signal[inputState="live"] { background:@button; color:#c2e5d3; }
    QLabel#signal[inputState="stale"] { background:@midlight; color:#e0c998; }
    QComboBox,QSpinBox,QLineEdit { background:@window; border:1px solid @midlight; border-radius:5px; padding:7px; min-height:17px; selection-background-color:@light; }
    QComboBox:focus,QSpinBox:focus,QLineEdit:focus { border-color:@focus; }
    QPushButton { background:@button; border:1px solid @midlight; border-radius:5px; padding:8px 12px; }
    QPushButton:hover { background:@midlight; }
    QPushButton:checked { background:@fill; border-color:@focus; color:@text; }
    QPushButton#transmit:checked { background:@midlight; border-color:#b5965c; color:#fff0cf; }
    QWidget:disabled { color:@border; }
    QTableWidget { gridline-color:@border; border:1px solid @midlight; background:#000; }
    QListWidget { background:@base; border:1px solid @midlight; border-radius:5px; outline:0; }
    QListWidget::item { padding:10px 8px; border-bottom:1px solid @midlight; }
    QListWidget::item:selected { background:@light; color:@text; }
    QToolButton { border:0; padding:2px; color:@muted; }
    QToolButton#activeChannelsButton { border:1px solid @midlight; border-radius:5px; padding:6px 10px; }
    QToolButton#activeChannelsButton:hover { background:@button; }
    QToolButton#activeChannelsButton:checked { background:@fill; border-color:@focus; color:@text; }
    QLabel#emptyChannelGrid { color:@muted; border:1px solid @midlight; border-radius:5px; padding:20px; }
    QLabel#diagnostics { background:@window; padding:10px; color:@muted; border-radius:5px; }
    QMenuBar { background:@base; } QMenu { background:@window; } QMenu::item:selected { background:@light; }
  )"), palette));
  connect(listen, &QPushButton::toggled, this, [this, rescan](bool) {
    rescan->setEnabled(!listen->isChecked() && !send->isChecked());
  });
  connect(send, &QPushButton::toggled, this, [this, rescan](bool) {
    rescan->setEnabled(!listen->isChecked() && !send->isChecked());
  });
  connect(listen, &QPushButton::toggled, this, [this](bool on) {
    if (on)
      start();
    else
      stop();
  });
  connect(protocol, &QComboBox::currentIndexChanged, this, [this] {
    autoSelectPending = false;
    endSend();
    universe->setRange(protocol->currentIndex() ? 1 : 0,
                       protocol->currentIndex() ? 63999 : 32767);
    if (listen->isChecked())
      join(protocol->currentIndex() ? universe->value() : 0);
    updateControls();
    render();
  });
  connect(universe, &QSpinBox::valueChanged, this, [this] {
    autoSelectPending = false;
    endSend();
    if (listen->isChecked())
      join(protocol->currentIndex() ? universe->value() : 0);
    render();
  });
  connect(sources, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem *item) {
            if (!item)
              return;
            const auto parts = item->data(Qt::UserRole).toString().split('/');
            if (parts.size() != 2)
              return;
            selectUniverse(parts[0].toInt(), parts[1].toInt());
            render();
          });
  connect(sources, &QListWidget::itemClicked, this,
          [this](QListWidgetItem *item) {
            autoSelectPending = false;
            if (!item ||
                item->data(Qt::UserRole).toString() == currentUniverseKey())
              return;
            const auto parts = item->data(Qt::UserRole).toString().split('/');
            if (parts.size() != 2)
              return;
            selectUniverse(parts[0].toInt(), parts[1].toInt());
            render();
          });
  connect(grid, &pxs::dmx::ChannelGrid::outputChosen, this,
          &Window::applyOutput);
  connect(grid, &pxs::dmx::ChannelGrid::releaseRequested, this,
          [this](const QVector<int> &numbers) {
            release(numbers);
            log(numbers.size() == 1
                    ? QString("EDIT · Released channel %1").arg(numbers.first())
                    : QString("EDIT · Released %1 channels")
                          .arg(numbers.size()));
            render();
          });
  connect(followInput, &QCheckBox::toggled, this, [this] { render(); });
  connect(copy, &QPushButton::clicked, this, [this] {
    QVector<int> set;
    for (int i = 0; i < 512; ++i)
      if (overrides.testBit(i))
        set << i + 1;
    followInput->setChecked(true);
    release(set);
    log("EDIT · Released all channel overrides");
    render();
  });
  connect(zero, &QPushButton::clicked, this, [this] {
    output.fill(0);
    overrides.fill(true);
    log("EDIT · All output channels set to 0");
    render();
  });
  connect(send, &QPushButton::toggled, this, [this](bool on) {
    if (!on) {
      if (sendTimer.isActive() && protocol->currentIndex())
        tx.end(outputFrame(), nullptr);
      sendTimer.stop();
      tx.stop();
      log("TX · Transmission stopped");
      protocol->setEnabled(true);
      universe->setEnabled(true);
      iface->setEnabled(!listen->isChecked());
      destination->setEnabled(true);
      priority->setEnabled(true);
      updateControls();
      return;
    }
    QHostAddress target(destination->text());
    if (!protocol->currentIndex() &&
        (target.isNull() ||
         target.protocol() != QAbstractSocket::IPv4Protocol)) {
      send->setChecked(false);
      status->setText("Enter a valid Art-Net destination IPv4 address.");
      return;
    }
    const auto network =
        QNetworkInterface::interfaceFromIndex(iface->currentData().toInt());
    QString txError;
    if (!network.isValid() ||
        !network.flags().testFlag(QNetworkInterface::IsUp)) {
      send->setChecked(false);
      status->setText("Selected network interface is unavailable. Refresh the "
                      "adapter list.");
      return;
    }
    if (!tx.start(network, &txError)) {
      send->setChecked(false);
      status->setText(txError);
      return;
    }
    protocol->setEnabled(false);
    universe->setEnabled(false);
    iface->setEnabled(false);
    destination->setEnabled(false);
    priority->setEnabled(false);
    sent = 0;
    autoSelectPending = false;
    log("TX · Transmission enabled");
    sendTimer.setTimerType(Qt::PreciseTimer);
    sendTimer.start(33);
    updateControls();
  });
  connect(&sendTimer, &QTimer::timeout, this, [this] { transmit(); });
  connect(&refresh, &QTimer::timeout, this, &Window::render);
  refresh.start(33);
  restoreSettings();
  updateControls();
  render();
}
Window::~Window() {
  if (!workspaceSaved)
    saveSettings();
  endSend();
  stop();
}
void Window::endSend() { send->setChecked(false); }
void Window::start() {
  if (iface->currentIndex() < 0) {
    listen->setChecked(false);
    status->setText("No active IPv4 interface. Click Refresh.");
    return;
  }
  const auto network =
      QNetworkInterface::interfaceFromIndex(iface->currentData().toInt());
  if (!network.isValid() || !network.flags().testFlag(QNetworkInterface::IsUp)) {
    listen->setChecked(false);
    status->setText(
        "Selected network interface is unavailable. Click Refresh.");
    return;
  }
  QString error;
  if (!monitor.start(network, &error)) {
    listen->setChecked(false);
    status->setText("Could not start listening: " + error);
    return;
  }
  autoSelectPending = !send->isChecked();
  heldInput.clear();
  heldExternal.clear();
  iface->setEnabled(false);
  status->setText(QString("Listening on Art-Net %1:%2 · sACN UDP %3")
                      .arg(monitor.artNetAddress())
                      .arg(artPort)
                      .arg(sacnPort));
  if (protocol->currentIndex())
    join(universe->value());
  listen->setText("Stop listening");
}
void Window::stop() {
  monitor.stop();
  iface->setEnabled(!send->isChecked());
  listen->setText("Start listening");
  if (status)
    status->setText("Input stopped");
}
void Window::join(int u) {
  QString error;
  if (!monitor.listenTo(u, &error))
    status->setText(error);
}
void Window::applyOutput(const QVector<int> &channels, int level) {
  prepareOutput();
  for (int number : channels) {
    overrides.setBit(number - 1);
    output[number - 1] = char(level);
  }
  render();
}
void Window::log(const QString &message) {
  if (pauseLog->isChecked())
    return;
  // A bounded queue keeps a busy network from consuming unbounded memory.
  if (pendingLog.size() >= 2048)
    pendingLog.removeFirst();
  QString line = message;
  line.replace('\n', " ↵ ").replace('\r', "");
  pendingLog << QDateTime::currentDateTime().toString(
                    "yyyy-MM-dd HH:mm:ss.zzz") +
                    " · " + line;
}
void Window::selectUniverse(int protocolIndex, int address) {
  const bool changed =
      protocol->currentIndex() != protocolIndex || universe->value() != address;
  if (changed)
    endSend();
  autoSelectPending = false;
  const QSignalBlocker protocolBlocker(protocol), universeBlocker(universe);
  protocol->setCurrentIndex(protocolIndex);
  universe->setRange(protocolIndex ? 1 : 0, protocolIndex ? 63999 : 32767);
  universe->setValue(address);
  if (changed && listen->isChecked())
    join(protocolIndex ? universe->value() : 0);
  updateControls();
}
QString Window::currentUniverseKey() const {
  return QString("%1/%2").arg(protocol->currentIndex()).arg(universe->value());
}
void Window::syncMergeMode() {
  const QSignalBlocker blocker(mergeMode);
  mergeMode->setCurrentIndex(universeMergeModes.value(currentUniverseKey(), 0));
}
QByteArray Window::merged(bool externalOnly, bool *live) {
  return monitor.merged(
      protocol->currentIndex(), universe->value(),
      universeMergeModes.value(currentUniverseKey(), 0) == 1
          ? pxs::dmx::MergeMode::LTP
          : pxs::dmx::MergeMode::HTP,
      externalOnly, live);
}
void Window::prepareOutput() {
  if (!followInput->isChecked())
    return;
  auto base = merged(true);
  auto key =
      QString("%1/%2").arg(protocol->currentIndex()).arg(universe->value());
  if (!base.isEmpty())
    heldExternal[key] = base;
  else
    base = heldExternal.value(key);
  if (base.size() != 512)
    return;
  for (int ch = 0; ch < 512; ++ch)
    if (!overrides.testBit(ch))
      output[ch] = base[ch];
}
// Back to the network: each channel follows the other devices again, or goes
// to zero when there is nothing received to follow
void Window::release(const QVector<int> &numbers) {
  for (int number : numbers)
    overrides.clearBit(number - 1);
  prepareOutput();
  const QString key = currentUniverseKey();
  const bool baseline = followInput->isChecked() &&
                        (!merged(true).isEmpty() ||
                         heldExternal.value(key).size() == 512);
  if (!baseline)
    for (int number : numbers)
      output[number - 1] = 0;
}
void Window::render() {
  monitor.prune();
  const auto stats = monitor.stats();
  const auto &streams = monitor.streams();
  QMap<QString, QList<const pxs::dmx::Stream *>> universes;
  for (const auto &stream : streams)
    universes[QString("%1/%2")
                  .arg(int(stream.frame.sacn))
                  .arg(stream.frame.universe)]
        << &stream;
  if (autoSelectPending && listen->isChecked() && !send->isChecked()) {
    const pxs::dmx::Stream *only = nullptr;
    int liveUniverses = 0;
    for (const auto &group : universes) {
      for (auto stream : group) {
        if (monitor.live(*stream)) {
          only = stream;
          ++liveUniverses;
          break;
        }
      }
      if (liveUniverses > 1)
        break;
    }
    if (liveUniverses == 1) {
      selectUniverse(int(only->frame.sacn), only->frame.universe);
      log(QString("VIEW · Automatically selected %1 · U%2")
              .arg(protocol->currentText())
              .arg(universe->value()));
    }
  }
  syncMergeMode();
  const QSignalBlocker sourceBlocker(sources);
  QHash<QString, QListWidgetItem *> items;
  for (int row = sources->count() - 1; row >= 0; --row) {
    auto item = sources->item(row);
    auto key = item->data(Qt::UserRole).toString();
    if (!universes.contains(key))
      delete sources->takeItem(row);
    else
      items.insert(key, item);
  }
  const QString current =
      QString("%1/%2").arg(protocol->currentIndex()).arg(universe->value());
  for (auto i = universes.cbegin(); i != universes.cend(); ++i) {
    auto item = items.value(i.key());
    if (!item) {
      item = new QListWidgetItem(sources);
      item->setData(Qt::UserRole, i.key());
    }
    int liveCount = 0;
    QStringList origins;
    for (auto stream : i.value()) {
      if (monitor.live(*stream))
        ++liveCount;
      origins << stream->ip + (stream->own ? " · DMXPulse output"
                                           : " · " + stream->frame.name);
    }
    auto f = i.value().first()->frame;
    QString text =
        QString("%1 · Universe %2  %3\n%4 source%5 · %6 live · %7")
            .arg(f.sacn ? "sACN" : "Art-Net")
            .arg(f.universe)
            .arg(liveCount ? "●" : "○")
            .arg(i.value().size())
            .arg(i.value().size() == 1 ? "" : "s")
            .arg(liveCount)
            .arg(universeMergeModes.value(i.key(), 0) ? "LTP" : "HTP");
    if (item->text() != text)
      item->setText(text);
    item->setToolTip(origins.join('\n'));
    item->setSelected(i.key() == current);
    if (i.key() == current)
      sources->setCurrentItem(item);
  }
  for (auto i = heldInput.begin(); i != heldInput.end();)
    if (!universes.contains(i.key()))
      i = heldInput.erase(i);
    else
      ++i;
  sourceHint->setVisible(streams.isEmpty());
  prepareOutput();
  bool live = false;
  QByteArray rx = merged(false, &live);
  if (!rx.isEmpty())
    heldInput[current] = rx;
  else
    rx = heldInput.value(current);
  live = live && listen->isChecked();
  const bool hasInput = rx.size() == 512;
  pxs::dmx::ChannelGrid::State state;
  state.received = hasInput ? rx : QByteArray();
  const QByteArray others = merged(true);
  state.others = others.size() == 512 ? others : QByteArray();
  state.output = output;
  state.overridden = overrides;
  state.following = followInput->isChecked();
  grid->setState(state);
  const int active = grid->activeCount();
  QString hint;
  if (listen->isChecked()) {
    if (!stats.datagrams)
      hint = "No UDP packets. Check the sender destination and port sharing. "
             "For local Art-Net, select 127.0.0.1 and start listening after "
             "the sender.";
    else if (!stats.received && stats.otherInterface)
      hint = "Traffic arrived on a different interface. Select the adapter "
             "used by the sender.";
    else if (!stats.received)
      hint = "UDP traffic detected, but no supported DMX data packets.";
    else if (!hasInput)
      hint = "DMX sources detected. Select a universe to inspect its received "
             "data.";
  }
  diagnostics->setText(
      QString("UDP %1  ·  DMX %2  ·  Other interface %3  ·  Unsupported %4\n%5")
          .arg(stats.datagrams)
          .arg(stats.received)
          .arg(stats.otherInterface)
          .arg(stats.invalid)
          .arg(hint) +
      QString("\nSequence drops: %1 · Sent: %2").arg(stats.reordered).arg(sent));
  const QString badgeState = live ? "live" : hasInput ? "stale" : "idle";
  if (signal->property("inputState").toString() != badgeState) {
    signal->setProperty("inputState", badgeState);
    signal->style()->unpolish(signal);
    signal->style()->polish(signal);
  }
  signal->setText(live       ? "● Live input"
                  : hasInput ? "○ Stale input"
                             : "○ No signal");
  universeTitle->setText(QString("Universe %1 · %2 received")
                             .arg(universe->value())
                             .arg(mergeMode->currentText()));
  if (!pendingLog.isEmpty()) {
    bool atEnd = eventLog->verticalScrollBar()->value() >=
                 eventLog->verticalScrollBar()->maximum() - 2;
    eventLog->appendPlainText(pendingLog.join('\n'));
    pendingLog.clear();
    if (atEnd)
      eventLog->verticalScrollBar()->setValue(
          eventLog->verticalScrollBar()->maximum());
  }
  metrics->setText(QString("%1  ·  Active channels: %2  ·  Received: %3  ·  "
                           "Unsupported: %4  ·  Output: %5")
                       .arg(live ? "Live" : "No signal / stale")
                       .arg(active)
                       .arg(stats.received)
                       .arg(stats.invalid)
                       .arg(sendTimer.isActive() ? "transmitting" : "off"));
}
pxs::dmx::Frame Window::outputFrame() {
  pxs::dmx::Frame f;
  f.sacn = protocol->currentIndex();
  f.universe = universe->value();
  f.values = output;
  f.name = "DMXPulse";
  f.priority = priority->value();
  return f;
}
void Window::transmit() {
  prepareOutput();
  const auto f = outputFrame();
  QString error;
  if (!tx.send(f, QHostAddress(destination->text()), &error)) {
    status->setText("Transmission error: " + error);
    endSend();
    return;
  }
  ++sent;
  log(QString("TX · %1 · U%2 · DMXPulse → %3:%4 · seq %5")
          .arg(f.sacn ? "sACN" : "Art-Net")
          .arg(f.universe)
          .arg(tx.lastTarget().toString())
          .arg(tx.lastPort())
          .arg(tx.lastSequence()));
}

void Window::updateControls() {
  destinationField->setVisible(protocol->currentIndex() == 0);
  priorityField->setVisible(protocol->currentIndex() == 1);
  bool transmitting = send->isChecked();
  send->setText(transmitting ? "Stop transmission" : "Enable transmission");
  priority->setEnabled(protocol->currentIndex() == 1 && !transmitting);
  destination->setEnabled(protocol->currentIndex() == 0 && !transmitting);
  universe->setToolTip(protocol->currentIndex()
                           ? "sACN universes: 1–63999"
                           : "Art-Net universes: 0–32767 (zero-based)");
  destination->setToolTip(
      protocol->currentIndex()
          ? "sACN output uses the universe multicast address"
          : "Unicast or broadcast IPv4 destination");
}
void Window::restoreSettings() {
  auto &s = *settings;
  restoreGeometry(s.value("window/geometry").toByteArray());
  split->restoreState(s.value("window/splitterV3").toByteArray());
  auto savedCid = s.value("network/cid").toByteArray();
  if (savedCid.size() == 16)
    tx.setCid(savedCid);
  auto savedInterface = s.value("network/interface").toString();
  int index = iface->findData(savedInterface, Qt::UserRole + 1);
  if (index >= 0)
    iface->setCurrentIndex(index);
  else if (!savedInterface.isEmpty())
    status->setText("Saved network interface is unavailable. Choose an "
                    "interface before listening.");
  protocol->setCurrentIndex(
      qBound(0, s.value("network/protocol", 0).toInt(), 1));
  universe->setValue(s.value("network/universe", 0).toInt());
  destination->setText(s.value("output/destination").toString());
  priority->setValue(s.value("output/priority", 100).toInt());
  auto savedOutput = s.value("output/values").toByteArray();
  if (savedOutput.size() == 512)
    output = savedOutput;
  grid->setCurrentChannel(s.value("view/channel", 1).toInt());
  activeChannelsOnly->setChecked(
      s.value("view/activeChannelsOnly", false).toBool());
  showDiagnostics->setChecked(s.value("view/diagnostics", false).toBool());
  showSources->setChecked(s.value("view/sources", true).toBool());
  s.beginGroup("view/mergeModes");
  for (const auto &key : s.allKeys()) {
    const auto parts = key.split('/');
    if (parts.size() != 2 || (parts[0] != "0" && parts[0] != "1"))
      continue;
    bool ok = false;
    int u = parts[1].toInt(&ok);
    bool sacn = parts[0] == "1";
    if (ok && u >= (sacn ? 1 : 0) && u <= (sacn ? 63999 : 32767))
      universeMergeModes[key] = qBound(0, s.value(key).toInt(), 1);
  }
  s.endGroup();
  // The former global choice belongs to the previously selected universe;
  // newly visited universes keep the default HTP mode.
  if (s.contains("view/mergeMode") &&
      !universeMergeModes.contains(currentUniverseKey()))
    universeMergeModes[currentUniverseKey()] =
        qBound(0, s.value("view/mergeMode").toInt(), 1);
  syncMergeMode();
  followInput->setChecked(s.value("output/followInput", true).toBool());
  auto mask = s.value("output/overrides").toByteArray();
  for (int i = 0; i < 512; ++i)
    overrides.setBit(i, mask.size() == 512 ? mask[i] != 0 : output[i] != 0);
  restoreState(s.value("window/docks").toByteArray());
  pauseLog->setChecked(s.value("log/paused", false).toBool());
  resumeReceive->setChecked(s.value("startup/receive", true).toBool());
  resumeSend->setChecked(s.value("startup/transmit", false).toBool());
  if (index >= 0) {
    if (resumeReceive->isChecked() &&
        s.value("session/listening", false).toBool())
      listen->setChecked(true);
    if (resumeSend->isChecked() &&
        s.value("session/transmitting", false).toBool())
      send->setChecked(true);
  }
}
void Window::saveSettings() {
  auto &s = *settings;
  s.setValue("window/geometry", saveGeometry());
  s.remove("window/splitterV2");
  s.setValue("window/splitterV3", split->saveState());
  s.setValue("network/cid", tx.cid());
  s.setValue("network/interface", iface->currentData(Qt::UserRole + 1));
  s.setValue("network/protocol", protocol->currentIndex());
  s.setValue("network/universe", universe->value());
  s.setValue("output/destination", destination->text());
  s.setValue("output/priority", priority->value());
  s.setValue("output/values", output);
  s.remove("view/gridMode");
  s.remove("view/inspectorTab");
  s.setValue("window/docks", saveState());
  s.setValue("log/paused", pauseLog->isChecked());
  s.remove("view/mergeMode");
  s.remove("view/mergeModes");
  s.beginGroup("view/mergeModes");
  for (auto i = universeMergeModes.cbegin(); i != universeMergeModes.cend();
       ++i)
    s.setValue(i.key(), i.value());
  s.endGroup();
  s.setValue("output/followInput", followInput->isChecked());
  QByteArray mask(512, 0);
  for (int i = 0; i < 512; ++i)
    mask[i] = overrides.testBit(i) ? 1 : 0;
  s.setValue("output/overrides", mask);
  s.setValue("view/channel", grid->currentChannel());
  s.setValue("view/activeChannelsOnly", activeChannelsOnly->isChecked());
  s.setValue("view/diagnostics", showDiagnostics->isChecked());
  s.setValue("view/sources", showSources->isChecked());
  s.remove("view/output");
  s.setValue("startup/receive", resumeReceive->isChecked());
  s.setValue("startup/transmit", resumeSend->isChecked());
  s.setValue("session/listening", listen->isChecked());
  s.setValue("session/transmitting", send->isChecked());
  s.sync();
  if (s.status() != QSettings::NoError)
    status->setText("Could not save workspace settings.");
}
void Window::closeEvent(QCloseEvent *event) {
  saveSettings();
  workspaceSaved = true;
  endSend();
  stop();
  refresh.stop();
  QMainWindow::closeEvent(event);
}
