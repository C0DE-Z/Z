#include "mediaimporter.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDateTime>
#include <QProcess>
#include <QProgressDialog>
#include <QCoreApplication>
#include <QDebug>

QString MediaImporter::standardizedImportDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/standardized_imports";
    QDir().mkpath(dir);
    return dir;
}

QString MediaImporter::standardizedImportPathForSource(const QString& sourcePath) {
    QFileInfo info(sourcePath);
    const QByteArray fingerprint = (info.absoluteFilePath() + "|" + QString::number(info.size()) + "|" + QString::number(info.lastModified().toMSecsSinceEpoch())).toUtf8();
    const QByteArray hash = QCryptographicHash::hash(fingerprint, QCryptographicHash::Sha1).toHex().left(12);
    const QString safeBase = info.completeBaseName().isEmpty() ? QStringLiteral("clip") : info.completeBaseName();
    return standardizedImportDir() + "/" + safeBase + "_" + QString::fromLatin1(hash) + ".mov";
}

QString MediaImporter::datamoshProxyPathForSource(const QString& sourcePath) {
    QFileInfo info(sourcePath);
    const QByteArray fingerprint = (info.absoluteFilePath() + "|" + QString::number(info.size()) + "|" + QString::number(info.lastModified().toMSecsSinceEpoch())).toUtf8();
    const QByteArray hash = QCryptographicHash::hash(fingerprint, QCryptographicHash::Sha1).toHex().left(12);
    const QString safeBase = info.completeBaseName().isEmpty() ? QStringLiteral("clip") : info.completeBaseName();
    return standardizedImportDir() + "/" + safeBase + "_" + QString::fromLatin1(hash) + "_datamosh.mp4";
}

QString MediaImporter::transcodeToStandardMov(const QString& sourcePath) {
    const QString suffix = QFileInfo(sourcePath).suffix().toLower();
    if (suffix == "mov") {
        return sourcePath;
    }

    const QString outputPath = standardizedImportPathForSource(sourcePath);
    if (QFileInfo::exists(outputPath) && QFileInfo(outputPath).size() > 0) {
        return outputPath;
    }

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef _WIN32
    env.insert("PATH", "C:\\msys64\\mingw64\\bin;C:\\msys64\\usr\\bin;" + env.value("PATH"));
    QString ffmpegPath = "ffmpeg.exe";
#else
    env.insert("PATH", "/usr/local/bin:/opt/homebrew/bin:/usr/bin:" + env.value("PATH"));
    QString ffmpegPath = "ffmpeg";
#endif
    proc.setProcessEnvironment(env);
    // Long transcodes can emit a large amount of progress text. Do not retain
    // it in QProcess buffers while the conversion is running.
    proc.setProcessChannelMode(QProcess::ForwardedErrorChannel);

    QStringList args;
    args << "-y"
         << "-hide_banner" << "-loglevel" << "error"
         << "-i" << sourcePath
         << "-map" << "0:v:0"
         << "-map" << "0:a?"
         << "-c:v" << "prores_ks"
         << "-profile:v" << "hq"
         << "-pix_fmt" << "yuva422p"
         << "-c:a" << "aac"
         << "-movflags" << "+faststart"
         << outputPath;

    proc.start(ffmpegPath, args);
    if (!proc.waitForStarted()) {
        qWarning() << "Import: Failed to start FFmpeg for MOV conversion.";
        return QString();
    }

    const auto startTime = QDateTime::currentMSecsSinceEpoch();
    while (proc.state() != QProcess::NotRunning) {
        if (!proc.waitForFinished(100)) {
            const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startTime;
            if (elapsed > 120000) {
                proc.kill();
                proc.waitForFinished();
                QFile::remove(outputPath);
                qWarning() << "Import: Media conversion timed out after 120s.";
                return QString();
            }
            continue;
        }
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0 || !QFileInfo::exists(outputPath) || QFileInfo(outputPath).size() == 0) {
        qWarning() << "Import: FFmpeg MOV conversion failed:" << proc.readAllStandardError();
        QFile::remove(outputPath);
        return QString();
    }

    return outputPath;
}

QString MediaImporter::transcodeToDatamoshProxy(const QString& sourcePath) {
    const QString outputPath = datamoshProxyPathForSource(sourcePath);
    if (QFileInfo::exists(outputPath) && QFileInfo(outputPath).size() > 0) {
        return outputPath;
    }

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef _WIN32
    env.insert("PATH", "C:\\msys64\\mingw64\\bin;C:\\msys64\\usr\\bin;" + env.value("PATH"));
    QString ffmpegPath = "ffmpeg.exe";
#else
    env.insert("PATH", "/usr/local/bin:/opt/homebrew/bin:/usr/bin:" + env.value("PATH"));
    QString ffmpegPath = "ffmpeg";
#endif
    proc.setProcessEnvironment(env);
    proc.setProcessChannelMode(QProcess::ForwardedErrorChannel);

    // Supermosh's core requirement is an H.264 stream made exclusively of an
    // initial/key GOP boundary plus P-frame deltas. Disabling B-frames keeps
    // packet order equal to presentation order; fixed closed GOPs give the
    // decoder repeatable I-frame removal points without touching the original.
    QStringList args;
    args << "-y"
         << "-hide_banner" << "-loglevel" << "error"
         << "-i" << sourcePath
         << "-map" << "0:v:0"
         << "-an"
         << "-c:v" << "libx264"
         << "-preset" << "veryfast"
         << "-crf" << "15"
         << "-g" << "30"
         << "-keyint_min" << "30"
         << "-sc_threshold" << "0"
         << "-bf" << "0"
         << "-flags:v" << "+cgop"
         << "-pix_fmt" << "yuv420p"
         << "-movflags" << "+faststart"
         << outputPath;

    proc.start(ffmpegPath, args);
    if (!proc.waitForStarted()) {
        qWarning() << "Datamosh: Failed to start FFmpeg proxy encoder.";
        return QString();
    }

    const auto startTime = QDateTime::currentMSecsSinceEpoch();
    while (proc.state() != QProcess::NotRunning) {
        if (!proc.waitForFinished(100)) {
            const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startTime;
            if (elapsed > 120000) {
                proc.kill();
                proc.waitForFinished();
                QFile::remove(outputPath);
                qWarning() << "Datamosh: Proxy conversion timed out after 120s.";
                return QString();
            }
            continue;
        }
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0 ||
        !QFileInfo::exists(outputPath) || QFileInfo(outputPath).size() == 0) {
        qWarning() << "Datamosh: FFmpeg proxy conversion failed:" << proc.readAllStandardError();
        QFile::remove(outputPath);
        return QString();
    }

    return outputPath;
}
