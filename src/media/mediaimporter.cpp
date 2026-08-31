#include "mediaimporter.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDateTime>
#include <QProcess>
#include <QCoreApplication>
#include <QDebug>

extern "C" {
#include <libavformat/avformat.h>
}

namespace {
QProcessEnvironment ffmpegEnvironment() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef _WIN32
    env.insert("PATH", "C:\\msys64\\mingw64\\bin;C:\\msys64\\usr\\bin;" + env.value("PATH"));
#else
    env.insert("PATH", "/usr/local/bin:/opt/homebrew/bin:/usr/bin:" + env.value("PATH"));
#endif
    return env;
}

QString ffmpegExecutable() {
    const QString bundledFfmpeg = QDir(QCoreApplication::applicationDirPath()).filePath("ffmpeg.exe");
    if (QFileInfo(bundledFfmpeg).isFile()) {
        return bundledFfmpeg;
    }
#ifdef _WIN32
    return QStringLiteral("ffmpeg.exe");
#else
    return QStringLiteral("ffmpeg");
#endif
}

bool probeVideoFile(const QString& path) {
    const QFileInfo info(path);
    if (!info.exists() || info.size() <= 0) {
        return false;
    }

    // Do not depend on the external ffprobe executable. The deployed editor
    // already links libavformat, while ffprobe is not normally installed on a
    // user's system. This catches incomplete MOV/MP4 outputs (for example,
    // those missing a final moov atom) without rejecting valid MP4 sources.
    AVFormatContext* formatContext = nullptr;
    const QByteArray encodedPath = path.toUtf8();
    if (avformat_open_input(&formatContext, encodedPath.constData(), nullptr, nullptr) < 0) {
        qWarning() << "Import: Discarding invalid media:" << path;
        return false;
    }

    const int streamInfoResult = avformat_find_stream_info(formatContext, nullptr);
    bool hasVideoStream = false;
    if (streamInfoResult >= 0) {
        for (unsigned int index = 0; index < formatContext->nb_streams; ++index) {
            if (formatContext->streams[index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                hasVideoStream = true;
                break;
            }
        }
    }
    avformat_close_input(&formatContext);
    if (!hasVideoStream) {
        qWarning() << "Import: Discarding media without a readable video stream:" << path;
    }
    return hasVideoStream;
}

bool promoteCompletedCacheFile(const QString& temporaryPath, const QString& outputPath, const QString& operation) {
    if (!probeVideoFile(temporaryPath)) {
        QFile::remove(temporaryPath);
        return false;
    }

    QFile::remove(outputPath);
    if (!QFile::rename(temporaryPath, outputPath)) {
        qWarning() << operation << "could not finalize cached media:" << outputPath;
        QFile::remove(temporaryPath);
        return false;
    }
    return true;
}

QString temporaryCachePath(const QString& outputPath) {
    const QFileInfo info(outputPath);
    return info.dir().filePath(info.completeBaseName() + ".partial." + info.suffix());
}
}

QString MediaImporter::standardizedImportDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/standardized_imports";
    QDir().mkpath(dir);
    return dir;
}

bool MediaImporter::isUsableVideoFile(const QString& path) {
    return probeVideoFile(path);
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
        if (probeVideoFile(sourcePath)) {
            return sourcePath;
        }
        qWarning() << "Import: Source MOV is invalid or incomplete:" << sourcePath;
        return QString();
    }

    const QString outputPath = standardizedImportPathForSource(sourcePath);
    if (probeVideoFile(outputPath)) {
        return outputPath;
    }
    QFile::remove(outputPath);
    const QString temporaryPath = temporaryCachePath(outputPath);
    QFile::remove(temporaryPath);

    QProcess proc;
    proc.setProcessEnvironment(ffmpegEnvironment());
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
            << temporaryPath;

        proc.start(ffmpegExecutable(), args);
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
                QFile::remove(temporaryPath);
                qWarning() << "Import: Media conversion timed out after 120s.";
                return QString();
            }
            continue;
        }
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0 ||
        !promoteCompletedCacheFile(temporaryPath, outputPath, "Import:")) {
        qWarning() << "Import: FFmpeg MOV conversion failed:" << proc.readAllStandardError();
        QFile::remove(temporaryPath);
        return QString();
    }

    return outputPath;
}

QString MediaImporter::transcodeToDatamoshProxy(const QString& sourcePath) {
    const QString outputPath = datamoshProxyPathForSource(sourcePath);
    if (probeVideoFile(outputPath)) {
        return outputPath;
    }
    QFile::remove(outputPath);
    const QString temporaryPath = temporaryCachePath(outputPath);
    QFile::remove(temporaryPath);

    QProcess proc;
    proc.setProcessEnvironment(ffmpegEnvironment());
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
            << temporaryPath;

        proc.start(ffmpegExecutable(), args);
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
                QFile::remove(temporaryPath);
                qWarning() << "Datamosh: Proxy conversion timed out after 120s.";
                return QString();
            }
            continue;
        }
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0 ||
        !promoteCompletedCacheFile(temporaryPath, outputPath, "Datamosh:")) {
        qWarning() << "Datamosh: FFmpeg proxy conversion failed:" << proc.readAllStandardError();
        QFile::remove(temporaryPath);
        return QString();
    }

    return outputPath;
}
