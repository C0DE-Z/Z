#include <QCoreApplication>
#include <QDebug>
#include "src/engine/videodecoder.h"
#include "src/engine/videoframe.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    if (argc < 2) {
        qWarning() << "Usage: test_transparency <video_file>";
        return 1;
    }
    
    QString inputFile = argv[1];
    qDebug() << "Testing transparency detection for:" << inputFile;
    
    VideoDecoder decoder;
    if (!decoder.openFile(inputFile.toStdString())) {
        qWarning() << "Failed to open file";
        return 1;
    }
    
    // Decode first 5 frames
    for (int i = 0; i < 5; ++i) {
        DecodedVideoFrame frame;
        if (decoder.decodeFrameAt(i * 0.1, frame)) {
            qDebug() << "\nFrame" << i << ":";
            qDebug() << "  Width:" << frame.width << "Height:" << frame.height;
            qDebug() << "  Has Alpha:" << frame.hasAlpha;
            qDebug() << "  RGB Data Size:" << frame.rgbData.size();
            qDebug() << "  Alpha Data Size:" << frame.alphaData.size();
            
            if (frame.hasAlpha && !frame.alphaData.empty()) {
                // Check alpha values
                int zeroCount = 0, fullCount = 0, partialCount = 0;
                for (uint8_t alpha : frame.alphaData) {
                    if (alpha == 0) zeroCount++;
                    else if (alpha == 255) fullCount++;
                    else partialCount++;
                }
                qDebug() << "  Alpha Distribution: Zero=" << zeroCount 
                         << "Full=" << fullCount << "Partial=" << partialCount;
            }
        } else {
            qDebug() << "Frame" << i << ": Failed to decode";
        }
    }
    
    return 0;
}
