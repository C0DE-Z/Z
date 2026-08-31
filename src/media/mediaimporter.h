#ifndef MEDIAIMPORTER_H
#define MEDIAIMPORTER_H

#include <QString>

class MediaImporter {
public:
    static QString transcodeToStandardMov(const QString& sourcePath);
    static QString standardizedImportDir();
    static QString standardizedImportPathForSource(const QString& sourcePath);
    // A separate interframe proxy used only by Datamosh. Editing continues to
    // use the alpha-safe ProRes source produced by transcodeToStandardMov().
    static QString datamoshProxyPathForSource(const QString& sourcePath);
    static QString transcodeToDatamoshProxy(const QString& sourcePath);
};

#endif // MEDIAIMPORTER_H
