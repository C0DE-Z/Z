#ifndef VIDEOENGINE_H
#define VIDEOENGINE_H

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <map>
#include <thread>
#include <condition_variable>
#include "videodecoder.h"

class VideoEngine {
public:
    static VideoEngine& instance() {
        static VideoEngine inst;
        return inst;
    }

    ~VideoEngine();

    bool loadVideo(const std::string& clipId, const std::string& filePath);
    bool getFrame(const std::string& clipId, double timestamp, DecodedVideoFrame& outFrame);
    void requestFrameAsync(const std::string& clipId, double timestamp);
    bool tryGetCachedFrame(const std::string& clipId, double timestamp, DecodedVideoFrame& outFrame);
    bool tryGetNearestCachedFrame(const std::string& clipId, double timestamp, DecodedVideoFrame& outFrame, double maxAgeSeconds = 0.08);
    void setAsyncDecodeEnabled(bool enabled);
    bool isAsyncDecodeEnabled() const;
    void setDatamoshing(const std::string& clipId, bool datamoshEnabled, double iDropProb, double pDupProb, int pDupCount, double pDropProb);
    void setOpticalSmear(const std::string& clipId, bool smearEnabled, double frameMerge, double frameSmear, double colorBleed, double lumaBias);
    void setCpuXor(const std::string& clipId, bool xorEnabled, double xorValue, double intensity);
    void setCpuOr(const std::string& clipId, bool orEnabled, double orValue, double intensity);
    void setCpuAnd(const std::string& clipId, bool andEnabled, double andValue, double intensity);
    void setCpuXnor(const std::string& clipId, bool xnorEnabled, double xnorValue, double intensity);
    void setCpuNand(const std::string& clipId, bool nandEnabled, double nandValue, double intensity);
    void setPlaybackQuality(int downscaleFactor);

    double getDuration(const std::string& clipId);

    double getFps(const std::string& clipId);
    bool getAudioSamples(const std::string& clipId, std::vector<float>& outSamples);

    void clear();

private:
    VideoEngine() = default;
    std::map<std::string, std::shared_ptr<VideoDecoder>> decoders;
    std::mutex engineMutex;
    std::mutex cacheMutex;

    std::thread workerThread;
    mutable std::mutex workerMutex;
    std::condition_variable workerCv;
    bool workerStop = false;
    bool workerHasRequest = false;
    bool asyncDecodeEnabled = false;
    std::string workerClipId;
    double workerTimestamp = 0.0;

    static const size_t MAX_CACHE_SIZE = 16;

    struct CacheEntry {
        std::string clipId;
        double timestamp;
        std::shared_ptr<DecodedVideoFrame> frame;
    };
    std::vector<CacheEntry> frameCache;

    void addToCache(const std::string& clipId, double timestamp, std::shared_ptr<DecodedVideoFrame> frame);
    bool getFromCache(const std::string& clipId, double timestamp, DecodedVideoFrame& outFrame);
    void workerLoop();
};

#endif
