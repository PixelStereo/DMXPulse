#include "window.h"
namespace {
constexpr int columns = 32;
class ChannelDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void paint(QPainter *p, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    p->save();
    p->fillRect(option.rect, index.data(Qt::BackgroundRole).value<QBrush>());
    p->setPen(index.data(Qt::ForegroundRole).value<QBrush>().color());
    const auto lines = index.data().toString().split('\n');
    QRect top = option.rect.adjusted(1, 1, -1, -1);
    top.setHeight(top.height() / 2);
    QRect bottom = top.translated(0, top.height());
    QFont font = option.font;
    font.setPixelSize(qBound(10, top.height() - 2, 14));
    p->setFont(font);
    p->drawText(top, Qt::AlignCenter, lines.value(0));
    font.setBold(true);
    p->setFont(font);
    p->drawText(bottom, Qt::AlignCenter, lines.value(1));
    if (option.state & QStyle::State_Selected) {
      p->setPen(QPen(QColor("#31c9a0"), 2));
      p->drawRect(option.rect.adjusted(1, 1, -2, -2));
    }
    p->restore();
  }
};
} // namespace

Window::Window(quint16 artPort, quint16 sacnPort)
    : artPort(artPort), sacnPort(sacnPort) {
  clock.start();
  setWindowTitle("DMXPulse • Art-Net / sACN");
  resize(1500, 920);
  auto root = new QWidget;
  setCentralWidget(root);
  auto layout = new QVBoxLayout(root);
  layout->setContentsMargins(22, 18, 22, 18);
  layout->setSpacing(12);
  auto title = new QLabel("DMXPULSE   /   MONITEUR RÉSEAU");
  title->setStyleSheet("font-size:24px;font-weight:700;color:#62dfbc");
  layout->addWidget(title);
  auto bar = new QHBoxLayout;
  iface = new QComboBox;
  for (auto n : QNetworkInterface::allInterfaces())
    if (n.flags().testFlag(QNetworkInterface::IsUp)) {
      QStringList addresses;
      for (auto entry : n.addressEntries())
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
          addresses << entry.ip().toString();
      if (!addresses.isEmpty())
        iface->addItem(
            QString("%1 (%2) — %3%4")
                .arg(n.humanReadableName(), n.name(), addresses.join(", "),
                     n.flags().testFlag(QNetworkInterface::IsLoopBack)
                         ? " [local]"
                         : ""),
            n.index());
    }
  for (int i = 0; i < iface->count(); ++i)
    if (!QNetworkInterface::interfaceFromIndex(iface->itemData(i).toInt())
             .flags()
             .testFlag(QNetworkInterface::IsLoopBack)) {
      iface->setCurrentIndex(i);
      break;
    }
  auto rescan = new QPushButton("Actualiser");
  connect(rescan, &QPushButton::clicked, this, [this] {
    int previous = iface->currentData().toInt();
    iface->clear();
    for (auto n : QNetworkInterface::allInterfaces())
      if (n.flags().testFlag(QNetworkInterface::IsUp)) {
        QStringList addresses;
        for (auto entry : n.addressEntries())
          if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
            addresses << entry.ip().toString();
        if (!addresses.isEmpty())
          iface->addItem(
              QString("%1 (%2) — %3%4")
                  .arg(n.humanReadableName(), n.name(), addresses.join(", "),
                       n.flags().testFlag(QNetworkInterface::IsLoopBack)
                           ? " [local]"
                           : ""),
              n.index());
      }
    int i = iface->findData(previous);
    if (i >= 0)
      iface->setCurrentIndex(i);
  });
  bar->addWidget(new QLabel("Interface"));
  bar->addWidget(iface, 1);
  listen = new QPushButton("Démarrer l’écoute");
  listen->setCheckable(true);
  bar->addWidget(rescan);
  bar->addWidget(listen);
  connect(listen, &QPushButton::toggled, rescan, &QPushButton::setDisabled);
  layout->addLayout(bar);
  auto controls = new QHBoxLayout;
  protocol = new QComboBox;
  protocol->addItems({"Art-Net", "sACN"});
  universe = new QSpinBox;
  universe->setRange(0, 32767);
  universe->setValue(0);
  controls->addWidget(new QLabel("Protocole"));
  controls->addWidget(protocol);
  controls->addWidget(new QLabel("Univers"));
  controls->addWidget(universe);
  priority = new QSpinBox;
  priority->setRange(0, 200);
  priority->setValue(100);
  controls->addWidget(new QLabel("Priorité sACN"));
  controls->addWidget(priority);
  destination = new QLineEdit;
  destination->setPlaceholderText(
      "Art-Net : IP cible ou broadcast (ex. 192.168.1.255)");
  controls->addWidget(destination, 1);
  send = new QPushButton("Activer l’émission • 30 Hz");
  send->setCheckable(true);
  connect(send, &QPushButton::toggled, this, [this, rescan](bool) {
    rescan->setEnabled(!send->isChecked() && !listen->isChecked());
  });
  controls->addWidget(send);
  layout->addLayout(controls);
  layout->addWidget(new QLabel("GRILLE : NUMÉRO EN HAUT · VALEUR EN BAS · NOIR "
                               "0 → BLANC 255   •   Cliquez un "
                               "canal pour régler sa sortie ci-dessous"));
  auto split = new QSplitter;
  auto side = new QWidget;
  auto sideLayout = new QVBoxLayout(side);
  sideLayout->setContentsMargins(0, 0, 10, 0);
  sideLayout->addWidget(new QLabel("FLUX DÉTECTÉS"));
  sources = new QListWidget;
  sideLayout->addWidget(sources);
  auto note = new QLabel(
      "Art-Net : univers 0–32767\nsACN : univers 1–63999\n\nChaque source "
      "reste distincte.\nAucune fusion HTP/LTP.\n\nsACN : abonnement multicast "
      "à\nl’univers choisi, plus unicast.\nArt-Net : datagrammes reçus\nsur "
      "l’interface choisie.\n\nModifier un canal prépare\nla sortie. Activez "
      "l’émission\npour envoyer les 512 valeurs.");
  note->setWordWrap(true);
  sideLayout->addWidget(note);
  split->addWidget(side);
  grid = new QTableWidget(16, columns);
  grid->horizontalHeader()->hide();
  grid->verticalHeader()->hide();
  grid->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  grid->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  grid->setItemDelegate(new ChannelDelegate(grid));
  grid->horizontalHeader()->setMinimumSectionSize(28);
  grid->verticalHeader()->setMinimumSectionSize(32);
  grid->setEditTriggers(QAbstractItemView::NoEditTriggers);
  grid->setSelectionMode(QAbstractItemView::SingleSelection);
  for (int i = 0; i < 512; i++) {
    auto item = new QTableWidgetItem;
    item->setTextAlignment(Qt::AlignCenter);
    grid->setItem(i / columns, i % columns, item);
  }
  split->addWidget(grid);
  split->setStretchFactor(1, 1);
  split->setSizes({250, 1000});
  layout->addWidget(split, 1);
  auto editbar = new QHBoxLayout;
  channel = new QSpinBox;
  channel->setRange(1, 512);
  value = new QSpinBox;
  value->setRange(0, 255);
  slider = new QSlider(Qt::Horizontal);
  slider->setRange(0, 255);
  editbar->addWidget(new QLabel("Canal"));
  editbar->addWidget(channel);
  editbar->addWidget(slider, 1);
  editbar->addWidget(value);
  auto copy = new QPushButton("Copier réception → sortie");
  editbar->addWidget(copy);
  auto zero = new QPushButton("Sortie à zéro");
  editbar->addWidget(zero);
  layout->addLayout(editbar);
  metrics = new QLabel;
  diagnostics = new QLabel;
  diagnostics->setObjectName("diagnostics");
  diagnostics->setWordWrap(true);
  layout->addWidget(diagnostics);
  status = new QLabel("Écoute arrêtée • émission arrêtée");
  layout->addWidget(metrics);
  layout->addWidget(status);
  setStyleSheet(
      "QWidget{background:#101822;color:#dbe5ef;font-size:12px} "
      "QComboBox,QSpinBox,QLineEdit,QPushButton{background:#202d3b;border:1px "
      "solid #354759;border-radius:5px;padding:7px} "
      "QPushButton:checked{background:#176c5c} "
      "QTableWidget{gridline-color:#243447;border:1px solid #304357} "
      "QListWidget{background:#162230;border:0} QLabel{color:#aebfd0} "
      "QSlider::groove:horizontal{height:6px;background:#304357} "
      "QSlider::handle:horizontal{width:16px;background:#62dfbc;margin:-5px "
      "0;border-radius:7px}");
  connect(listen, &QPushButton::toggled, this, [this](bool on) {
    if (on)
      start();
    else
      stop();
  });
  connect(&art, &QUdpSocket::readyRead, this, [this] { receive(&art, false); });
  connect(&sacn, &QUdpSocket::readyRead, this,
          [this] { receive(&sacn, true); });
  connect(protocol, &QComboBox::currentIndexChanged, this, [this] {
    endSend();
    universe->setRange(protocol->currentIndex() ? 1 : 0,
                       protocol->currentIndex() ? 63999 : 32767);
    selected.clear();
    if (listen->isChecked())
      join(protocol->currentIndex() ? universe->value() : 0);
    render();
  });
  connect(universe, &QSpinBox::valueChanged, this, [this] {
    endSend();
    selected.clear();
    if (listen->isChecked())
      join(protocol->currentIndex() ? universe->value() : 0);
    render();
  });
  connect(sources, &QListWidget::itemClicked, this, [this](QListWidgetItem *i) {
    endSend();
    QString key = i->data(Qt::UserRole).toString();
    auto f = streams.value(key).frame;
    protocol->setCurrentIndex(f.sacn ? 1 : 0);
    universe->setValue(f.universe);
    selected = key;
    selectStream();
  });
  connect(grid, &QTableWidget::cellClicked, this,
          [this](int r, int c) { channel->setValue(r * columns + c + 1); });
  connect(channel, &QSpinBox::valueChanged, this, [this] {
    updating = true;
    value->setValue(quint8(output[channel->value() - 1]));
    slider->setValue(value->value());
    updating = false;
  });
  connect(value, &QSpinBox::valueChanged, this, [this](int v) {
    if (!updating)
      edit(v);
  });
  connect(slider, &QSlider::valueChanged, value, &QSpinBox::setValue);
  connect(copy, &QPushButton::clicked, this, [this] {
    if (streams.contains(selected))
      output = streams[selected].frame.values;
    render();
  });
  connect(zero, &QPushButton::clicked, this, [this] {
    output.fill(0);
    render();
  });
  connect(send, &QPushButton::toggled, this, [this](bool on) {
    if (!on) {
      if (sendTimer.isActive() && protocol->currentIndex())
        for (int i = 0; i < 3; i++)
          transmit(true);
      sendTimer.stop();
      tx.close();
      protocol->setEnabled(true);
      universe->setEnabled(true);
      iface->setEnabled(!listen->isChecked());
      destination->setEnabled(true);
      priority->setEnabled(true);
      return;
    }
    QHostAddress target(destination->text());
    if (!protocol->currentIndex() &&
        (target.isNull() ||
         target.protocol() != QAbstractSocket::IPv4Protocol)) {
      send->setChecked(false);
      status->setText("Saisissez une adresse IPv4 Art-Net valide.");
      return;
    }
    network =
        QNetworkInterface::interfaceFromIndex(iface->currentData().toInt());
    tx.setMulticastInterface(network);
    protocol->setEnabled(false);
    universe->setEnabled(false);
    iface->setEnabled(false);
    destination->setEnabled(false);
    priority->setEnabled(false);
    sendTimer.start(33);
  });
  connect(&sendTimer, &QTimer::timeout, this, [this] { transmit(); });
  connect(&refresh, &QTimer::timeout, this, &Window::render);
  refresh.start(100);
  render();
}
Window::~Window() {
  endSend();
  stop();
}
void Window::endSend() { send->setChecked(false); }
void Window::start() {
  if (iface->currentIndex() < 0) {
    listen->setChecked(false);
    status->setText("Aucune interface IPv4 active. Cliquez Actualiser.");
    return;
  }
  network = QNetworkInterface::interfaceFromIndex(iface->currentData().toInt());
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
    status->setText("Échec de l’écoute : " + error);
    return;
  }
  datagrams = received = invalid = otherInterface = 0;
  streams.clear();
  selected.clear();
  iface->setEnabled(false);
  status->setText(QString("Écoute Art-Net %1:%2 / sACN UDP %3")
                      .arg(art.localAddress().toString())
                      .arg(artPort)
                      .arg(sacnPort));
  if (protocol->currentIndex())
    join(universe->value());
  listen->setText("Arrêter l’écoute");
}
void Window::stop() {
  art.close();
  sacn.close();
  groups.clear();
  iface->setEnabled(!send->isChecked());
  listen->setText("Démarrer l’écoute");
  if (status)
    status->setText("Écoute arrêtée");
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
    status->setText("Échec abonnement multicast : " + sacn.errorString());
}
void Window::receive(QUdpSocket *socket, bool isSacn) {
  int budget = 256;
  while (socket->hasPendingDatagrams() && budget--) {
    auto d = socket->receiveDatagram();
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
      continue;
    }
    if (isSacn && f.cid == cid)
      continue;
    QString key = QString("%1/%2/%3/%4")
                      .arg(isSacn ? "sACN" : "Art-Net")
                      .arg(f.universe)
                      .arg(d.senderAddress().toString(),
                           QString::fromLatin1(f.cid.toHex()));
    if (f.terminated) {
      streams.remove(key);
      continue;
    }
    if (!streams.contains(key) && streams.size() >= 2048)
      continue;
    auto &s = streams[key];
    if (s.packets && clock.elapsed() - s.seen < 2500 &&
        (isSacn || f.sequence != 0)) {
      int delta = quint8(f.sequence - s.frame.sequence);
      if (delta == 0 || delta > 127)
        continue;
    }
    s.frame = f;
    s.ip = d.senderAddress().toString();
    s.seen = clock.elapsed();
    s.packets++;
    received++;
    if (selected.isEmpty() && f.sacn == bool(protocol->currentIndex()) &&
        f.universe == universe->value())
      selected = key;
  }
}
void Window::selectStream() { render(); }
void Window::edit(int v) {
  output[channel->value() - 1] = char(v);
  slider->setValue(v);
  render();
}
void Window::render() {
  auto now = clock.elapsed();
  for (auto i = streams.begin(); i != streams.end();)
    if (now - i->seen > 60000)
      i = streams.erase(i);
    else
      ++i;
  sources->clear();
  for (auto i = streams.cbegin(); i != streams.cend(); ++i) {
    auto &s = i.value();
    auto item = new QListWidgetItem(
        QString("%1 · U%2   %3\n%4 %5\n%6 paquets · priorité %7")
            .arg(s.frame.sacn ? "sACN" : "Art-Net")
            .arg(s.frame.universe)
            .arg(now - s.seen < 2500 ? "●" : "○")
            .arg(s.ip, s.frame.name)
            .arg(s.packets)
            .arg(s.frame.priority),
        sources);
    item->setData(Qt::UserRole, i.key());
    if (i.key() == selected)
      item->setSelected(true);
  }
  bool live = streams.contains(selected) && now - streams[selected].seen < 2500;
  QByteArray rx = streams.contains(selected) ? streams[selected].frame.values
                                             : QByteArray(512, 0);
  int active = 0;
  for (int i = 0; i < 512; i++) {
    int v = quint8(rx[i]), out = quint8(output[i]);
    if (v)
      active++;
    auto item = grid->item(i / columns, i % columns);
    item->setText(QString("%1\n%2").arg(i + 1).arg(v));
    item->setBackground(QColor(v, v, v));
    item->setForeground(v < 128 ? QColor(Qt::white) : QColor(Qt::black));
    item->setToolTip(QString("Canal %1\nRéception : %2\nSortie : %3")
                         .arg(i + 1)
                         .arg(v)
                         .arg(out));
  }
  QString hint;
  if (listen->isChecked()) {
    if (!datagrams)
      hint = "Aucun datagramme : vérifiez la destination MadMapper et le "
             "partage du port 6454. En local, choisissez 127.0.0.1 et démarrez "
             "l’écoute après MadMapper.";
    else if (!received && otherInterface)
      hint = "Trafic reçu sur une autre interface : choisissez celle utilisée "
             "par MadMapper.";
    else if (!received)
      hint = "Trafic UDP présent, mais aucun paquet DMX pris en charge.";
    else if (!streams.contains(selected))
      hint = "Des flux DMX sont présents : cliquez un flux dans la liste pour "
             "choisir son protocole et son univers.";
  }
  diagnostics->setText(QString("UDP : %1 · DMX : %2 · autre interface : %3 · "
                               "non-DMX / non pris en charge : %4\n%5")
                           .arg(datagrams)
                           .arg(received)
                           .arg(otherInterface)
                           .arg(invalid)
                           .arg(hint));
  updating = true;
  value->setValue(quint8(output[channel->value() - 1]));
  slider->setValue(value->value());
  updating = false;
  metrics->setText(
      QString("%1  •  %2 canaux actifs  •  %3 paquets reçus  •  %4 paquets "
              "ignorés  •  Sortie %5")
          .arg(live ? "SIGNAL ACTIF" : "AUCUN SIGNAL / SIGNAL PÉRIMÉ")
          .arg(active)
          .arg(received)
          .arg(invalid)
          .arg(sendTimer.isActive() ? "EN ÉMISSION" : "ARRÊTÉE"));
}
void Window::transmit(bool terminated) {
  Frame f;
  f.sacn = protocol->currentIndex();
  f.universe = universe->value();
  f.values = output;
  f.cid = cid;
  f.name = "DMXPulse";
  f.priority = priority->value();
  sequence = sequence == 255 ? 1 : sequence + 1;
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
      status->setText(
          "Impossible de lier la sortie à l’interface sélectionnée");
      endSend();
      return;
    }
    tx.setMulticastInterface(network);
    tx.setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
  }
  if (tx.writeDatagram(Protocol::encode(f), target,
                       f.sacn ? sacnPort : artPort) < 0) {
    if (terminated)
      return;
    status->setText("Erreur émission : " + tx.errorString());
    endSend();
  }
}
