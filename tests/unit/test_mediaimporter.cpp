#include "media/mediaimporter.h"
#include "engine/videodecoder.h"
#include <QFileInfo>
#include <QTemporaryFile>
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
    assert(!MediaImporter::isDatamoshDirectSourceEligible(source));

    // An effect instance is not enough to activate packet Datamosh. All-zero
    // settings and a repeat count of one must preserve the original decoder.
    assert(!VideoDecoder::hasEffectiveDatamoshSettings(true, 0.0, 0.0, 1, 0.0));
    assert(!VideoDecoder::hasEffectiveDatamoshSettings(true, 0.0, 1.0, 1, 0.0));
    assert(VideoDecoder::hasEffectiveDatamoshSettings(true, 0.0, 1.0, 2, 0.0));
    assert(VideoDecoder::hasEffectiveDatamoshSettings(true, 1.0, 0.0, 1, 0.0));
    assert(VideoDecoder::hasEffectiveDatamoshSettings(true, 0.0, 0.0, 1, 0.01));

    // Normal imports retain their own validated source format rather than
    // creating an opaque video-only cache or a YUVA ProRes intermediate.
    assert(MediaImporter::transcodeToStandardMov(source).isEmpty());

    QTemporaryFile incompleteMedia;
    assert(incompleteMedia.open());
    assert(incompleteMedia.write("not a media container") > 0);
    incompleteMedia.flush();
    assert(!MediaImporter::isUsableVideoFile(incompleteMedia.fileName()));

    std::cout << "[PASS] test_mediaimporter_standardized_mov_output\n";
    return 0;
}
