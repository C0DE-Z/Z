#ifndef MEDIAIMPORTER_H
#define MEDIAIMPORTER_H

#include <QString>
#include <functional>

class MediaImporter {
public:
    using ProgressCallback = std::function<void(double progress)>;

    // Normal editing reads the validated source directly. This legacy-named
    // entry point remains so existing project/import code keeps its API.
    static QString transcodeToStandardMov(const QString& sourcePath, ProgressCallback progressCallback = {});
    static QString standardizedImportDir();
    static QString standardizedImportPathForSource(const QString& sourcePath);
    static bool isUsableVideoFile(const QString& path);
    // A separate opaque interframe proxy used only by Datamosh. Normal
    // editing continues to use the original media, including source alpha.
    static QString datamoshProxyPathForSource(const QString& sourcePath);
    // A P-frame-compatible H.264 MP4 can be read by a separate software
    // decoder directly. Non-H.264 sources and streams with frame reordering
    // still need Z's controlled P-frame proxy.
    static bool isDatamoshDirectSourceEligible(const QString& sourcePath);
    static QString transcodeToDatamoshProxy(const QString& sourcePath, ProgressCallback progressCallback = {});
};

#endif // MEDIAIMPORTER_H
