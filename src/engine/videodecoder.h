#pragma once

#include <string>
#include <vector>
#include <mutex>
#include "videoframe.h"

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;
struct SwrContext; 
struct AVBufferRef;

class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    bool openFile(const std::string& filePath);
    void close();

    bool decodeFrameAt(double timestamp, DecodedVideoFrame& outFrame);

    double getDuration() const { return duration; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    double getFps() const { return fps; }
    const std::vector<float>& getAudioSamples() const { return audioSamples; }

    void setDatamoshing(bool datamoshEnabled, double iDropProb, double pDupProb, int pDupCount, double pDropProb);
    void setOpticalSmear(bool smearEnabled, double frameMerge, double frameSmear, double colorBleed, double lumaBias);
    void setCpuXor(bool xorEnabled, double xorValue, double intensity);
    void setCpuOr(bool orEnabled, double orValue, double intensity);
    void setCpuAnd(bool andEnabled, double andValue, double intensity);
    void setCpuXnor(bool xnorEnabled, double xnorValue, double intensity);
    void setCpuNand(bool nandEnabled, double nandValue, double intensity);
    void setPlaybackQuality(int downscaleFactor);

private:
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;
    int videoStreamIndex = -1;
    AVFrame* frame = nullptr;
    AVFrame* swFrame = nullptr; 
    AVPacket* packet = nullptr;
    SwsContext* swsCtx = nullptr;
    AVBufferRef* hwDeviceCtx = nullptr;
    int hwPixFmt = -1; 

    AVCodecContext* audioCodecCtx = nullptr;
    int audioStreamIndex = -1;
    SwrContext* swrCtx = nullptr;
    std::vector<float> audioSamples; 

    double duration = 0.0;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    int playbackQualityScale = 1;
    double timeBase = 0.0;
    double lastDecodedTime = -999.0;
    bool datamoshEnabled = false;
    double iFrameDropProb = 0.0;
    double pFrameDuplicateProb = 0.0;
    int pFrameDuplicateCount = 1;
    double pFrameDropProb = 0.0;

    bool opticalSmearEnabled = false;
    double mergeStrength = 0.0;
    double smearStrength = 0.0;
    double bleedStrength = 0.0;
    double lumaStrength = 0.0;
    bool cpuXorEnabled = false;
    double cpuXorValue = 0.5;
    double cpuXorIntensity = 1.0;

    bool cpuOrEnabled = false;
    double cpuOrValue = 0.5;
    double cpuOrIntensity = 1.0;

    bool cpuAndEnabled = false;
    double cpuAndValue = 1.0;
    double cpuAndIntensity = 1.0;

    bool cpuXnorEnabled = false;
    double cpuXnorValue = 0.5;
    double cpuXnorIntensity = 1.0;

    bool cpuNandEnabled = false;
    double cpuNandValue = 0.5;
    double cpuNandIntensity = 1.0;

    bool hasReferenceFrame = false;
    std::vector<uint8_t> referenceFrameRgb;
    mutable std::mutex decodeMutex;

    bool seekTo(double timestamp);
    void preDecodeAudio(const std::string& filePath);
};
