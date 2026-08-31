#include "mediaexporter.h"
#include "engine/videoengine.h"
#include "core/project.h"
#include "utils/logging.h"
#include "engine/glwidget.h"
#include "ui/preferencesdialog.h"
#include <QFileDialog>
#include <QProgressDialog>
#include <QProcess>
#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QElapsedTimer>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <future>

namespace {
class ExportConfigDialog : public QDialog {
public:
    ExportConfigDialog(QWidget* parent, int srcW, int srcH, double srcFps, double totalDur, double markIn, double markOut)
        : QDialog(parent), srcW(srcW), srcH(srcH), srcFps(srcFps), totalDur(totalDur), markIn(markIn), markOut(markOut) {
        setWindowTitle("Export Video");
        resize(460, 360);
        setModal(true);

        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(14, 14, 14, 14);
        mainLayout->setSpacing(10);

        QLabel* titleLabel = new QLabel("E X P O R T   S E T T I N G S", this);
        titleLabel->setStyleSheet("font-weight: bold; color: #e855f4; font-size: 12px;");
        mainLayout->addWidget(titleLabel);

        QFormLayout* form = new QFormLayout();
        form->setSpacing(8);

        resCombo = new QComboBox(this);
        resCombo->addItem(QString("Source Resolution (%1x%2)").arg(srcW).arg(srcH), QSize(srcW, srcH));
        resCombo->addItem("1080p Full HD (1920x1080)", QSize(1920, 1080));
        resCombo->addItem("4K Ultra HD (3840x2160)", QSize(3840, 2160));
        resCombo->addItem("720p HD (1280x720)", QSize(1280, 720));
        form->addRow("Resolution Preset:", resCombo);

        fpsCombo = new QComboBox(this);
        fpsCombo->addItem(QString("Source Framerate (%1 fps)").arg(srcFps, 0, 'f', 2), srcFps);
        fpsCombo->addItem("60 fps (Smooth)", 60.0);
        fpsCombo->addItem("30 fps (Standard)", 30.0);
        fpsCombo->addItem("24 fps (Cinematic)", 24.0);
        form->addRow("Framerate:", fpsCombo);

        qualityCombo = new QComboBox(this);
        qualityCombo->addItem("Archival / Master (CRF 16 - Lossless Visual)", 16);
        qualityCombo->addItem("High Quality (CRF 19 - Recommended)", 19);
        qualityCombo->addItem("Balanced (CRF 23 - Standard)", 23);
        qualityCombo->addItem("Web / Social (CRF 26 - Small File)", 26);
        qualityCombo->setCurrentIndex(1);
        form->addRow("Quality & Compression:", qualityCombo);

        rangeCombo = new QComboBox(this);
        rangeCombo->addItem(QString("Entire Timeline (0:00 - %1s)").arg(totalDur, 0, 'f', 1), 0);
        if (markIn >= 0.0 && markOut > markIn) {
            rangeCombo->addItem(QString("In/Out Marked Range (%1s - %2s)").arg(markIn, 0, 'f', 1).arg(markOut, 0, 'f', 1), 1);
            rangeCombo->setCurrentIndex(1);
        }
        form->addRow("Export Range:", rangeCombo);

        includeAudioCheck = new QCheckBox("Include Master Audio (Stereo AAC)", this);
        includeAudioCheck->setChecked(true);
        form->addRow("", includeAudioCheck);

        preserveAlphaCheck = new QCheckBox("Preserve transparency (MOV ProRes 4444; slower)", this);
        preserveAlphaCheck->setToolTip("Leave this off for ordinary opaque exports. MP4 output cannot preserve alpha.");
        preserveAlphaCheck->setChecked(false);
        form->addRow("", preserveAlphaCheck);

        mainLayout->addLayout(form);
        mainLayout->addStretch();

        QHBoxLayout* btnLayout = new QHBoxLayout();
        btnLayout->addStretch();
        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        QPushButton* exportBtn = new QPushButton("Choose Destination & Render...", this);
        exportBtn->setStyleSheet("QPushButton { background: #5a1a63; border: 1px solid #e855f4; color: white; padding: 6px 14px; font-weight: bold; } QPushButton:hover { background: #7a2286; }");
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(exportBtn, &QPushButton::clicked, this, &QDialog::accept);
        btnLayout->addWidget(cancelBtn);
        btnLayout->addWidget(exportBtn);
        mainLayout->addLayout(btnLayout);
    }

    ExportSettings getSettings(const QString& outputPath) const {
        ExportSettings s;
        s.outputPath = outputPath;
        QSize sz = resCombo->currentData().toSize();
        s.width = sz.width();
        s.height = sz.height();
        s.fps = fpsCombo->currentData().toDouble();
        s.crf = qualityCombo->currentData().toInt();
        s.includeAudio = includeAudioCheck->isChecked();
        s.preserveAlpha = preserveAlphaCheck->isChecked();

        if (rangeCombo->currentData().toInt() == 1 && markIn >= 0.0 && markOut > markIn) {
            s.startTime = markIn;
            s.duration = markOut - markIn;
        } else {
            s.startTime = 0.0;
            s.duration = totalDur;
        }
        return s;
    }

private:
    int srcW, srcH;
    double srcFps, totalDur, markIn, markOut;
    QComboBox* resCombo = nullptr;
    QComboBox* fpsCombo = nullptr;
    QComboBox* qualityCombo = nullptr;
    QComboBox* rangeCombo = nullptr;
    QCheckBox* includeAudioCheck = nullptr;
    QCheckBox* preserveAlphaCheck = nullptr;
};
}

void MediaExporter::exportVideo(
    QWidget* parentWindow,
    const QString& activeClipId,
    const QString& activeFilePath,
    GLWidget* glWidget,
    std::function<void(double)> scrubCallback,
    std::function<void()> togglePlaybackCallback,
    bool wasPlaying,
    double markIn,
    double markOut
) {
    if (activeClipId.isEmpty()) {
        QMessageBox::information(parentWindow, "Export Video", "Please import and select a video clip on the timeline before exporting.");
        return;
    }

    double fps = VideoEngine::instance().getFps(activeClipId.toStdString());
    if (fps <= 0.0) fps = 30.0;
    int srcW = glWidget->width();
    int srcH = glWidget->height();
    if (srcW <= 0) srcW = 1920;
    if (srcH <= 0) srcH = 1080;

    double duration = 0.0;
    for (const auto& t : Project::instance().getTracks()) {
        for (const auto& c : t.clips) {
            duration = std::max(duration, c.timelineStart + c.sourceDuration);
        }
    }
    if (duration <= 0.0) duration = 10.0;

    ExportConfigDialog configDialog(parentWindow, srcW, srcH, fps, duration, markIn, markOut);
    if (configDialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString suggestedName = QString("%1.mov").arg(activeClipId.isEmpty() ? "render" : activeClipId);
    const QString defaultPath = QDir::homePath() + "/" + suggestedName;
    QString selectedFilter = "QuickTime MOV (*.mov)";
    QString outputPath = QFileDialog::getSaveFileName(
        parentWindow,
        "Save Rendered Video",
        defaultPath,
        "QuickTime MOV (*.mov);;MP4 Video (*.mp4);;All Files (*)",
        &selectedFilter
    );

    if (outputPath.isEmpty()) return;

    const QString lower = outputPath.toLower();
    QString extension = ".mov";
    if (selectedFilter.contains("MP4", Qt::CaseInsensitive) || lower.endsWith(".mp4")) {
        extension = ".mp4";
    } else if (selectedFilter.contains("MOV", Qt::CaseInsensitive) || lower.endsWith(".mov")) {
        extension = ".mov";
    }

    if (!lower.endsWith(".mov") && !lower.endsWith(".mp4")) {
        outputPath += extension;
    }

    ExportSettings settings = configDialog.getSettings(outputPath);

    const bool logsWereEnabled = AppLogging::enabled();
    AppLogging::setEnabled(false);

    const bool asyncWasEnabled = VideoEngine::instance().isAsyncDecodeEnabled();
    VideoEngine::instance().setAsyncDecodeEnabled(false);

    if (wasPlaying) {
        togglePlaybackCallback();
    }

    int totalFrames = std::max(1, static_cast<int>(settings.duration * settings.fps));
    QProgressDialog progress("Initializing video render pipeline...", "Cancel Export", 0, totalFrames, parentWindow);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.show();

    int exportW = settings.width;
    int exportH = settings.height;
    if (exportW % 2 != 0) exportW--;
    if (exportH % 2 != 0) exportH--;

#ifdef _WIN32
    QString ffmpegPath = QDir(QCoreApplication::applicationDirPath()).filePath("ffmpeg.exe");
    if (!QFileInfo(ffmpegPath).isFile()) ffmpegPath = "ffmpeg.exe";
#else
    QString ffmpegPath = "ffmpeg";
#endif

    QStringList arguments;
    arguments << "-y"
              << "-f" << "rawvideo"
              << "-pix_fmt" << "rgba"
              << "-s" << QString("%1x%2").arg(exportW).arg(exportH)
              << "-r" << QString::number(settings.fps)
              << "-i" << "pipe:0";

    if (settings.includeAudio && !activeFilePath.isEmpty()) {
        arguments << "-ss" << QString::number(settings.startTime, 'f', 3)
                  << "-t" << QString::number(settings.duration, 'f', 3)
                  << "-i" << activeFilePath
                  << "-map" << "0:v:0"
                  << "-map" << "1:a:0?"
                  << "-c:a" << "aac"
                  << "-b:a" << "192k";
    } else {
        arguments << "-map" << "0:v:0";
    }

    const bool isMov = settings.outputPath.toLower().endsWith(".mov");
    if (settings.preserveAlpha && !isMov) {
        QMessageBox::information(parentWindow, "Transparency Export",
            "MP4 does not support the alpha format used by Z. This export will be opaque. "
            "Choose MOV and enable Preserve transparency when you need an alpha channel.");
        settings.preserveAlpha = false;
    }

    if (isMov && settings.preserveAlpha) {
        // ProRes 4444 preserves the source alpha plane in a MOV container.
        arguments << "-c:v" << "prores_ks"
                  << "-profile:v" << "4444"
                  << "-pix_fmt" << "yuva444p10le"
                  << "-alpha_bits" << "16"
                  << "-threads" << "0"
                  << "-movflags" << "+faststart";
    } else {
        // Opaque H.264 is much faster and smaller than ProRes 4444. FFmpeg
        // uses all available encoder workers with the automatic thread count.
        arguments << "-c:v" << "libx264"
                  << "-preset" << "medium"
                  << "-crf" << QString::number(settings.crf)
                  << "-pix_fmt" << "yuv420p"
                  << "-threads" << "0"
                  << "-movflags" << "+faststart";
    }
    arguments << settings.outputPath;

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef _WIN32
    env.insert("PATH", "C:\\msys64\\mingw64\\bin;C:\\msys64\\usr\\bin;" + env.value("PATH"));
#else
    env.insert("PATH", "/usr/local/bin:/opt/homebrew/bin:/usr/bin:" + env.value("PATH"));
#endif
    proc.setProcessEnvironment(env);
    proc.setProcessChannelMode(QProcess::MergedChannels);

    proc.start(ffmpegPath, arguments);
    if (!proc.waitForStarted()) {
        QMessageBox::critical(parentWindow, "Export Failed", "Could not start FFmpeg encoder process. Please verify FFmpeg is installed and accessible.");
        VideoEngine::instance().setAsyncDecodeEnabled(asyncWasEnabled);
        AppLogging::setEnabled(logsWereEnabled);
        if (wasPlaying) togglePlaybackCallback();
        return;
    }

    QElapsedTimer timer;
    timer.start();

    const int actualW = exportW;
    const int actualH = exportH;
    const int rowBytes = actualW * 4;

    // The QOpenGLWidget has one context, so timeline rendering itself must
    // remain ordered. Overlap the CPU image conversion/scale for frame N with
    // GPU rendering of frame N + 1; the bounded one-frame queue prevents 4K
    // renders from consuming unbounded RAM while FFmpeg encodes concurrently.
    auto prepareFrame = [actualW, actualH](QImage image) {
        if (image.width() != actualW || image.height() != actualH) {
            image = image.convertToFormat(QImage::Format_RGBA8888_Premultiplied)
                        .scaled(actualW, actualH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                        .convertToFormat(QImage::Format_RGBA8888);
        } else if (image.format() != QImage::Format_RGBA8888) {
            image = image.convertToFormat(QImage::Format_RGBA8888);
        }
        return image;
    };

    auto writeFrame = [&](const QImage& image) {
        for (int y = 0; y < actualH; ++y) {
            const char* row = reinterpret_cast<const char*>(image.constScanLine(y));
            if (proc.write(row, rowBytes) < 0) {
                return false;
            }
        }
        return true;
    };

    std::future<QImage> pendingFrame;
    int pendingFrameIndex = -1;
    int encodedFrames = 0;
    auto flushPendingFrame = [&]() -> bool {
        if (!pendingFrame.valid()) return true;
        const QImage image = pendingFrame.get();
        if (image.isNull() || !writeFrame(image)) return false;
        encodedFrames = pendingFrameIndex + 1;
        if (encodedFrames % 3 == 0 || encodedFrames == totalFrames) {
            const double elapsedSec = timer.elapsed() / 1000.0;
            const double fpsRender = encodedFrames / std::max(0.001, elapsedSec);
            const double remainingSec = (totalFrames - encodedFrames) / std::max(0.001, fpsRender);
            progress.setLabelText(QString("Rendering and encoding frame %1 of %2 (%3 fps)\nETA: %4s remaining")
                .arg(encodedFrames).arg(totalFrames)
                .arg(fpsRender, 0, 'f', 1)
                .arg(static_cast<int>(remainingSec)));
            progress.setValue(encodedFrames);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }
        return true;
    };

    for (int i = 0; i < totalFrames; ++i) {
        if (progress.wasCanceled()) {
            proc.kill();
            proc.waitForFinished();
            QFile::remove(settings.outputPath);
            VideoEngine::instance().setAsyncDecodeEnabled(asyncWasEnabled);
            AppLogging::setEnabled(logsWereEnabled);
            if (wasPlaying) togglePlaybackCallback();
            QMessageBox::information(parentWindow, "Export Canceled", "Video export was canceled by user.");
            return;
        }

        const double time = settings.startTime + (static_cast<double>(i) / settings.fps);
        scrubCallback(time);

        // Capture the renderer's transparent offscreen result, not the
        // QOpenGLWidget window surface (which may already be black/opaque).
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        QImage img = glWidget->grabRenderedFrame();
        if (img.isNull()) {
            proc.kill();
            proc.waitForFinished();
            VideoEngine::instance().setAsyncDecodeEnabled(asyncWasEnabled);
            AppLogging::setEnabled(logsWereEnabled);
            if (wasPlaying) togglePlaybackCallback();
            QMessageBox::critical(parentWindow, "Export Error", QString("Failed to capture GPU frame %1 of %2.").arg(i + 1).arg(totalFrames));
            return;
        }

        // Submit this frame before waiting for the previous CPU preparation,
        // allowing the preparation thread to run while OpenGL draws the next
        // timeline frame.
        if (!flushPendingFrame()) {
            proc.kill();
            proc.waitForFinished();
            VideoEngine::instance().setAsyncDecodeEnabled(asyncWasEnabled);
            AppLogging::setEnabled(logsWereEnabled);
            if (wasPlaying) togglePlaybackCallback();
            QMessageBox::critical(parentWindow, "Export Error", "Failed sending frame buffer to video encoder.");
            return;
        }
        pendingFrame = std::async(std::launch::async, prepareFrame, std::move(img));
        pendingFrameIndex = i;
    }

    if (!flushPendingFrame()) {
        proc.kill();
        proc.waitForFinished();
        VideoEngine::instance().setAsyncDecodeEnabled(asyncWasEnabled);
        AppLogging::setEnabled(logsWereEnabled);
        if (wasPlaying) togglePlaybackCallback();
        QMessageBox::critical(parentWindow, "Export Error", "Failed sending final frame buffer to video encoder.");
        return;
    }

    proc.closeWriteChannel();
    progress.setLabelText("Finalizing MOV container and encoding audio...");
    progress.setValue(totalFrames);

    if (!proc.waitForFinished(-1) || proc.exitCode() != 0) {
        QString errOutput = proc.readAllStandardOutput();
        QMessageBox::critical(parentWindow, "Export Failed", "FFmpeg failed encoding final video:\n" + errOutput);
    } else {
        QMessageBox msg(parentWindow);
        msg.setWindowTitle("Export Complete");
        msg.setText(QString("Video successfully rendered and saved!\n\nFile: %1").arg(settings.outputPath));
        QPushButton* openFolderBtn = msg.addButton("Open Containing Folder", QMessageBox::ActionRole);
        QPushButton* playBtn = msg.addButton("Play Video", QMessageBox::ActionRole);
        msg.addButton("Close", QMessageBox::RejectRole);
        msg.exec();

        if (msg.clickedButton() == openFolderBtn) {
            QFileInfo fileInfo(settings.outputPath);
            QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absolutePath()));
        } else if (msg.clickedButton() == playBtn) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(settings.outputPath));
        }
    }

    VideoEngine::instance().setAsyncDecodeEnabled(asyncWasEnabled);
    AppLogging::setEnabled(logsWereEnabled);

    scrubCallback(settings.startTime);
    if (wasPlaying) {
        togglePlaybackCallback();
    }
}
