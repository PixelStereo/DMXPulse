#include "levelcontrol.h"
#include <QtWidgets>
namespace {
constexpr int labelWidth = 74, valueWidth = 34, rowHeight = 16, rowGap = 8;
const QColor networkColor("#9aa7a1"), outputColor("#d9b36c"),
    trackColor("#2a2e2c");
} // namespace
LevelControl::LevelControl(QWidget *parent) : QWidget(parent) {
  setObjectName("outputEncoder");
  setAccessibleName("Output level");
  setAccessibleDescription(
      "Click or drag to set the output level. Hold Shift for fine adjustment. "
      "Arrow keys change by one; Page Up or Down by ten; Home sets zero; End "
      "sets full.");
  setFocusPolicy(Qt::StrongFocus);
  setFixedHeight(2 * rowHeight + rowGap + 8);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setCursor(Qt::PointingHandCursor);
  setToolTip("Click or drag the output bar · Scroll to adjust · Shift for fine "
             "control · Double-click to type");
}
void LevelControl::setValue(int next) {
  next = qBound(0, next, 255);
  if (next == level)
    return;
  level = next;
  update();
  emit valueChanged(level);
}
void LevelControl::setNetwork(int next, int others) {
  next = next < 0 ? -1 : qMin(next, 255);
  others = others < 0 ? -1 : qMin(others, 255);
  if (next == net && others == external)
    return;
  net = next;
  external = others;
  update();
}
QRectF LevelControl::track(int row) const {
  const qreal top = 4 + row * (rowHeight + rowGap);
  return {qreal(labelWidth), top,
          qreal(width() - labelWidth - valueWidth - 8), qreal(rowHeight)};
}
int LevelControl::levelAt(qreal x) const {
  const QRectF bar = track(1);
  return qBound(0, qRound((x - bar.left()) / bar.width() * 255.0), 255);
}
void LevelControl::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  QFont font = this->font();
  font.setPixelSize(10);
  font.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
  const QColor muted("#8d9892");
  for (int row = 0; row < 2; ++row) {
    const bool output = row == 1;
    const int shown = output ? level : net;
    const QRectF bar = track(row);
    painter.setFont(font);
    painter.setPen(muted);
    painter.drawText(QRectF(0, bar.top(), labelWidth - 6, bar.height()),
                     Qt::AlignVCenter | Qt::AlignLeft,
                     output ? "OUTPUT" : "NETWORK");
    painter.setPen(Qt::NoPen);
    painter.setBrush(trackColor);
    painter.drawRoundedRect(bar, 4, 4);
    const QColor color = output ? outputColor : networkColor;
    if (shown > 0) {
      painter.setBrush(color);
      painter.drawRoundedRect(
          QRectF(bar.left(), bar.top(), qMax(8.0, bar.width() * shown / 255.0),
                 bar.height()),
          4, 4);
    }
    if (!output && external >= 0 && external != net) {
      // Other devices alone: where the network would stand without this output
      const qreal x = bar.left() + bar.width() * external / 255.0;
      painter.setPen(QPen(QColor("#f0f4f1"), 2));
      painter.drawLine(QPointF(x, bar.top() - 2), QPointF(x, bar.bottom() + 2));
    }
    QFont valueFont = this->font();
    valueFont.setPixelSize(13);
    valueFont.setWeight(QFont::DemiBold);
    painter.setFont(valueFont);
    painter.setPen(shown < 0 ? muted : color);
    painter.drawText(QRectF(bar.right() + 6, bar.top(), valueWidth, bar.height()),
                     Qt::AlignVCenter | Qt::AlignRight,
                     shown < 0 ? QString("—") : QString::number(shown));
  }
  if (hasFocus()) {
    painter.setPen(QPen(QColor("#7b9689"), 1, Qt::DotLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(track(1).adjusted(-3, -3, 3, 3), 6, 6);
  }
}
void LevelControl::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    dragging = true;
    dragX = event->position().x();
    dragLevel = level;
    setFocus(Qt::MouseFocusReason);
    // A plain click puts the output where it lands; Shift moves it relatively.
    if (!event->modifiers().testFlag(Qt::ShiftModifier))
      setValue(levelAt(dragX));
    dragLevel = level;
    event->accept();
  } else
    QWidget::mousePressEvent(event);
}
void LevelControl::mouseMoveEvent(QMouseEvent *event) {
  if (dragging) {
    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
      const qreal perPixel = 255.0 / track(1).width();
      dragLevel = qBound(
          0.0, dragLevel + (event->position().x() - dragX) * perPixel * 0.2,
          255.0);
      dragX = event->position().x();
      setValue(qRound(dragLevel));
    } else {
      dragX = event->position().x();
      setValue(levelAt(dragX));
      dragLevel = level;
    }
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
