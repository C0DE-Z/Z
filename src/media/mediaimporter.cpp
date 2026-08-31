#include "mediaimporter.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDateTime>
#include <QProcess>
#include <QCoreApplication>
#include <QDebug>
#include <algorithm>

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

double videoDurationSeconds(const QString& path) {
    AVFormatContext* formatContext = nullptr;
    const QByteArray encodedPath = path.toUtf8();
    if (avformat_open_input(&formatContext, encodedPath.constData(), nullptr, nullptr) < 0) {
        return 0.0;
    }

    double duration = 0.0;
    if (avformat_find_stream_info(formatContext, nullptr) >= 0 &&
        formatContext->duration != AV_NOPTS_VALUE) {
        duration = static_cast<double>(formatContext->duration) / AV_TIME_BASE;
    }
    avformat_close_input(&formatContext);
    return std::max(0.0, duration);
}

bool waitForTranscode(QProcess& process, double inputDurationSeconds,
                      const MediaImporter::ProgressCallback& progressCallback,
                      const QString& operation) {
    if (!process.waitForStarted()) {
        qWarning() << operation << "Failed to start FFmpeg.";
        return false;
    }

    QByteArray progressBuffer;
    double lastReportedProgress = -1.0;
    auto consumeProgress = [&] {
        progressBuffer.append(process.readAllStandardOutput());
        qsizetype newlineIndex = 0;
        while ((newlineIndex = progressBuffer.indexOf('\n')) >= 0) {
            const QByteArray line = progressBuffer.left(newlineIndex).trimmed();
            progressBuffer.remove(0, newlineIndex + 1);
            if (!progressCallback) continue;

            if (line.startsWith("out_time_us=") && inputDurationSeconds > 0.0) {
                bool parsed = false;
                const qint64 outputMicroseconds = line.mid(12).toLongLong(&parsed);
                if (!parsed) continue;
                const double progress = std::clamp(
                    static_cast<double>(outputMicroseconds) / (inputDurationSeconds * AV_TIME_BASE),
                    0.0, 1.0);
                if (progress >= 1.0 || progress - lastReportedProgress >= 0.002) {
                    lastReportedProgress = progress;
                    progressCallback(progress);
                }
            } else if (line == "progress=end" && lastReportedProgress < 1.0) {
                lastReportedProgress = 1.0;
                progressCallback(1.0);
            }
        }
    };

    const qint64 startTime = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 maximumConversionMilliseconds = 30LL * 60LL * 1000LL;
    while (process.state() != QProcess::NotRunning) {
        consumeProgress();
        if (!process.waitForFinished(100)) {
            if (QDateTime::currentMSecsSinceEpoch() - startTime > maximumConversionMilliseconds) {
                process.kill();
                process.waitForFinished();
                qWarning() << operation << "Media conversion timed out after 30 minutes.";
                return false;
            }
            continue;
        }
    }
    consumeProgress();
    return true;
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

QString MediaImporter::transcodeToStandardMov(const QString& sourcePath, ProgressCallback progressCallback) {
    Q_UNUSED(progressCallback);
    // Re-encoding all footage to 4:2:2 alpha-capable ProRes made opaque MP4
    // imports exceptionally slow and created huge, unnecessary cache files.
    // libavformat/libavcodec already decode Z's supported formats directly;
    // this also keeps the source's native alpha only when it actually has it.
    if (probeVideoFile(sourcePath)) {
        return sourcePath;
    }
    qWarning() << "Import: Source media is invalid or incomplete:" << sourcePath;
    return QString();
}

QString MediaImporter::transcodeToDatamoshProxy(const QString& sourcePath, ProgressCallback progressCallback) {
    const QString outputPath = datamoshProxyPathForSource(sourcePath);
    if (probeVideoFile(outputPath)) {
        return outputPath;
    }
    QFile::remove(outputPath);
    const QString temporaryPath = temporaryCachePath(outputPath);
    QFile::remove(temporaryPath);
    const double inputDurationSeconds = videoDurationSeconds(sourcePath);

    QProcess proc;
    proc.setProcessEnvironment(ffmpegEnvironment());
    proc.setProcessChannelMode(QProcess::SeparateChannels);

    // Supermosh's core requirement is an H.264 stream made exclusively of an
    // initial/key GOP boundary plus P-frame deltas. Disabling B-frames keeps
    // packet order equal to presentation order; fixed closed GOPs give the
    // decoder repeatable I-frame removal points without touching the original.
    QStringList args;
    args << "-y"
         << "-hide_banner" << "-loglevel" << "error"
            << "-progress" << "pipe:1" << "-nostats"
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
    if (!waitForTranscode(proc, inputDurationSeconds, progressCallback, "Datamosh:")) {
        QFile::remove(temporaryPath);
        return QString();
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0 ||
        !promoteCompletedCacheFile(temporaryPath, outputPath, "Datamosh:")) {
        qWarning() << "Datamosh: FFmpeg proxy conversion failed:" << proc.readAllStandardError();
        QFile::remove(temporaryPath);
        return QString();
    }

    return outputPath;
}
