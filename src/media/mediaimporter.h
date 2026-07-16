#ifndef MEDIAIMPORTER_H
#define MEDIAIMPORTER_H

#include <QString>

class MediaImporter {
public:
    static QString transcodeToStandardMp4(const QString& sourcePath);
    static QString standardizedImportDir();
    static QString standardizedImportPathForSource(const QString& sourcePath);
};

#endif // MEDIAIMPORTER_H
