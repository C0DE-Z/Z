#ifndef LOGGING_H
#define LOGGING_H

#include <atomic>
#include <cstdio>
#include <QString>
#include <QtGlobal>

namespace AppLogging {
inline std::atomic_bool& enabledFlag() {
    static std::atomic_bool flag{true};
    return flag;
}

inline bool enabled() {
    return enabledFlag().load(std::memory_order_relaxed);
}

inline void setEnabled(bool value) {
    enabledFlag().store(value, std::memory_order_relaxed);
}

inline void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& message) {
    if (!enabled() && (type == QtDebugMsg || type == QtInfoMsg)) {
        return;
    }

    const QByteArray local = message.toLocal8Bit();
    std::fprintf(stderr, "%s\n", local.constData());
    std::fflush(stderr);
}

inline void install() {
    qInstallMessageHandler(messageHandler);
}
} 

#endif 
