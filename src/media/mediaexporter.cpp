#include "mediaexporter.h"
#include "engine/videoengine.h"
#include "core/project.h"
#include "utils/logging.h"
#include "engine/glwidget.h"
#include <QFileDialog>
#include <QProgressDialog>
#include <QProcess>
#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QDebug>

void MediaExporter::exportVideo(
    QWidget* parentWindow,
    const QString& activeClipId,
    const QString& activeFilePath,
    GLWidget* glWidget,
    std::function<void(double)> scrubCallback,
    std::function<void()> togglePlaybackCallback,
    bool wasPlaying
) {
    if (activeClipId.isEmpty()) {
        qWarning() << "Export: No active clip loaded to export.";
        return;
    }

    double fps = 30.0;
    if (!activeClipId.isEmpty()) {
        fps = VideoEngine::instance().getFps(activeClipId.toStdString());
    }
    if (fps <= 0.0) fps = 30.0;
    double duration = 0.0;
    for (const auto& t : Project::instance().getTracks()) {
        for (const auto& c : t.clips) {
            duration = std::max(duration, c.timelineStart + c.sourceDuration);
        }
    }
    if (duration <= 0.0) duration = 30.0;

    QString outputPath = QFileDialog::getSaveFileName(
        parentWindow, "Export Glitched Video", "", 
        "MP4 Video (*.mp4);;All Files (*)"
    );

    if (outputPath.isEmpty()) return;

    const bool logsWereEnabled = AppLogging::enabled();
    AppLogging::setEnabled(false);

    const bool asyncWasEnabled = VideoEngine::instance().isAsyncDecodeEnabled();
    VideoEngine::instance().setAsyncDecodeEnabled(false);

    if (wasPlaying) {
        togglePlaybackCallback();
    }

    int totalFrames = static_cast<int>(duration * fps);
    QProgressDialog progress("Exporting Video Frames...", "Cancel", 0, totalFrames, parentWindow);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();

    int exportW = glWidget->width();
    int exportH = glWidget->height();
    if (exportW % 2 != 0) exportW--;
    if (exportH % 2 != 0) exportH--;

    QString ffmpegPath = "ffmpeg.exe";
    QStringList arguments;
    arguments << "-y"
              << "-f" << "rawvideo"
              << "-pix_fmt" << "rgb24"
              << "-s" << QString("%1x%2").arg(exportW).arg(exportH)
              << "-r" << QString::number(fps)
              << "-i" << "pipe:0"
              << "-i" << activeFilePath
              << "-map" << "0:v:0"
              << "-map" << "1:a:0?"
              << "-c:v" << "libx264"
              << "-pix_fmt" << "yuv420p"
              << "-c:a" << "aac"
              << "-shortest"
              << outputPath;

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PATH", "C:\\msys64\\mingw64\\bin;C:\\msys64\\usr\\bin;" + env.value("PATH"));
    proc.setProcessEnvironment(env);
    proc.setProcessChannelMode(QProcess::MergedChannels);

    proc.start(ffmpegPath, arguments);
    if (!proc.waitForStarted()) {
        qWarning() << "Failed to start FFmpeg process.";
        VideoEngine::instance().setAsyncDecodeEnabled(asyncWasEnabled);
        AppLogging::setEnabled(logsWereEnabled);
        if (wasPlaying) {
            togglePlaybackCallback();
        }
        return;
    }

    const int rowBytes = exportW * 3;
    int lastUiUpdate = 0;

    for (int i = 0; i < totalFrames; ++i) {
        if (progress.wasCanceled()) {
            proc.kill();
            proc.waitForFinished();
            VideoEngine::instance().setAsyncDecodeEnabled(asyncWasEnabled);
            AppLogging::setEnabled(logsWereEnabled);
            if (wasPlaying) {
                togglePlaybackCallback();
            }
            return;
        }

        const double time = static_cast<double>(i) / fps;

        scrubCallback(time);

        QImage img = glWidget->grabFramebuffer().convertToFormat(QImage::Format_RGB888);
        if (img.isNull()) {
            qWarning() << "Export: Failed to capture frame" << i;
            proc.kill();
            proc.waitForFinished();
            VideoEngine::instance().setAsyncDecodeEnabled(asyncWasEnabled);
            AppLogging::setEnabled(logsWereEnabled);
            if (wasPlaying) {
                togglePlaybackCallback();
            }
            return;
        }

        if (img.width() != exportW || img.height() != exportH) {
            img = img.copy(0, 0, exportW, exportH);
        }

        for (int y = 0; y < exportH; ++y) {
            const char* row = reinterpret_cast<const char*>(img.constScanLine(y));
            if (proc.write(row, rowBytes) < 0) {
                qWarning() << "Export: Failed to write video frame data to FFmpeg.";
                proc.kill();
                proc.waitForFinished();
                VideoEngine::instance().setAsyncDecodeEnabled(asyncWasEnabled);
                AppLogging::setEnabled(logsWereEnabled);
                if (wasPlaying) {
                    togglePlaybackCallback();
                }
                return;
            }
        }

        if ((i - lastUiUpdate) >= 4 || i + 1 == totalFrames) {
            progress.setValue(i + 1);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
            lastUiUpdate = i;
        }
    }

    proc.closeWriteChannel();
    progress.setLabelText("Compiling final video with audio...");
    progress.setValue(totalFrames);

    if (!proc.waitForFinished(-1)) {
        qWarning() << "Failed to finish FFmpeg export. Error:" << proc.errorString();
        qWarning() << "FFmpeg output:" << proc.readAllStandardOutput();
    } else if (proc.exitCode() != 0) {
        qWarning() << "FFmpeg export failed with exit code" << proc.exitCode() << ":" << proc.readAllStandardOutput();
    } else {
        qDebug() << "FFmpeg export completed successfully!";
    }

    VideoEngine::instance().setAsyncDecodeEnabled(asyncWasEnabled);
    AppLogging::setEnabled(logsWereEnabled);

    scrubCallback(0.0);
    if (wasPlaying) {
        togglePlaybackCallback();
    }
}
