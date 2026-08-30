#include "videoengine.h"
#include <map>
#include <algorithm>
#include <QDebug>


VideoEngine::~VideoEngine() {
    {
        std::lock_guard<std::mutex> lock(workerMutex);
        workerStop = true;
        workerHasRequest = false;
    }
    workerCv.notify_all();
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void VideoEngine::setAsyncDecodeEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(workerMutex);
    asyncDecodeEnabled = enabled;
    ++workerGeneration;
    workerHasRequest = false;
    workerPrefetchUntil = -1.0;
    workerCv.notify_all();
}

bool VideoEngine::isAsyncDecodeEnabled() const {
    std::lock_guard<std::mutex> lock(workerMutex);
    return asyncDecodeEnabled;
}

void VideoEngine::requestFrameAsync(const std::string& clipId, double timestamp) {
    std::lock_guard<std::mutex> lock(workerMutex);
    if (workerThread.joinable() == false) {
        workerThread = std::thread(&VideoEngine::workerLoop, this);
    }

    // Let the worker finish its contiguous decode window. Replacing its request
    // every UI tick was forcing repeated seeks and collapsing playback FPS.
    if (workerClipId == clipId && timestamp <= workerPrefetchUntil) {
        return;
    }

    workerClipId = clipId;
    workerTimestamp = timestamp;
    workerPrefetchUntil = timestamp + 1.0;
    ++workerGeneration;
    workerHasRequest = true;
    workerCv.notify_one();
}

bool VideoEngine::tryGetCachedFrame(const std::string& clipId, double timestamp, DecodedVideoFrame& outFrame) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    for (const auto& entry : frameCache) {
        if (entry.clipId == clipId && std::abs(entry.timestamp - timestamp) < 0.020) {
            outFrame = *entry.frame;
            return true;
        }
    }
    return false;
}

bool VideoEngine::tryGetNearestCachedFrame(const std::string& clipId, double timestamp, DecodedVideoFrame& outFrame, double maxAgeSeconds) {
    std::lock_guard<std::mutex> lock(cacheMutex);

    const CacheEntry* bestEntry = nullptr;
    double bestDistance = maxAgeSeconds;

    for (const auto& entry : frameCache) {
        if (entry.clipId != clipId) {
            continue;
        }

        const double distance = std::abs(timestamp - entry.timestamp);
        if (distance > bestDistance) {
            continue;
        }

        if (!bestEntry || distance < bestDistance) {
            bestDistance = distance;
            bestEntry = &entry;
        }
    }

    if (bestEntry) {
        outFrame = *bestEntry->frame;
        return true;
    }

    return false;
}

bool VideoEngine::loadVideo(const std::string& clipId, const std::string& filePath) {
    std::lock_guard<std::mutex> lock(engineMutex);

    auto decoder = std::make_shared<VideoDecoder>();
    if (decoder->openFile(filePath)) {
        decoders[clipId] = decoder;
        auto asyncDecoder = std::make_shared<VideoDecoder>();
        if (asyncDecoder->openFile(filePath, false)) {
            asyncDecoders[clipId] = std::move(asyncDecoder);
        } else {
            asyncDecoders.erase(clipId);
        }
        return true;
    }
    return false;
}

bool VideoEngine::getFrame(const std::string& clipId, double timestamp, DecodedVideoFrame& outFrame) {
    if (getFromCache(clipId, timestamp, outFrame)) {
        return true;
    }

    std::shared_ptr<VideoDecoder> decoder;
    {
        std::lock_guard<std::mutex> lock(engineMutex);
        auto it = decoders.find(clipId);
        if (it == decoders.end()) {
            return false;
        }
        decoder = it->second;
    }

    auto framePtr = std::make_shared<DecodedVideoFrame>();
    if (decoder->decodeFrameAt(timestamp, *framePtr)) {
        outFrame = *framePtr;
        addToCache(clipId, timestamp, std::move(framePtr));
        return true;
    }

    return false;
}

double VideoEngine::getDuration(const std::string& clipId) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        return it->second->getDuration();
    }
    return 0.0;
}

double VideoEngine::getFps(const std::string& clipId) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        return it->second->getFps();
    }
    return 0.0;
}

void VideoEngine::setDatamoshing(const std::string& clipId, bool datamoshEnabled, double iDropProb, double pDupProb, int pDupCount, double pDropProb) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setDatamoshing(datamoshEnabled, iDropProb, pDupProb, pDupCount, pDropProb);
    }
}

void VideoEngine::setOpticalSmear(const std::string& clipId, bool smearEnabled, double frameMerge, double frameSmear, double colorBleed, double lumaBias) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setOpticalSmear(smearEnabled, frameMerge, frameSmear, colorBleed, lumaBias);
    }
}

void VideoEngine::setCpuXor(const std::string& clipId, bool xorEnabled, double xorValue, double intensity) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setCpuXor(xorEnabled, xorValue, intensity);
    }
}

void VideoEngine::setCpuOr(const std::string& clipId, bool orEnabled, double orValue, double intensity) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setCpuOr(orEnabled, orValue, intensity);
    }
}

void VideoEngine::setCpuAnd(const std::string& clipId, bool andEnabled, double andValue, double intensity) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setCpuAnd(andEnabled, andValue, intensity);
    }
}

void VideoEngine::setCpuXnor(const std::string& clipId, bool xnorEnabled, double xnorValue, double intensity) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setCpuXnor(xnorEnabled, xnorValue, intensity);
    }
}

void VideoEngine::setCpuNand(const std::string& clipId, bool nandEnabled, double nandValue, double intensity) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setCpuNand(nandEnabled, nandValue, intensity);
    }
}

void VideoEngine::setPlaybackQuality(int downscaleFactor) {
    std::lock_guard<std::mutex> lock(engineMutex);
    for (auto& pair : decoders) {
        pair.second->setPlaybackQuality(downscaleFactor);
    }
    for (auto& pair : asyncDecoders) {
        pair.second->setPlaybackQuality(downscaleFactor);
    }
}

bool VideoEngine::getAudioSamples(const std::string& clipId, std::vector<float>& outSamples) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        outSamples = it->second->getAudioSamples();
        return true;
    }
    return false;
}

void VideoEngine::clear() {
    {
        std::lock_guard<std::mutex> lock(workerMutex);
        workerHasRequest = false;
        ++workerGeneration;
    }
    std::lock_guard<std::mutex> lock(engineMutex);
    decoders.clear();
    asyncDecoders.clear();
    {
        std::lock_guard<std::mutex> cacheLock(cacheMutex);
        frameCache.clear();
    }
}

void VideoEngine::addToCache(const std::string& clipId, double timestamp, std::shared_ptr<DecodedVideoFrame> frame) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    if (frameCache.size() >= MAX_CACHE_SIZE) {
        frameCache.erase(frameCache.begin());
    }
    frameCache.push_back({ clipId, timestamp, std::move(frame) });
}

bool VideoEngine::getFromCache(const std::string& clipId, double timestamp, DecodedVideoFrame& outFrame) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    for (const auto& entry : frameCache) {
        if (entry.clipId == clipId && std::abs(entry.timestamp - timestamp) < 0.020) {
            outFrame = *entry.frame;
            return true;
        }
    }
    return false;
}

void VideoEngine::workerLoop() {
    while (true) {
        std::string clipId;
        double startTimestamp = 0.0;
        uint64_t generation = 0;

        {
            std::unique_lock<std::mutex> lock(workerMutex);
            workerCv.wait(lock, [&] {
                return workerStop || (workerHasRequest && asyncDecodeEnabled);
            });

            if (workerStop) {
                return;
            }

            if (!workerHasRequest || !asyncDecodeEnabled) {
                continue;
            }

            clipId = workerClipId;
            startTimestamp = workerTimestamp;
            generation = workerGeneration;
            workerHasRequest = false;
        }

        std::shared_ptr<VideoDecoder> decoder;
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            auto it = asyncDecoders.find(clipId);
            if (it != asyncDecoders.end()) {
                decoder = it->second;
            }
        }

        if (!decoder) {
            continue;
        }

        double fps = std::max(1.0, decoder->getFps());
        double frameDuration = 1.0 / fps;
        {
            std::lock_guard<std::mutex> lock(workerMutex);
            workerPrefetchUntil = startTimestamp + (30.0 * frameDuration);
        }

        for (int i = 0; i < 30; ++i) {
            {
                std::lock_guard<std::mutex> lock(workerMutex);
                if (workerStop || !asyncDecodeEnabled || workerHasRequest || generation != workerGeneration) {
                    break;
                }
            }

            double targetTime = startTimestamp + (i * frameDuration);
            if (targetTime > decoder->getDuration()) {
                break;
            }

            bool alreadyCached = false;
            {
                std::lock_guard<std::mutex> lock(cacheMutex);
                for (const auto& entry : frameCache) {
                    if (entry.clipId == clipId && std::abs(entry.timestamp - targetTime) < 0.01) {
                        alreadyCached = true;
                        break;
                    }
                }
            }

            if (alreadyCached) {
                continue;
            }

            DecodedVideoFrame decoded;
            if (decoder->decodeFrameAt(targetTime, decoded)) {
                {
                    std::lock_guard<std::mutex> lock(workerMutex);
                    if (workerStop || !asyncDecodeEnabled || workerHasRequest || generation != workerGeneration) {
                        break;
                    }
                }
                auto framePtr = std::make_shared<DecodedVideoFrame>(std::move(decoded));
                addToCache(clipId, targetTime, std::move(framePtr));
            }
        }
    }
}
