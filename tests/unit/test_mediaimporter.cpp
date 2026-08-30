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

    std::cout << "[PASS] test_mediaimporter_standardized_mov_output\n";
    return 0;
}
