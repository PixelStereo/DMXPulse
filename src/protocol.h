#pragma once
#include <QtCore>
struct Frame {
  bool sacn = false;
  int universe = 0;
  int priority = 100;
  quint8 sequence = 0;
  QByteArray cid, values;
  QString name;
  bool terminated = false;
};
namespace Protocol {
QByteArray encode(const Frame &f);
bool decode(const QByteArray &data, bool sacn, Frame &frame);
} // namespace Protocol
