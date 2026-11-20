#include <pxs/Theme.h>
#include "window.h"
namespace {

class ChannelDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter *p, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    if (!index.data(Qt::UserRole).isValid())
      return;
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
    if (index.data(Qt::UserRole + 2).toBool()) {
      // This channel's output is set here: an amber bar along the bottom,
      // as long as its level
      const int out = index.data(Qt::UserRole + 1).toInt();
      const QColor amber("#d9b36c");
      QRect bar = option.rect.adjusted(1, 0, -1, -1);
      bar.setTop(bar.bottom() - 3);
      bar.setWidth(qMax(2, bar.width() * out / 255));
      p->fillRect(bar, amber);
      p->setPen(QPen(amber, 1));
      p->drawRect(option.rect.adjusted(0, 0, -1, -1));
    }
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
  columns = 32;
  clock.start();
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
  canvas = new QWidget;
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
  grid = new QTableWidget(512 / columns, columns);
  grid->setObjectName("channelGrid");
  grid->horizontalHeader()->hide();
  grid->verticalHeader()->hide();
  grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  grid->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  grid->setItemDelegate(new ChannelDelegate(grid));
  grid->horizontalHeader()->setMinimumSectionSize(20);
  grid->verticalHeader()->setMinimumSectionSize(22);
  grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
  grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
  grid->setSelectionBehavior(QAbstractItemView::SelectItems);
  for (int i = 0; i < 512; ++i) {
    visibleChannels.append(i);
    auto item = new QTableWidgetItem;
    item->setData(Qt::UserRole, i + 1);
    item->setTextAlignment(Qt::AlignCenter);
    grid->setItem(i / columns, i % columns, item);
  }
  grid->setCurrentCell(0, 0);
  grid->viewport()->installEventFilter(this);
  gridStack = new QStackedWidget;
  gridStack->addWidget(grid);
  emptyGrid = new QLabel;
  emptyGrid->setObjectName("emptyChannelGrid");
  emptyGrid->setAlignment(Qt::AlignCenter);
  emptyGrid->setWordWrap(true);
  gridStack->addWidget(emptyGrid);
  canvasLayout->addWidget(gridStack, 1);
  // The editor of the clicked channel: a small card beside its cell, with the
  // network's level above the level to send
  editor = new QFrame(canvas);
  editor->setObjectName("channelEditor");
  editor->setFixedWidth(330);
  editor->hide();
  auto editorLayout = new QVBoxLayout(editor);
  editorLayout->setContentsMargins(12, 10, 12, 12);
  editorLayout->setSpacing(8);
  auto editorHeader = new QHBoxLayout;
  editorTitle = new QLabel("Channel");
  editorTitle->setObjectName("section");
  editorHeader->addWidget(editorTitle);
  channel = new QSpinBox;
  channel->setObjectName("channel");
  channel->setRange(1, 512);
  channel->setToolTip("Channel (DMX address)");
  editorHeader->addWidget(channel);
  editorHeader->addStretch();
  editorState = new QLabel;
  editorState->setObjectName("muted");
  editorHeader->addWidget(editorState);
  auto closeButton = new QToolButton;
  closeButton->setText("✕");
  closeButton->setToolTip("Close (Esc)");
  closeButton->setCursor(Qt::PointingHandCursor);
  connect(closeButton, &QToolButton::clicked, this, [this] { closeEditor(); });
  editorHeader->addWidget(closeButton);
  editorLayout->addLayout(editorHeader);
  encoder = new LevelControl;
  editorLayout->addWidget(encoder);
  auto editorTools = new QHBoxLayout;
  editorTools->setSpacing(6);
  value = new QSpinBox;
  value->setObjectName("outputValue");
  value->setRange(0, 255);
  value->setToolTip("Output level, 0–255");
  editorTools->addWidget(value);
  for (const auto &preset : QList<QPair<QString, int>>{
           {"0", 0}, {"50%", 128}, {"Full", 255}}) {
    auto button = new QPushButton(preset.first);
    button->setObjectName(QString("levelPreset%1").arg(preset.second));
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(QString("Set output to %1").arg(preset.second));
    button->setStyleSheet("padding:6px 0;min-width:0");
    connect(button, &QPushButton::clicked, this,
            [this, level = preset.second] { value->setValue(level); });
    editorTools->addWidget(button);
  }
  releaseChannel = new QPushButton("Release");
  releaseChannel->setObjectName("releaseChannel");
  releaseChannel->setToolTip("Let this channel follow the received signal again");
  releaseChannel->setStyleSheet("padding:6px 10px;min-width:0");
  editorTools->addWidget(releaseChannel);
  editorLayout->addLayout(editorTools);
  auto closeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), editor);
  closeShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(closeShortcut, &QShortcut::activated, this, [this] { closeEditor(); });
  connect(activeChannelsOnly, &QAction::toggled, this, [this](bool active) {
    grid->verticalHeader()->setSectionResizeMode(
        active ? QHeaderView::Fixed : QHeaderView::Stretch);
    if (active)
      grid->verticalHeader()->setDefaultSectionSize(48);
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
    QFrame#channelEditor { background:@window; border:1px solid @focus; border-radius:8px; }
    QFrame#channelEditor QSpinBox { padding:4px 6px; min-height:15px; }
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
  connect(&art, &QUdpSocket::readyRead, this, [this] { receive(&art, false); });
  connect(&artUnicast, &QUdpSocket::readyRead, this,
          [this] { receive(&artUnicast, false); });
  connect(&sacn, &QUdpSocket::readyRead, this,
          [this] { receive(&sacn, true); });
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
  connect(channel, &QSpinBox::valueChanged, this, [this] {
    updating = true;
    value->setValue(quint8(output[channel->value() - 1]));
    encoder->setValue(value->value());
    updating = false;
    syncChannelSelection();
    if (editor->isVisible()) {
      placeEditor();
      render();
    }
  });
  connect(grid->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex &current) {
            if (current.isValid() && current.data(Qt::UserRole).isValid())
              channel->setValue(current.data(Qt::UserRole).toInt());
          });
  connect(grid->selectionModel(), &QItemSelectionModel::selectionChanged, this,
          [this] {
            if (editor->isVisible())
              render();
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
  connect(releaseChannel, &QPushButton::clicked, this, [this] {
    const auto numbers = selectedChannels();
    release(numbers);
    log(numbers.size() == 1
            ? QString("EDIT · Released channel %1").arg(numbers.first())
            : QString("EDIT · Released %1 channels").arg(numbers.size()));
    render();
  });
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
  autoSelectPending = !send->isChecked();
  heldInput.clear();
  heldExternal.clear();
  iface->setEnabled(false);
  status->setText(QString("Listening on Art-Net %1:%2 · sACN UDP %3")
                      .arg(art.localAddress().toString())
                      .arg(artPort)
                      .arg(sacnPort));
  if (protocol->currentIndex())
    join(universe->value());
  listen->setText("Stop listening");
}
void Window::stop() {
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
  for (int number : selectedChannels()) {
    overrides.setBit(number - 1);
    output[number - 1] = char(v);
  }
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
  const bool ltp = universeMergeModes.value(currentUniverseKey(), 0) == 1;
  std::array<quint64, 512> latest{};
  for (auto s : candidates)
    for (int ch = 0; ch < 512; ++ch)
      if (!ltp) {
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
void Window::syncChannelSelection() {
  const QSignalBlocker blocker(grid->selectionModel());
  const int position = visibleChannels.indexOf(channel->value() - 1);
  if (position < 0) {
    grid->clearSelection();
    grid->setCurrentIndex(QModelIndex());
  } else {
    const QModelIndex index =
        grid->model()->index(position / columns, position % columns);
    if (grid->selectionModel()->isSelected(index)) // inside a selection: keep it
      grid->selectionModel()->setCurrentIndex(index,
                                              QItemSelectionModel::NoUpdate);
    else
      grid->setCurrentIndex(index);
  }
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
QVector<int> Window::selectedChannels() const {
  QVector<int> numbers;
  for (const auto &index : grid->selectionModel()->selectedIndexes())
    if (index.data(Qt::UserRole).isValid())
      numbers << index.data(Qt::UserRole).toInt();
  std::sort(numbers.begin(), numbers.end());
  if (numbers.isEmpty())
    numbers << channel->value();
  return numbers;
}
bool Window::eventFilter(QObject *watched, QEvent *event) {
  if (watched == grid->viewport() && !grid->isHidden()) {
    auto *mouse = dynamic_cast<QMouseEvent *>(event);
    if (mouse && mouse->button() == Qt::LeftButton) {
      if (event->type() == QEvent::MouseButtonPress) {
        // Clicking the only channel already being edited will close its editor
        const QModelIndex under = grid->indexAt(mouse->position().toPoint());
        const auto numbers = selectedChannels();
        pressedOnlyOpen = editor->isVisible() && under.isValid() &&
                          mouse->modifiers() == Qt::NoModifier &&
                          numbers.size() == 1 &&
                          numbers.first() == under.data(Qt::UserRole).toInt();
      } else if (event->type() == QEvent::MouseButtonRelease) {
        QTimer::singleShot(0, this, [this] {
          const auto numbers = selectedChannels();
          if (pressedOnlyOpen && numbers.size() == 1)
            closeEditor();
          else if (grid->selectionModel()->hasSelection())
            openEditor(channel->value());
          pressedOnlyOpen = false;
        });
      }
    }
  }
  return QMainWindow::eventFilter(watched, event);
}
void Window::openEditor(int channelNumber) {
  editorChannel = channelNumber;
  editor->show();
  editor->raise();
  placeEditor();
  encoder->setFocus();
  render();
}
void Window::closeEditor() {
  editorChannel = 0;
  editor->hide();
  grid->setFocus();
}
void Window::placeEditor() {
  if (!editor->isVisible())
    return;
  const int position = visibleChannels.indexOf(channel->value() - 1);
  if (position < 0) { // filtered out of the view: nothing to stand beside
    closeEditor();
    return;
  }
  const QRect cell = grid->visualRect(
      grid->model()->index(position / columns, position % columns));
  const QPoint corner = grid->viewport()->mapTo(canvas, cell.topLeft());
  editor->adjustSize();
  const int gap = 8;
  int x = corner.x() + cell.width() + gap;
  if (x + editor->width() > canvas->width())
    x = corner.x() - editor->width() - gap;
  int y = corner.y() + cell.height() / 2 - editor->height() / 2;
  x = qBound(0, x, qMax(0, canvas->width() - editor->width()));
  y = qBound(0, y, qMax(0, canvas->height() - editor->height()));
  editor->move(x, y);
}
void Window::refreshEditor(const QByteArray &received, bool hasInput) {
  if (!editor->isVisible())
    return;
  const auto numbers = selectedChannels();
  const bool several = numbers.size() > 1;
  channel->setVisible(!several);
  editorTitle->setText(several ? QString("%1 channels").arg(numbers.size())
                               : QString("Channel"));
  // The network's level: the same on every selected channel, or none to show
  int net = hasInput ? quint8(received[numbers.first() - 1]) : -1;
  const QByteArray others = merged(true);
  int external = others.size() == 512 ? quint8(others[numbers.first() - 1]) : -1;
  bool anyOverride = false, allOverride = true;
  for (int number : numbers) {
    if (hasInput && quint8(received[number - 1]) != net)
      net = -1;
    if (others.size() == 512 && quint8(others[number - 1]) != external)
      external = -1;
    const bool set = overrides.testBit(number - 1);
    anyOverride |= set;
    allOverride &= set;
  }
  encoder->setNetwork(net, external);
  editorState->setText(several && net < 0 && hasInput ? "Levels differ"
                       : allOverride                  ? "● Set here"
                       : anyOverride                  ? "● Partly set"
                       : followInput->isChecked()     ? "Follows network"
                                                      : "Not following");
  releaseChannel->setEnabled(anyOverride);
  placeEditor();
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
  if (autoSelectPending && listen->isChecked() && !send->isChecked()) {
    const Stream *only = nullptr;
    int liveUniverses = 0;
    for (const auto &group : universes) {
      for (auto stream : group) {
        if (now - stream->seen < 2500) {
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
      if (now - stream->seen < 2500)
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
  const bool activeOnly = activeChannelsOnly->isChecked();
  const int displayColumns = activeOnly ? 16 : 32;
  int active = 0;
  QVector<int> shown;
  shown.reserve(512);
  for (int i = 0; i < 512; ++i) {
    const bool aboveZero = hasInput && quint8(rx[i]) > 0;
    if (aboveZero)
      ++active;
    if (!activeOnly || aboveZero)
      shown.append(i);
  }
  if (shown != visibleChannels || columns != displayColumns) {
    // Repack only when channel membership changes, preserving the inspector's
    // real DMX address even when that channel disappears from the filtered view.
    const QSignalBlocker blocker(grid->selectionModel());
    grid->setUpdatesEnabled(false);
    visibleChannels = shown;
    columns = displayColumns;
    grid->setColumnCount(columns);
    grid->setRowCount((shown.size() + columns - 1) / columns);
    for (int position = 0; position < grid->rowCount() * columns; ++position) {
      auto item = grid->item(position / columns, position % columns);
      if (!item) {
        item = new QTableWidgetItem;
        item->setTextAlignment(Qt::AlignCenter);
        grid->setItem(position / columns, position % columns, item);
      }
      if (position < shown.size()) {
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setData(Qt::UserRole, shown[position] + 1);
      } else {
        item->setFlags(Qt::NoItemFlags);
        item->setData(Qt::UserRole, QVariant());
        item->setText({});
        item->setToolTip({});
        item->setBackground(Qt::black);
      }
    }
    paintedInput.fill(-2);
    paintedOutput.fill(-2);
    syncChannelSelection();
    grid->setUpdatesEnabled(true);
  }
  emptyGrid->setText(hasInput ? "No channels above zero in this universe."
                              : "Waiting for received DMX data.");
  gridStack->setCurrentWidget(shown.isEmpty() ? static_cast<QWidget *>(emptyGrid)
                                             : static_cast<QWidget *>(grid));
  for (int position = 0; position < shown.size(); ++position) {
    const int i = shown[position];
    const int receivedValue = hasInput ? quint8(rx[i]) : -1;
    const int out = quint8(output[i]);
    const bool overridden = overrides.testBit(i);
    const int outState = out | (overridden ? 256 : 0);
    auto item = grid->item(position / columns, position % columns);
    if (paintedOutput[i] != outState) {
      item->setData(Qt::UserRole + 1, out);
      item->setData(Qt::UserRole + 2, overridden);
    }
    if (paintedInput[i] != receivedValue) {
      item->setText(QString("%1\n%2").arg(i + 1).arg(
          receivedValue < 0 ? QString("—") : QString::number(receivedValue)));
      int gray = qMax(0, receivedValue);
      item->setBackground(QColor(gray, gray, gray));
      item->setForeground(gray < 128 ? QColor(Qt::white) : QColor(Qt::black));
    }
    if (paintedInput[i] != receivedValue || paintedOutput[i] != outState)
      item->setToolTip(QString("Channel %1\nReceived: %2\nOutput: %3")
                           .arg(i + 1)
                           .arg(receivedValue < 0
                                    ? QString("No data")
                                    : QString::number(receivedValue))
                           .arg(out));
    paintedInput[i] = receivedValue;
    paintedOutput[i] = outState;
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
  updating = true;
  value->setValue(quint8(output[channel->value() - 1]));
  encoder->setValue(value->value());
  updating = false;
  refreshEditor(rx, hasInput);
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
  syncChannelSelection();
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
  s.setValue("network/cid", cid);
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
  s.setValue("view/channel", channel->value());
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
