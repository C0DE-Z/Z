#ifndef MEDIAEXPORTER_H
#define MEDIAEXPORTER_H

#include <QString>
#include <functional>
#include <QWidget>

class GLWidget;

struct ExportSettings {
    QString outputPath;
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
    int crf = 22;
    double startTime = 0.0;
    double duration = 0.0;
    bool includeAudio = true;
    bool preserveAlpha = false;
};

class MediaExporter {
public:
    static void exportVideo(
        QWidget* parentWindow,
        const QString& activeClipId,
        const QString& activeFilePath,
        GLWidget* glWidget,
        std::function<void(double)> scrubCallback,
        std::function<void()> togglePlaybackCallback,
        bool wasPlaying,
        double markIn = -1.0,
        double markOut = -1.0
    );
};

#endif // MEDIAEXPORTER_H
