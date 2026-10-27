#include "protocol.h"
namespace {
int u8(const QByteArray &d, int i) { return quint8(d[i]); }
int be(const QByteArray &d, int i) { return (u8(d, i) << 8) | u8(d, i + 1); }
void put(QByteArray &d, int i, int v) {
  d[i] = char(v >> 8);
  d[i + 1] = char(v);
}
bool pdu(const QByteArray &d, int i) {
  return (u8(d, i) & 0xf0) == 0x70 && (be(d, i) & 0xfff) == d.size() - i;
}
} // namespace
QByteArray Protocol::encode(const Frame &f) {
  QByteArray values = f.values.left(512);
  values.resize(512, 0);
  if (!f.sacn) {
    QByteArray d(530, 0);
    d.replace(0, 8, QByteArray("Art-Net\0", 8));
    d[9] = 0x50;
    put(d, 10, 14);
    d[12] = char(f.sequence);
    d[14] = char(f.universe);
    d[15] = char(f.universe >> 8);
    put(d, 16, 512);
    d.replace(18, 512, values);
    return d;
  }
  QByteArray d(638, 0);
  put(d, 0, 16);
  d.replace(4, 12, QByteArray("ASC-E1.17\0\0\0", 12));
  put(d, 16, 0x7000 | 622);
  d[21] = 4;
  QByteArray cid = f.cid.left(16);
  cid.resize(16, 0);
  d.replace(22, 16, cid);
  put(d, 38, 0x7000 | 600);
  d[43] = 2;
  auto name = f.name.toUtf8().left(63);
  d.replace(44, name.size(), name);
  d[108] = char(f.priority);
  d[111] = char(f.sequence);
  d[112] = f.terminated ? 0x40 : 0;
  put(d, 113, f.universe);
  put(d, 115, 0x7000 | 523);
  d[117] = 2;
  d[118] = char(0xa1);
  put(d, 121, 1);
  put(d, 123, 513);
  d.replace(126, 512, values);
  return d;
}
bool Protocol::decode(const QByteArray &d, bool sacn, Frame &f) {
  f = Frame{};
  f.sacn = sacn;
  if (!sacn) {
    if (d.size() < 18 || d.left(8) != QByteArray("Art-Net\0", 8) ||
        u8(d, 8) != 0 || u8(d, 9) != 0x50 || be(d, 10) < 14)
      return false;
    int n = be(d, 16);
    if (n < 2 || n > 512 || n % 2 || d.size() < 18 + n || u8(d, 15) > 127)
      return false;
    f.universe = u8(d, 14) | (u8(d, 15) << 8);
    f.sequence = u8(d, 12);
    f.values = d.mid(18, n);
  } else {
    if (d.size() < 126 || be(d, 0) != 16 || be(d, 2) != 0 ||
        d.mid(4, 12) != QByteArray("ASC-E1.17\0\0\0", 12) || !pdu(d, 16) ||
        d.mid(18, 4) != QByteArray::fromHex("00000004") || !pdu(d, 38) ||
        d.mid(40, 4) != QByteArray::fromHex("00000002") || !pdu(d, 115) ||
        u8(d, 117) != 2 || u8(d, 118) != 0xa1 || be(d, 119) != 0 ||
        be(d, 121) != 1 || u8(d, 125) != 0)
      return false;
    int n = be(d, 123);
    if (n < 1 || n > 513 || d.size() != 125 + n || be(d, 113) < 1 ||
        be(d, 113) > 63999 || u8(d, 108) > 200 || be(d, 109) != 0 ||
        (u8(d, 112) & 0x80))
      return false;
    f.universe = be(d, 113);
    f.priority = u8(d, 108);
    f.sequence = u8(d, 111);
    f.terminated = u8(d, 112) & 0x40;
    f.cid = d.mid(22, 16);
    f.name = QString::fromUtf8(d.mid(44, 64).split('\0').first());
    f.values = d.mid(126, n - 1);
  }
  f.values.resize(512, 0);
  return true;
}
