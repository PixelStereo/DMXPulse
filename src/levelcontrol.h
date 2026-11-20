#pragma once
#include <QWidget>
// Two level bars one above the other: what the network carries (read only) and
// what this application outputs (drag, scroll or type to change it).
class LevelControl final : public QWidget {
  Q_OBJECT
  Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
public:
  explicit LevelControl(QWidget *parent = nullptr);
  int value() const { return level; }
  int network() const { return net; }
  int others() const { return external; }
  QSize sizeHint() const override { return {280, 64}; }
public slots:
  void setValue(int next);
  // The level on the network, or -1 without data; `others` is the level sent
  // by other devices alone (-1 when it is not worth showing apart).
  void setNetwork(int level, int others = -1);
signals:
  void valueChanged(int value);
  void editRequested();

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void mouseDoubleClickEvent(QMouseEvent *) override;
  void wheelEvent(QWheelEvent *) override;
  void keyPressEvent(QKeyEvent *) override;

private:
  QRectF track(int row) const;
  int levelAt(qreal x) const;
  int level = 0, net = -1, external = -1;
  qreal dragX = 0, dragLevel = 0, wheelRemainder = 0;
  bool dragging = false;
};
