#ifndef MEDIAEXPORTER_H
#define MEDIAEXPORTER_H

#include <QString>
#include <functional>
#include <QWidget>

class GLWidget;

class MediaExporter {
public:
    static void exportVideo(
        QWidget* parentWindow,
        const QString& activeClipId,
        const QString& activeFilePath,
        GLWidget* glWidget,
        std::function<void(double)> scrubCallback,
        std::function<void()> togglePlaybackCallback,
        bool wasPlaying
    );
};

#endif // MEDIAEXPORTER_H
