#include "media/mediaimporter.h"
#include <QFileInfo>
#include <cassert>
#include <iostream>

int main() {
    const QString source = QStringLiteral("C:/tmp/transparent_clip.png");
    const QString path = MediaImporter::standardizedImportPathForSource(source);

    assert(path.endsWith(".mov", Qt::CaseInsensitive));
    assert(path.contains("transparent_clip"));
    assert(!path.endsWith(".mp4", Qt::CaseInsensitive));

    const QString datamoshPath = MediaImporter::datamoshProxyPathForSource(source);
    assert(datamoshPath.endsWith("_datamosh.mp4", Qt::CaseInsensitive));
    assert(datamoshPath.contains("transparent_clip"));
    assert(datamoshPath != path);

    std::cout << "[PASS] test_mediaimporter_standardized_mov_output\n";
    return 0;
}
