#include "window.h"
#include "platform.h"
namespace {

class ChannelDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter *p, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    p->save();
    p->fillRect(option.rect, index.data(Qt::BackgroundRole).value<QBrush>());
    const auto textColor =
        index.data(Qt::ForegroundRole).value<QBrush>().color();
    QColor channelColor = textColor;
    channelColor.setAlpha(190);
    p->setPen(channelColor);
    const auto lines = index.data().toString().split('\n');
    QRect top = option.rect.adjusted(1, 1, -1, -1);
    top.setHeight(top.height() / 2);
    QRect bottom = top.translated(0, top.height());
    QFont font = option.font;
    font.setPixelSize(qBound(10, top.height() - 2, 14));
    p->setFont(font);
    p->drawText(top, Qt::AlignCenter, lines.value(0));
    font.setBold(true);
    p->setPen(textColor);
    p->setFont(font);
    p->drawText(bottom, Qt::AlignCenter, lines.value(1));
    if (option.state & QStyle::State_Selected) {
      p->setPen(QPen(QColor("#a8c4b8"), 2));
      p->drawRect(option.rect.adjusted(1, 1, -2, -2));
    }
    p->restore();
  }
};
} // namespace

Window::Window(quint16 artPort, quint16 sacnPort, const QString &settingsFile)
    : artPort(artPort), sacnPort(sacnPort) {
  settings =
      settingsFile.isEmpty()
          ? std::make_unique<QSettings>(QSettings::IniFormat,
                                        QSettings::UserScope, "PixelStereo",
                                        "DMXPulse")
          : std::make_unique<QSettings>(settingsFile, QSettings::IniFormat);
  paintedInput.fill(-2);
  paintedOutput.fill(-2);
  compact = QGuiApplication::platformName() == "android" ||
            QGuiApplication::platformName() == "ios" ||
            qEnvironmentVariableIntValue("DMXPULSE_COMPACT_UI") == 1;
  columns = compact ? 8 : 32;
  clock.start();
  setWindowTitle("DMXPulse");
  resize(compact ? 390 : 1440, compact ? 844 : 900);
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
  subtitle->setVisible(!compact);
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
  if (compact) {
    iface->setMinimumWidth(0);
    listen->setMinimumWidth(0);
    universe->setMinimumWidth(0);
    toolbar->addWidget(field("Network interface", iface), 0, 0, 1, 3);
    toolbar->addWidget(field("Protocol", protocol), 1, 0);
    toolbar->addWidget(field("Universe", universe), 1, 1);
    toolbar->addWidget(field("Adapters", rescan), 1, 2);
    toolbar->addWidget(listen, 2, 0, 1, 3);
  } else {
    toolbar->addWidget(field("Network interface", iface), 0, 0);
    toolbar->setColumnStretch(0, 1);
    toolbar->addWidget(field("Adapters", rescan), 0, 1);
    toolbar->addWidget(field("Protocol", protocol), 0, 2);
    toolbar->addWidget(field("Universe", universe), 0, 3);
    toolbar->addWidget(field("Input", listen), 0, 4);
  }
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
  outputPanel = new QFrame;
  outputPanel->setObjectName("outputPanel");
  outputPanel->setMinimumWidth(compact ? 0 : 270);
  outputPanel->setMaximumWidth(compact ? QWIDGETSIZE_MAX : 340);
  auto editorWidget = new QWidget;
  auto editorLayout = new QVBoxLayout(editorWidget);
  editorLayout->setContentsMargins(14, 14, 14, 14);
  editorLayout->setSpacing(10);
  auto editorTitle = new QLabel("DMX output");
  editorTitle->setObjectName("section");
  editorLayout->addWidget(editorTitle);
  channel = new QSpinBox;
  channel->setObjectName("channel");
  channel->setRange(1, 512);
  inputValue = new QLabel("—");
  inputValue->setObjectName("reading");
  value = new QSpinBox;
  value->setObjectName("outputValue");
  value->setRange(0, 255);
  auto inspector = new QHBoxLayout;
  inspector->addWidget(field("Channel", channel));
  inspector->addWidget(field("Received", inputValue), 1);
  inspector->addWidget(field("Output", value));
  editorLayout->addLayout(inspector);
  encoder = new LevelControl;
  editorLayout->addWidget(encoder);
  followInput = new QCheckBox("Follow received input");
  followInput->setObjectName("followInput");
  followInput->setChecked(true);
  followInput->setToolTip("Unedited channels follow received data. Your edits "
                          "override individual channels.");
  editorLayout->addWidget(followInput);
  auto copy = new QPushButton("Release channel overrides");
  copy->setObjectName("releaseOverrides");
  copy->setToolTip(
      "Release all edits and follow the external received signal again");
  editorLayout->addWidget(copy);
  auto zero = new QPushButton("Zero all channels");
  zero->setObjectName("zeroOutput");
  zero->setToolTip("Set all 512 output channels to zero");
  editorLayout->addWidget(zero);
  auto editHint = new QLabel(
      "Edits change the output buffer. Enable transmission to send them.");
  editHint->setObjectName("muted");
  editHint->setWordWrap(true);
  editorLayout->addWidget(editHint);
  editorLayout->addStretch();
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
  mergeMode->setToolTip("HTP: highest received level. LTP: last source to "
                        "change each channel. sACN priority is applied first.");
  canvasHeader->addWidget(mergeMode);
  connect(mergeMode, &QComboBox::currentIndexChanged, this,
          [this] { render(); });
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
  grid = new QTableWidget(512 / columns, columns);
  grid->setObjectName("channelGrid");
  grid->horizontalHeader()->hide();
  grid->verticalHeader()->hide();
  grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  grid->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  grid->setItemDelegate(new ChannelDelegate(grid));
  grid->horizontalHeader()->setMinimumSectionSize(20);
  grid->verticalHeader()->setMinimumSectionSize(22);
  if (compact) {
    legend->hide();
    grid->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    grid->verticalHeader()->setDefaultSectionSize(48);
    grid->horizontalHeader()->setMinimumSectionSize(32);
    QScroller::grabGesture(grid->viewport(), QScroller::TouchGesture);
  }
  grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
  grid->setSelectionMode(QAbstractItemView::SingleSelection);
  grid->setSelectionBehavior(QAbstractItemView::SelectItems);
  for (int i = 0; i < 512; ++i) {
    auto item = new QTableWidgetItem;
    item->setTextAlignment(Qt::AlignCenter);
    grid->setItem(i / columns, i % columns, item);
  }
  grid->setCurrentCell(0, 0);
  canvasLayout->addWidget(grid, 1);
  split->addWidget(canvas);
  split->setStretchFactor(1, 1);
  split->setSizes({210, 940, 290});
  layout->addWidget(split, 1);
  auto outputScroll = new QScrollArea;
  outputScroll->setFrameShape(QFrame::NoFrame);
  outputScroll->setWidgetResizable(true);
  outputScroll->setWidget(editorWidget);
  auto transportLayout = new QVBoxLayout(outputPanel);
  transportLayout->setContentsMargins(10, 10, 10, 10);
  transportLayout->setSpacing(8);
  transportLayout->addWidget(outputScroll, 1);
  auto divider = new QFrame;
  divider->setFrameShape(QFrame::HLine);
  transportLayout->addWidget(divider);
  destination = new QLineEdit;
  destination->setObjectName("destination");
  destination->setPlaceholderText("IPv4 address, e.g. 127.0.0.1");
  priority = new QSpinBox;
  priority->setObjectName("priority");
  priority->setRange(0, 200);
  priority->setValue(100);
  send = new QPushButton("Enable transmission");
  send->setObjectName("transmit");
  send->setCheckable(true);
  auto destinationField = field("Art-Net destination", destination);
  transportLayout->addWidget(destinationField);
  auto priorityField = field("sACN priority", priority);
  transportLayout->addWidget(priorityField);
  priorityField->hide();
  connect(protocol, &QComboBox::currentIndexChanged, this,
          [destinationField, priorityField](int index) {
            destinationField->setVisible(index == 0);
            priorityField->setVisible(index == 1);
          });
  auto rate = new QLabel("512 channels · 30 Hz");
  rate->setObjectName("muted");
  transportLayout->addWidget(rate);
  transportLayout->addWidget(send);
  split->addWidget(outputPanel);
  split->setSizes({200, 900, 300});
  logDock = new QDockWidget("Network log", this);
  logDock->setObjectName("networkLogDock");
  auto logPanel = new QWidget;
  auto logLayout = new QVBoxLayout(logPanel);
  auto logTools = new QHBoxLayout;
  auto logHint = new QLabel("Latest 2,000 entries · timestamp · direction · "
                            "universe · source · changes");
  logHint->setObjectName("muted");
  logHint->setVisible(!compact);
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
  logDock->setVisible(!compact);
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
  showSources = view->addAction("Universes");
  showSources->setCheckable(true);
  showSources->setChecked(true);
  connect(showSources, &QAction::toggled, side, &QWidget::setVisible);
  outputToggle = view->addAction("DMX output");
  outputToggle->setObjectName("outputToggle");
  outputToggle->setCheckable(true);
  outputToggle->setChecked(true);
  connect(outputToggle, &QAction::toggled, outputPanel, &QWidget::setVisible);
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
                       "monitor.<br>PixelStereo · MIT license<br><a "
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
  if (compact) {
    side->setMinimumWidth(0);
    side->setMaximumWidth(QWIDGETSIZE_MAX);
    sources->setMaximumHeight(140);
    side->setMaximumHeight(210);
    split->setOrientation(Qt::Vertical);
    split->setMinimumHeight(1400);
    split->setSizes({180, 540, 680});
    grid->setMinimumHeight(400);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    takeCentralWidget();
    scroll->setWidget(root);
    setCentralWidget(scroll);
    metrics->setWordWrap(true);
    status->setWordWrap(true);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
  }
  connect(qGuiApp, &QGuiApplication::applicationStateChanged, this,
          [this](Qt::ApplicationState state) {
            if (!workspaceSaved && (state == Qt::ApplicationInactive ||
                                    state == Qt::ApplicationSuspended))
              saveSettings();
          });
  setStyleSheet(R"(
    QWidget { background:#171819; color:#e5e5e5; font-size:12px; }
    QLabel, QWidget#field { background:transparent; }
    QLabel#title { font-size:23px; font-weight:600; color:#fafafa; }
    QLabel#section { font-size:13px; font-weight:600; color:#ededed; }
    QLabel#muted { color:#96999d; }
    QLabel#reading { font-size:22px; font-weight:600; }
    QLabel#signal { background:#242628; border-radius:10px; padding:6px 12px; color:#b7babe; }
    QLabel#signal[inputState="live"] { background:#283e34; color:#c2e5d3; }
    QLabel#signal[inputState="stale"] { background:#443c2c; color:#e0c998; }
    QComboBox,QSpinBox,QLineEdit { background:#222426; border:1px solid #393c3f; border-radius:5px; padding:7px; min-height:17px; selection-background-color:#3c665e; }
    QComboBox:focus,QSpinBox:focus,QLineEdit:focus { border-color:#9cb8af; }
    QPushButton { background:#292c2e; border:1px solid #404346; border-radius:5px; padding:8px 12px; }
    QPushButton:hover { background:#35393b; }
    QPushButton:checked { background:#314c44; border-color:#6e9b8c; color:#ebfff6; }
    QPushButton#transmit:checked { background:#54452b; border-color:#b5965c; color:#fff0cf; }
    QWidget:disabled { color:#686c70; }
    QTableWidget { gridline-color:#303234; border:1px solid #36393b; background:#000; }
    QListWidget { background:#1d1f20; border:1px solid #333638; border-radius:5px; outline:0; }
    QListWidget::item { padding:10px 8px; border-bottom:1px solid #303234; }
    QListWidget::item:selected { background:#303a36; color:#f3fff9; }
    QFrame#outputPanel { background:#1e2021; border:1px solid #343739; border-radius:7px; }
    QToolButton { border:0; padding:2px; color:#b8bcbe; }
    QLabel#diagnostics { background:#202223; padding:10px; color:#a9afb2; border-radius:5px; }
    QMenuBar { background:#171819; } QMenu { background:#242628; } QMenu::item:selected { background:#3c4842; }
  )");
  if (compact)
    setStyleSheet(styleSheet() +
                  "QComboBox,QSpinBox,QLineEdit,QPushButton{min-height:26px;"
                  "font-size:14px} QLabel{font-size:12px}");
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
  connect(&art, &QUdpSocket::readyRead, this, [this] { receive(&art, false); });
  connect(&artUnicast, &QUdpSocket::readyRead, this,
          [this] { receive(&artUnicast, false); });
  connect(&sacn, &QUdpSocket::readyRead, this,
          [this] { receive(&sacn, true); });
  connect(protocol, &QComboBox::currentIndexChanged, this, [this] {
    endSend();
    universe->setRange(protocol->currentIndex() ? 1 : 0,
                       protocol->currentIndex() ? 63999 : 32767);
    if (listen->isChecked())
      join(protocol->currentIndex() ? universe->value() : 0);
    updateControls();
    render();
  });
  connect(universe, &QSpinBox::valueChanged, this, [this] {
    endSend();
    if (listen->isChecked())
      join(protocol->currentIndex() ? universe->value() : 0);
    render();
  });
  connect(sources, &QListWidget::itemClicked, this, [this](QListWidgetItem *i) {
    auto parts = i->data(Qt::UserRole).toString().split('/');
    protocol->setCurrentIndex(parts[0].toInt());
    universe->setValue(parts[1].toInt());
    render();
  });
  connect(grid, &QTableWidget::cellClicked, this,
          [this](int r, int c) { channel->setValue(r * columns + c + 1); });
  connect(channel, &QSpinBox::valueChanged, this, [this] {
    updating = true;
    value->setValue(quint8(output[channel->value() - 1]));
    encoder->setValue(value->value());
    updating = false;
    grid->setCurrentCell((channel->value() - 1) / columns,
                         (channel->value() - 1) % columns);
  });
  connect(grid->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex &current) {
            if (current.isValid())
              channel->setValue(current.row() * columns + current.column() + 1);
          });
  connect(value, &QSpinBox::valueChanged, this, [this](int v) {
    if (!updating)
      edit(v);
  });
  connect(encoder, &LevelControl::valueChanged, value, &QSpinBox::setValue);
  connect(encoder, &LevelControl::editRequested, this, [this] {
    value->setFocus();
    value->selectAll();
  });
  connect(followInput, &QCheckBox::toggled, this, [this] { render(); });
  connect(copy, &QPushButton::clicked, this, [this] {
    overrides.fill(false);
    followInput->setChecked(true);
    prepareOutput();
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
        for (int i = 0; i < 3; i++)
          transmit(true);
      sendTimer.stop();
      tx.close();
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
    network =
        QNetworkInterface::interfaceFromIndex(iface->currentData().toInt());
    if (!network.isValid() ||
        !network.flags().testFlag(QNetworkInterface::IsUp)) {
      send->setChecked(false);
      status->setText("Selected network interface is unavailable. Refresh the "
                      "adapter list.");
      return;
    }
    protocol->setEnabled(false);
    universe->setEnabled(false);
    iface->setEnabled(false);
    destination->setEnabled(false);
    priority->setEnabled(false);
    sent = 0;
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
  network = QNetworkInterface::interfaceFromIndex(iface->currentData().toInt());
  if (!network.isValid() ||
      !network.flags().testFlag(QNetworkInterface::IsUp)) {
    listen->setChecked(false);
    status->setText(
        "Selected network interface is unavailable. Click Refresh.");
    return;
  }
  auto flags = QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint;
  // A specific loopback listener avoids losing local unicast to a more
  // specific listener owned by another lighting application.
  QHostAddress artAddress =
      network.flags().testFlag(QNetworkInterface::IsLoopBack)
          ? QHostAddress(QHostAddress::LocalHost)
          : QHostAddress(QHostAddress::AnyIPv4);
  if (!art.bind(artAddress, artPort, flags) ||
      !sacn.bind(QHostAddress::AnyIPv4, sacnPort, flags)) {
    QString error = art.state() != QAbstractSocket::BoundState
                        ? art.errorString()
                        : sacn.errorString();
    stop();
    listen->setChecked(false);
    status->setText("Could not start listening: " + error);
    return;
  }
  if (!network.flags().testFlag(QNetworkInterface::IsLoopBack)) {
    for (const auto &entry : network.addressEntries())
      if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
        if (!artUnicast.bind(entry.ip(), artPort, flags)) {
          status->setText("Local unicast listener unavailable: " +
                          artUnicast.errorString());
        }
        break;
      }
  }
  art.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                      1024 * 1024);
  sacn.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                       1024 * 1024);
  datagrams = received = invalid = otherInterface = reordered = 0;
  streams.clear();
  heldInput.clear();
  heldExternal.clear();
  iface->setEnabled(false);
  status->setText(QString("Listening on Art-Net %1:%2 · sACN UDP %3")
                      .arg(art.localAddress().toString())
                      .arg(artPort)
                      .arg(sacnPort));
  if (protocol->currentIndex())
    join(universe->value());
  if (!setMulticastReceptionEnabled(true))
    status->setText("Could not acquire Wi-Fi multicast reception. Unicast is "
                    "still available.");
  listen->setText("Stop listening");
}
void Window::stop() {
  setMulticastReceptionEnabled(false);
  art.close();
  artUnicast.close();
  sacn.close();
  groups.clear();
  iface->setEnabled(!send->isChecked());
  listen->setText("Start listening");
  if (status)
    status->setText("Input stopped");
}
void Window::join(int u) {
  if (sacn.state() != QAbstractSocket::BoundState)
    return;
  for (int old : groups)
    sacn.leaveMulticastGroup(
        QHostAddress(QString("239.255.%1.%2").arg(old >> 8).arg(old & 255)),
        network);
  groups.clear();
  if (u < 1)
    return;
  if (sacn.joinMulticastGroup(
          QHostAddress(QString("239.255.%1.%2").arg(u >> 8).arg(u & 255)),
          network))
    groups.insert(u);
  else
    status->setText("Could not join multicast group: " + sacn.errorString());
}
void Window::receive(QUdpSocket *socket, bool isSacn) {
  int budget = 256;
  while (socket->hasPendingDatagrams() && budget--) {
    auto d = socket->receiveDatagram();
    if (!d.isValid())
      break;
    ++datagrams;
    if (d.interfaceIndex() != 0 &&
        d.interfaceIndex() != uint(network.index())) {
      bool local = false;
      for (auto entry : network.addressEntries())
        if (entry.ip() == d.senderAddress())
          local = true;
      // Same-machine traffic may be delivered through the loopback interface.
      if (d.senderAddress().isLoopback())
        local = true;
      if (!local) {
        ++otherInterface;
        continue;
      }
    }
    Frame f;
    if (!Protocol::decode(d.data(), isSacn, f)) {
      invalid++;
      log(QString("RX unsupported · %1:%2 · %3 bytes")
              .arg(d.senderAddress().toString())
              .arg(d.senderPort())
              .arg(d.data().size()));
      continue;
    }
    QString key = QString("%1/%2/%3/%4/%5")
                      .arg(isSacn ? "sACN" : "Art-Net")
                      .arg(f.universe)
                      .arg(isSacn ? QString() : d.senderAddress().toString(),
                           QString::fromLatin1(f.cid.toHex()))
                      .arg(isSacn ? 0 : d.senderPort());
    if (f.terminated) {
      log(QString("RX · sACN · U%1 · %2 · CID %3 · stream terminated")
              .arg(f.universe)
              .arg(d.senderAddress().toString(),
                   QString::fromLatin1(f.cid.toHex())));
      streams.remove(key);
      continue;
    }
    if (!streams.contains(key) && streams.size() >= 2048)
      continue;
    auto &s = streams[key];
    if (s.packets && clock.elapsed() - s.seen < 2500) {
      if (isSacn) {
        int diff = int(f.sequence) - int(s.frame.sequence);
        if (diff <= 0 && diff > -20) {
          ++reordered;
          continue;
        }
      } else if (f.sequence && s.frame.sequence) {
        int diff = (int(f.sequence) - int(s.frame.sequence) + 255) % 255;
        if (diff == 0 || diff > 127) {
          ++reordered;
          continue;
        }
      }
    }
    QStringList changes;
    int changedCount = 0;
    ++arrival;
    for (int ch = 0; ch < 512; ++ch) {
      if (!s.packets || s.frame.values[ch] != f.values[ch]) {
        s.changed[ch] = arrival;
        ++changedCount;
        if (changes.size() < 8)
          changes << QString("%1=%2").arg(ch + 1).arg(quint8(f.values[ch]));
      }
    }
    s.frame = f;
    s.ip = d.senderAddress().toString() +
           (isSacn ? QString() : QString(":%1").arg(d.senderPort()));
    s.own =
        s.own || (isSacn ? f.cid == cid
                         : d.senderPort() == lastTxPort && lastTxPort != 0 &&
                               d.senderAddress() == lastTxAddress);
    if (s.own)
      s.frame.name = "DMXPulse output";
    log(QString("RX · %1 · U%2 · %3%4 · seq %5%6 · %7")
            .arg(isSacn ? "sACN" : "Art-Net")
            .arg(f.universe)
            .arg(s.ip,
                 s.frame.name.isEmpty() ? QString() : " · " + s.frame.name)
            .arg(f.sequence)
            .arg(isSacn ? QString(" · priority %1 · CID %2")
                              .arg(f.priority)
                              .arg(QString::fromLatin1(f.cid.toHex()))
                        : QString())
            .arg(changes.isEmpty()
                     ? "unchanged"
                     : changes.join(", ") +
                           (changedCount > 8
                                ? QString(" … (%1 changed)").arg(changedCount)
                                : QString())));
    s.seen = clock.elapsed();
    s.packets++;
    received++;
  }
  // Drain in bounded batches, yielding to UI and timers between batches.
  if (socket->hasPendingDatagrams() && !pendingDrains.contains(socket)) {
    pendingDrains.insert(socket);
    QTimer::singleShot(0, this, [this, socket, isSacn] {
      pendingDrains.remove(socket);
      receive(socket, isSacn);
    });
  }
}
void Window::edit(int v) {
  prepareOutput();
  overrides.setBit(channel->value() - 1);
  output[channel->value() - 1] = char(v);
  encoder->setValue(v);
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
QByteArray Window::merged(bool externalOnly, bool *live) {
  if (live)
    *live = false;
  QList<const Stream *> candidates;
  int highest = -1;
  for (const auto &s : streams) {
    if (s.frame.sacn != bool(protocol->currentIndex()) ||
        s.frame.universe != universe->value() ||
        clock.elapsed() - s.seen >= 2500 || (externalOnly && s.own))
      continue;
    if (s.frame.sacn && s.frame.priority < highest)
      continue;
    if (s.frame.sacn && s.frame.priority > highest) {
      candidates.clear();
      highest = s.frame.priority;
    }
    candidates << &s;
  }
  if (candidates.isEmpty())
    return {};
  if (live)
    *live = true;
  QByteArray result(512, 0);
  std::array<quint64, 512> latest{};
  for (auto s : candidates)
    for (int ch = 0; ch < 512; ++ch)
      if (mergeMode->currentIndex() == 0) {
        if (quint8(s->frame.values[ch]) > quint8(result[ch]))
          result[ch] = s->frame.values[ch];
      } else if (s->changed[ch] >= latest[ch]) {
        result[ch] = s->frame.values[ch];
        latest[ch] = s->changed[ch];
      }
  return result;
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
void Window::render() {
  auto now = clock.elapsed();
  for (auto i = streams.begin(); i != streams.end();)
    if (now - i->seen > 60000)
      i = streams.erase(i);
    else
      ++i;
  QMap<QString, QList<const Stream *>> universes;
  for (const auto &stream : streams)
    universes[QString("%1/%2")
                  .arg(int(stream.frame.sacn))
                  .arg(stream.frame.universe)]
        << &stream;
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
      if (now - stream->seen < 2500)
        ++liveCount;
      origins << stream->ip + (stream->own ? " · DMXPulse output"
                                           : " · " + stream->frame.name);
    }
    auto f = i.value().first()->frame;
    QString text = QString("%1 · Universe %2  %3\n%4 source%5 · %6 live")
                       .arg(f.sacn ? "sACN" : "Art-Net")
                       .arg(f.universe)
                       .arg(liveCount ? "●" : "○")
                       .arg(i.value().size())
                       .arg(i.value().size() == 1 ? "" : "s")
                       .arg(liveCount);
    if (item->text() != text)
      item->setText(text);
    item->setToolTip(origins.join('\n'));
    item->setSelected(i.key() == current);
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
  int active = 0;
  for (int i = 0; i < 512; ++i) {
    int receivedValue = hasInput ? quint8(rx[i]) : -1;
    int out = quint8(output[i]);
    if (receivedValue > 0)
      ++active;
    auto item = grid->item(i / columns, i % columns);
    if (paintedInput[i] != receivedValue) {
      item->setText(QString("%1\n%2").arg(i + 1).arg(
          receivedValue < 0 ? QString("—") : QString::number(receivedValue)));
      int gray = qMax(0, receivedValue);
      item->setBackground(QColor(gray, gray, gray));
      item->setForeground(gray < 128 ? QColor(Qt::white) : QColor(Qt::black));
    }
    if (paintedInput[i] != receivedValue || paintedOutput[i] != out)
      item->setToolTip(QString("Channel %1\nReceived: %2\nOutput: %3")
                           .arg(i + 1)
                           .arg(receivedValue < 0
                                    ? QString("No data")
                                    : QString::number(receivedValue))
                           .arg(out));
    paintedInput[i] = receivedValue;
    paintedOutput[i] = out;
  }
  QString hint;
  if (listen->isChecked()) {
    if (!datagrams)
      hint = "No UDP packets. Check the sender destination and port sharing. "
             "For local Art-Net, select 127.0.0.1 and start listening after "
             "the sender.";
    else if (!received && otherInterface)
      hint = "Traffic arrived on a different interface. Select the adapter "
             "used by the sender.";
    else if (!received)
      hint = "UDP traffic detected, but no supported DMX data packets.";
    else if (!hasInput)
      hint = "DMX sources detected. Select a universe to inspect its received "
             "data.";
  }
  diagnostics->setText(
      QString("UDP %1  ·  DMX %2  ·  Other interface %3  ·  Unsupported %4\n%5")
          .arg(datagrams)
          .arg(received)
          .arg(otherInterface)
          .arg(invalid)
          .arg(hint) +
      QString("\nSequence drops: %1 · Sent: %2").arg(reordered).arg(sent));
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
  inputValue->setText(hasInput
                          ? QString::number(quint8(rx[channel->value() - 1]))
                          : QString("—"));
  inputValue->setToolTip(live ? "Live received value"
                              : "No live input; last value is retained");
  updating = true;
  value->setValue(quint8(output[channel->value() - 1]));
  encoder->setValue(value->value());
  updating = false;
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
                       .arg(received)
                       .arg(invalid)
                       .arg(sendTimer.isActive() ? "transmitting" : "off"));
}
void Window::transmit(bool terminated) {
  prepareOutput();
  Frame f;
  f.sacn = protocol->currentIndex();
  f.universe = universe->value();
  f.values = output;
  f.cid = cid;
  f.name = "DMXPulse";
  f.priority = priority->value();
  sequence =
      f.sacn ? quint8(sequence + 1) : (sequence == 255 ? 1 : sequence + 1);
  f.sequence = sequence;
  f.terminated = terminated;
  QHostAddress target = f.sacn ? QHostAddress(QString("239.255.%1.%2")
                                                  .arg(f.universe >> 8)
                                                  .arg(f.universe & 255))
                               : QHostAddress(destination->text());
  if (tx.state() != QAbstractSocket::BoundState) {
    QHostAddress local;
    for (auto a : network.addressEntries())
      if (a.ip().protocol() == QAbstractSocket::IPv4Protocol) {
        local = a.ip();
        break;
      }
    if (local.isNull() || !tx.bind(local, 0)) {
      status->setText("Could not bind output to the selected interface");
      if (!terminated)
        endSend();
      return;
    }
    tx.setMulticastInterface(network);
    tx.setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
    tx.setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
  }
  lastTxPort = tx.localPort();
  lastTxAddress = tx.localAddress();
  if (tx.writeDatagram(Protocol::encode(f), target,
                       f.sacn ? sacnPort : artPort) < 0) {
    if (terminated)
      return;
    status->setText("Transmission error: " + tx.errorString());
    endSend();
  } else if (!terminated) {
    ++sent;
    log(QString("TX · %1 · U%2 · DMXPulse → %3:%4 · seq %5")
            .arg(f.sacn ? "sACN" : "Art-Net")
            .arg(f.universe)
            .arg(target.toString())
            .arg(f.sacn ? sacnPort : artPort)
            .arg(f.sequence));
  }
}

void Window::updateControls() {
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
  if (!compact)
    restoreGeometry(s.value("window/geometry").toByteArray());
  split->restoreState(s.value("window/splitterV2").toByteArray());
  auto savedCid = s.value("network/cid").toByteArray();
  if (savedCid.size() == 16)
    cid = savedCid;
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
  channel->setValue(s.value("view/channel", 1).toInt());
  grid->setCurrentCell((channel->value() - 1) / columns,
                       (channel->value() - 1) % columns);
  showDiagnostics->setChecked(s.value("view/diagnostics", false).toBool());
  showSources->setChecked(compact ? true
                                  : s.value("view/sources", true).toBool());
  outputToggle->setChecked(compact ? true
                                   : s.value("view/output", true).toBool());
  mergeMode->setCurrentIndex(
      qBound(0, s.value("view/mergeMode", 0).toInt(), 1));
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
  s.setValue("window/splitterV2", split->saveState());
  s.setValue("network/cid", cid);
  s.setValue("network/interface", iface->currentData(Qt::UserRole + 1));
  s.setValue("network/protocol", protocol->currentIndex());
  s.setValue("network/universe", universe->value());
  s.setValue("output/destination", destination->text());
  s.setValue("output/priority", priority->value());
  s.setValue("output/values", output);
  s.remove("view/gridMode");
  s.remove("view/mobileTab");
  s.remove("view/inspectorTab");
  s.setValue("window/docks", saveState());
  s.setValue("log/paused", pauseLog->isChecked());
  s.setValue("view/mergeMode", mergeMode->currentIndex());
  s.setValue("output/followInput", followInput->isChecked());
  QByteArray mask(512, 0);
  for (int i = 0; i < 512; ++i)
    mask[i] = overrides.testBit(i) ? 1 : 0;
  s.setValue("output/overrides", mask);
  s.setValue("view/channel", channel->value());
  s.setValue("view/diagnostics", showDiagnostics->isChecked());
  s.setValue("view/sources", showSources->isChecked());
  s.setValue("view/output", outputToggle->isChecked());
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
