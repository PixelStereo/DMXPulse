#pragma once
#include <QWidget>
class LevelControl final : public QWidget {
  Q_OBJECT
  Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
public:
  explicit LevelControl(QWidget *parent = nullptr);
  int value() const { return level; }
  QSize sizeHint() const override { return {220, 200}; }
public slots:
  void setValue(int next);
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
  int level = 0;
  qreal dragY = 0, dragLevel = 0, wheelRemainder = 0;
  bool dragging = false;
};
