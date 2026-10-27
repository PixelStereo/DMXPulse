#include "levelcontrol.h"
#include <QtWidgets>
LevelControl::LevelControl(QWidget *parent) : QWidget(parent) {
  setObjectName("outputEncoder");
  setAccessibleName("Output level encoder");
  setAccessibleDescription(
      "Drag vertically to adjust. Hold Shift for fine adjustment. Arrow keys "
      "change by one; Page Up or Down by ten; Home sets zero; End sets full.");
  setFocusPolicy(Qt::StrongFocus);
  setMinimumHeight(196);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setCursor(Qt::SizeVerCursor);
  setToolTip("Drag vertically · Scroll to adjust · Shift for fine control · "
             "Double-click to type");
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 120, 0, 0);
  layout->setSpacing(6);
  auto steps = new QHBoxLayout;
  steps->setSpacing(4);
  for (int step : {-10, -1, 1, 10}) {
    auto button = new QPushButton(QString(step > 0 ? "+%1" : "%1").arg(step));
    button->setObjectName(QString("levelStep%1").arg(step));
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRepeat(true);
    button->setAutoRepeatDelay(350);
    button->setAutoRepeatInterval(80);
    button->setStyleSheet("padding:5px 0;min-width:0;min-height:18px");
    connect(button, &QPushButton::clicked, this,
            [this, step] { setValue(level + step); });
    steps->addWidget(button);
  }
  layout->addLayout(steps);
  auto presets = new QHBoxLayout;
  presets->setSpacing(4);
  const QList<QPair<QString, int>> values = {
      {"0", 0}, {"25%", 64}, {"50%", 128}, {"75%", 191}, {"Full", 255}};
  for (const auto &preset : values) {
    auto button = new QPushButton(preset.first);
    button->setObjectName(QString("levelPreset%1").arg(preset.second));
    button->setCursor(Qt::PointingHandCursor);
    button->setToolTip(QString("Set output to %1").arg(preset.second));
    button->setStyleSheet(
        "padding:5px 0;min-width:0;min-height:18px;font-size:11px");
    connect(button, &QPushButton::clicked, this,
            [this, preset] { setValue(preset.second); });
    presets->addWidget(button);
  }
  layout->addLayout(presets);
}
void LevelControl::setValue(int next) {
  next = qBound(0, next, 255);
  if (next == level)
    return;
  level = next;
  update();
  emit valueChanged(level);
}
void LevelControl::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF ring(width() / 2.0 - 48, 3, 96, 96);
  QPen pen(QColor("#383d3a"), 5, Qt::SolidLine, Qt::RoundCap);
  painter.setPen(pen);
  painter.drawArc(ring, 225 * 16, -270 * 16);
  pen.setColor(QColor("#b7cfc1"));
  painter.setPen(pen);
  painter.drawArc(ring, 225 * 16, qRound(-270 * 16 * level / 255.0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor("#232725"));
  painter.drawEllipse(ring.adjusted(8, 8, -8, -8));
  QFont font = this->font();
  font.setPixelSize(30);
  font.setWeight(QFont::DemiBold);
  painter.setFont(font);
  painter.setPen(QColor("#f0f4f1"));
  painter.drawText(QRectF(0, 23, width(), 38), Qt::AlignCenter,
                   QString::number(level));
  font.setPixelSize(9);
  font.setWeight(QFont::Normal);
  font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
  painter.setFont(font);
  painter.setPen(QColor("#a3b0a8"));
  painter.drawText(QRectF(0, 60, width(), 15), Qt::AlignCenter, "OUTPUT");
  font.setPixelSize(10);
  font.setLetterSpacing(QFont::AbsoluteSpacing, 0);
  painter.setFont(font);
  painter.setPen(QColor("#909995"));
  painter.drawText(QRectF(0, 102, width(), 15), Qt::AlignCenter,
                   "Drag · Scroll · Shift for fine");
  if (hasFocus()) {
    painter.setPen(QPen(QColor("#7b9689"), 1, Qt::DotLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(ring.adjusted(-7, -2, 7, 7), 8, 8);
  }
}
void LevelControl::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && event->position().y() < 102) {
    dragging = true;
    dragY = event->position().y();
    dragLevel = level;
    setFocus(Qt::MouseFocusReason);
    event->accept();
  } else
    QWidget::mousePressEvent(event);
}
void LevelControl::mouseMoveEvent(QMouseEvent *event) {
  if (dragging) {
    qreal sensitivity =
        event->modifiers().testFlag(Qt::ShiftModifier) ? 0.2 : 1.0;
    dragLevel = qBound(
        0.0, dragLevel + (dragY - event->position().y()) * sensitivity, 255.0);
    dragY = event->position().y();
    setValue(qRound(dragLevel));
    event->accept();
  } else
    QWidget::mouseMoveEvent(event);
}
void LevelControl::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton && dragging) {
    dragging = false;
    event->accept();
  } else
    QWidget::mouseReleaseEvent(event);
}
void LevelControl::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    dragging = false;
    emit editRequested();
    event->accept();
  } else
    QWidget::mouseDoubleClickEvent(event);
}
void LevelControl::wheelEvent(QWheelEvent *event) {
  wheelRemainder += event->angleDelta().y() / 120.0;
  int steps = int(wheelRemainder);
  wheelRemainder -= steps;
  int increment = event->modifiers().testFlag(Qt::ShiftModifier) ? 1 : 4;
  setValue(level + steps * increment);
  event->accept();
}
void LevelControl::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Up:
  case Qt::Key_Right:
    setValue(level + 1);
    break;
  case Qt::Key_Down:
  case Qt::Key_Left:
    setValue(level - 1);
    break;
  case Qt::Key_PageUp:
    setValue(level + 10);
    break;
  case Qt::Key_PageDown:
    setValue(level - 10);
    break;
  case Qt::Key_Home:
    setValue(0);
    break;
  case Qt::Key_End:
    setValue(255);
    break;
  default:
    QWidget::keyPressEvent(event);
    return;
  }
  event->accept();
}
