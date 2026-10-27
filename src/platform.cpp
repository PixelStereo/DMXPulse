#include "platform.h"
#include <QCoreApplication>
#ifdef Q_OS_ANDROID
#include <QJniEnvironment>
#include <QJniObject>
#endif
bool setMulticastReceptionEnabled(bool enabled) {
#ifdef Q_OS_ANDROID
  static QJniObject lock;
  static bool held = false;
  if (enabled && !lock.isValid()) {
    auto context = QNativeInterface::QAndroidApplication::context();
    auto service = QJniObject::fromString("wifi");
    auto wifi = context.callObjectMethod(
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
        service.object<jstring>());
    auto name = QJniObject::fromString("DMXPulse");
    lock = wifi.callObjectMethod(
        "createMulticastLock",
        "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;",
        name.object<jstring>());
    if (!lock.isValid())
      return false;
    lock.callMethod<void>("setReferenceCounted", "(Z)V", false);
  }
  if (enabled != held && lock.isValid()) {
    lock.callMethod<void>(enabled ? "acquire" : "release", "()V");
    QJniEnvironment env;
    if (env.checkAndClearExceptions())
      return false;
    held = enabled;
  }
#else
  Q_UNUSED(enabled);
#endif
  return true;
}
