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
    return standardizedImportDir() + "/" + safeBase + "_" + QString::fromLatin1(hash) + ".mp4";
}

QString MediaImporter::transcodeToStandardMp4(const QString& sourcePath) {
    const QString suffix = QFileInfo(sourcePath).suffix().toLower();
    if (suffix == "mp4" || suffix == "m4v") {
        return sourcePath;
    }

    const QString outputPath = standardizedImportPathForSource(sourcePath);
    if (QFileInfo::exists(outputPath) && QFileInfo(outputPath).size() > 0) {
        return outputPath;
    }

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", "C:\\msys64\\mingw64\\bin;C:\\msys64\\usr\\bin;" + env.value("PATH"));
    proc.setProcessEnvironment(env);

    QStringList args;
    args << "-y"
         << "-i" << sourcePath
         << "-map" << "0:v:0"
         << "-map" << "0:a?"
         << "-c:v" << "libx264"
         << "-preset" << "veryfast"
         << "-crf" << "20"
         << "-pix_fmt" << "yuv420p"
         << "-c:a" << "aac"
         << "-movflags" << "+faststart"
         << outputPath;

    proc.start("ffmpeg.exe", args);
    if (!proc.waitForStarted()) {
        qWarning() << "Import: Failed to start FFmpeg for media conversion.";
        return QString();
    }

    QProgressDialog progress("Converting media to MP4...", "Cancel", 0, 0);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();

    while (proc.state() != QProcess::NotRunning) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (progress.wasCanceled()) {
            proc.kill();
            proc.waitForFinished();
            QFile::remove(outputPath);
            qWarning() << "Import: Media conversion canceled.";
            return QString();
        }
        proc.waitForFinished(50);
    }

    progress.close();

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0 || !QFileInfo::exists(outputPath) || QFileInfo(outputPath).size() == 0) {
        qWarning() << "Import: FFmpeg conversion failed:" << proc.readAllStandardError();
        QFile::remove(outputPath);
        return QString();
    }

    return outputPath;
}
