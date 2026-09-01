#include "videoengine.h"
#include "media/mediaimporter.h"
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
    {
        std::lock_guard<std::mutex> engineLock(engineMutex);
        const auto decoder = decoderForClipLocked(clipId, true);

        if (!decoder || !decoder->canUseAsyncFrameCache()) {
            return;
        }
    }
    std::lock_guard<std::mutex> lock(workerMutex);
    if (workerThread.joinable() == false) {
        workerThread = std::thread(&VideoEngine::workerLoop, this);
    }

    if (workerClipId == clipId && timestamp <= workerPrefetchUntil) {
        return;
    }

    workerClipId = clipId;
    workerTimestamp = timestamp;
    workerPrefetchUntil = timestamp + 2.0;
    ++workerGeneration;
    workerHasRequest = true;
    workerCv.notify_one();
}

bool VideoEngine::tryGetCachedFrame(const std::string& clipId, double timestamp, DecodedVideoFrame& outFrame) {
    {
        std::lock_guard<std::mutex> lock(engineMutex);
        const auto decoder = decoderForClipLocked(clipId, false);
        if (decoder && !decoder->canUseAsyncFrameCache()) return false;
    }
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
    {
        std::lock_guard<std::mutex> lock(engineMutex);
        const auto decoder = decoderForClipLocked(clipId, false);
        if (decoder && !decoder->canUseAsyncFrameCache()) return false;
    }
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

bool VideoEngine::loadVideo(const std::string& clipId, const std::string& filePath, const std::string& datamoshProxyPath) {
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

        datamoshDecoders.erase(clipId);
        asyncDatamoshDecoders.erase(clipId);
        datamoshActive[clipId] = false;
        directDatamoshSources.erase(clipId);
        datamoshSettings.erase(clipId);
        const QString sourcePath = QString::fromStdString(filePath);
        const bool useDirectSource = MediaImporter::isDatamoshDirectSourceEligible(sourcePath);
        const std::string packetSourcePath = useDirectSource
            ? filePath : datamoshProxyPath;
        if (!packetSourcePath.empty()) {
            // Software decoding is deliberate: hardware decoders commonly
            // conceal dropped reference packets, erasing the very artifacts
            // this effect creates.
            auto datamoshDecoder = std::make_shared<VideoDecoder>();
            if (datamoshDecoder->openFile(packetSourcePath, false, false)) {
                datamoshDecoders[clipId] = std::move(datamoshDecoder);
                auto asyncDatamoshDecoder = std::make_shared<VideoDecoder>();
                if (asyncDatamoshDecoder->openFile(packetSourcePath, false, false)) {
                    asyncDatamoshDecoders[clipId] = std::move(asyncDatamoshDecoder);
                }
                directDatamoshSources[clipId] = useDirectSource;
            } else {
                qWarning() << "Datamosh packet source could not be opened for" << QString::fromStdString(clipId);
            }
        }
        return true;
    }
    return false;
}

bool VideoEngine::hasDatamoshPacketSource(const std::string& clipId) const {
    std::lock_guard<std::mutex> lock(engineMutex);
    return datamoshDecoders.contains(clipId);
}

bool VideoEngine::usesDirectDatamoshSource(const std::string& clipId) const {
    std::lock_guard<std::mutex> lock(engineMutex);
    const auto it = directDatamoshSources.find(clipId);
    return it != directDatamoshSources.end() && it->second;
}

bool VideoEngine::getFrame(const std::string& clipId, double timestamp, DecodedVideoFrame& outFrame) {
    std::shared_ptr<VideoDecoder> decoder;
    {
        std::lock_guard<std::mutex> lock(engineMutex);
        decoder = decoderForClipLocked(clipId, false);
        if (!decoder) {
            return false;
        }
    }

    if (decoder->canUseAsyncFrameCache() && getFromCache(clipId, timestamp, outFrame)) {
        return true;
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
    const bool proxyAvailable = datamoshDecoders.contains(clipId);
    const int duplicateCount = std::max(1, pDupCount);
    const bool effectIsEffective = VideoDecoder::hasEffectiveDatamoshSettings(
        datamoshEnabled, iDropProb, pDupProb, duplicateCount, pDropProb);
    const bool isActive = effectIsEffective && proxyAvailable;
    const DatamoshSettings newSettings {
        isActive,
        isActive && iDropProb >= 0.5 ? 1.0 : 0.0,
        isActive && pDupProb > 0.0 && duplicateCount > 1 ? pDupProb : 0.0,
        duplicateCount,
        isActive && pDropProb > 0.0 ? pDropProb : 0.0
    };
    const auto oldSettings = datamoshSettings.find(clipId);
    const bool settingsChanged = oldSettings == datamoshSettings.end() || oldSettings->second != newSettings;
    if (!settingsChanged) {

        return;
    }
    datamoshSettings[clipId] = newSettings;
    datamoshActive[clipId] = isActive;
    if (effectIsEffective && !proxyAvailable && settingsChanged) {
        qWarning() << "Datamosh packet source is unavailable for" << QString::fromStdString(clipId);
    }
    if (auto it = decoders.find(clipId); it != decoders.end()) {
        it->second->setDatamoshing(false, 0.0, 0.0, 1, 0.0);
    }
    if (auto it = asyncDecoders.find(clipId); it != asyncDecoders.end()) {
        it->second->setDatamoshing(false, 0.0, 0.0, 1, 0.0);
    }
    if (auto it = datamoshDecoders.find(clipId); it != datamoshDecoders.end()) {
        it->second->setDatamoshing(isActive, iDropProb, pDupProb, duplicateCount, pDropProb);
    }
    if (auto it = asyncDatamoshDecoders.find(clipId); it != asyncDatamoshDecoders.end()) {
        it->second->setDatamoshing(isActive, iDropProb, pDupProb, duplicateCount, pDropProb);
    }
    invalidateCacheForClip(clipId);
    std::lock_guard<std::mutex> workerLock(workerMutex);
    ++workerGeneration;
    workerHasRequest = false;
    workerPrefetchUntil = -1.0;
}

void VideoEngine::setOpticalSmear(const std::string& clipId, bool smearEnabled, double frameMerge, double frameSmear, double colorBleed, double lumaBias) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setOpticalSmear(smearEnabled, frameMerge, frameSmear, colorBleed, lumaBias);
    }
    const auto asyncIt = asyncDecoders.find(clipId);
    if (asyncIt != asyncDecoders.end()) asyncIt->second->setOpticalSmear(smearEnabled, frameMerge, frameSmear, colorBleed, lumaBias);
    const auto datamoshIt = datamoshDecoders.find(clipId);
    if (datamoshIt != datamoshDecoders.end()) datamoshIt->second->setOpticalSmear(smearEnabled, frameMerge, frameSmear, colorBleed, lumaBias);
    const auto asyncDatamoshIt = asyncDatamoshDecoders.find(clipId);
    if (asyncDatamoshIt != asyncDatamoshDecoders.end()) asyncDatamoshIt->second->setOpticalSmear(smearEnabled, frameMerge, frameSmear, colorBleed, lumaBias);
}

void VideoEngine::setCpuXor(const std::string& clipId, bool xorEnabled, double xorValue, double intensity) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setCpuXor(xorEnabled, xorValue, intensity);
    }
    const auto asyncIt = asyncDecoders.find(clipId);
    if (asyncIt != asyncDecoders.end()) asyncIt->second->setCpuXor(xorEnabled, xorValue, intensity);
    const auto datamoshIt = datamoshDecoders.find(clipId);
    if (datamoshIt != datamoshDecoders.end()) datamoshIt->second->setCpuXor(xorEnabled, xorValue, intensity);
    const auto asyncDatamoshIt = asyncDatamoshDecoders.find(clipId);
    if (asyncDatamoshIt != asyncDatamoshDecoders.end()) asyncDatamoshIt->second->setCpuXor(xorEnabled, xorValue, intensity);
}

void VideoEngine::setCpuOr(const std::string& clipId, bool orEnabled, double orValue, double intensity) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setCpuOr(orEnabled, orValue, intensity);
    }
    const auto asyncIt = asyncDecoders.find(clipId);
    if (asyncIt != asyncDecoders.end()) asyncIt->second->setCpuOr(orEnabled, orValue, intensity);
    const auto datamoshIt = datamoshDecoders.find(clipId);
    if (datamoshIt != datamoshDecoders.end()) datamoshIt->second->setCpuOr(orEnabled, orValue, intensity);
    const auto asyncDatamoshIt = asyncDatamoshDecoders.find(clipId);
    if (asyncDatamoshIt != asyncDatamoshDecoders.end()) asyncDatamoshIt->second->setCpuOr(orEnabled, orValue, intensity);
}

void VideoEngine::setCpuAnd(const std::string& clipId, bool andEnabled, double andValue, double intensity) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setCpuAnd(andEnabled, andValue, intensity);
    }
    const auto asyncIt = asyncDecoders.find(clipId);
    if (asyncIt != asyncDecoders.end()) asyncIt->second->setCpuAnd(andEnabled, andValue, intensity);
    const auto datamoshIt = datamoshDecoders.find(clipId);
    if (datamoshIt != datamoshDecoders.end()) datamoshIt->second->setCpuAnd(andEnabled, andValue, intensity);
    const auto asyncDatamoshIt = asyncDatamoshDecoders.find(clipId);
    if (asyncDatamoshIt != asyncDatamoshDecoders.end()) asyncDatamoshIt->second->setCpuAnd(andEnabled, andValue, intensity);
}

void VideoEngine::setCpuXnor(const std::string& clipId, bool xnorEnabled, double xnorValue, double intensity) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setCpuXnor(xnorEnabled, xnorValue, intensity);
    }
    const auto asyncIt = asyncDecoders.find(clipId);
    if (asyncIt != asyncDecoders.end()) asyncIt->second->setCpuXnor(xnorEnabled, xnorValue, intensity);
    const auto datamoshIt = datamoshDecoders.find(clipId);
    if (datamoshIt != datamoshDecoders.end()) datamoshIt->second->setCpuXnor(xnorEnabled, xnorValue, intensity);
    const auto asyncDatamoshIt = asyncDatamoshDecoders.find(clipId);
    if (asyncDatamoshIt != asyncDatamoshDecoders.end()) asyncDatamoshIt->second->setCpuXnor(xnorEnabled, xnorValue, intensity);
}

void VideoEngine::setCpuNand(const std::string& clipId, bool nandEnabled, double nandValue, double intensity) {
    std::lock_guard<std::mutex> lock(engineMutex);
    auto it = decoders.find(clipId);
    if (it != decoders.end()) {
        it->second->setCpuNand(nandEnabled, nandValue, intensity);
    }
    const auto asyncIt = asyncDecoders.find(clipId);
    if (asyncIt != asyncDecoders.end()) asyncIt->second->setCpuNand(nandEnabled, nandValue, intensity);
    const auto datamoshIt = datamoshDecoders.find(clipId);
    if (datamoshIt != datamoshDecoders.end()) datamoshIt->second->setCpuNand(nandEnabled, nandValue, intensity);
    const auto asyncDatamoshIt = asyncDatamoshDecoders.find(clipId);
    if (asyncDatamoshIt != asyncDatamoshDecoders.end()) asyncDatamoshIt->second->setCpuNand(nandEnabled, nandValue, intensity);
}

void VideoEngine::setPlaybackQuality(int downscaleFactor) {
    std::lock_guard<std::mutex> lock(engineMutex);
    for (auto& pair : decoders) {
        pair.second->setPlaybackQuality(downscaleFactor);
    }
    for (auto& pair : asyncDecoders) {
        pair.second->setPlaybackQuality(downscaleFactor);
    }
    for (auto& pair : datamoshDecoders) {
        pair.second->setPlaybackQuality(downscaleFactor);
    }
    for (auto& pair : asyncDatamoshDecoders) {
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

bool VideoEngine::wasAudioPreloadSkipped(const std::string& clipId) {
    std::lock_guard<std::mutex> lock(engineMutex);
    const auto it = decoders.find(clipId);
    return it != decoders.end() && it->second->wasAudioPreloadSkipped();
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
    datamoshDecoders.clear();
    asyncDatamoshDecoders.clear();
    datamoshActive.clear();
    directDatamoshSources.clear();
    datamoshSettings.clear();
    {
        std::lock_guard<std::mutex> cacheLock(cacheMutex);
        frameCache.clear();
    }
}

std::shared_ptr<VideoDecoder> VideoEngine::decoderForClipLocked(const std::string& clipId, bool asynchronous) const {
    const bool useDatamosh = datamoshActive.contains(clipId) && datamoshActive.at(clipId);
    const auto& preferred = asynchronous ? asyncDatamoshDecoders : datamoshDecoders;
    const auto& fallback = asynchronous ? asyncDecoders : decoders;
    if (useDatamosh) {
        if (const auto it = preferred.find(clipId); it != preferred.end()) {
            return it->second;
        }
    }
    if (const auto it = fallback.find(clipId); it != fallback.end()) {
        return it->second;
    }
    return {};
}

void VideoEngine::addToCache(const std::string& clipId, double timestamp, std::shared_ptr<DecodedVideoFrame> frame) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    const size_t incomingBytes = frameByteSize(*frame);
    size_t cachedBytes = 0;
    for (const auto& entry : frameCache) {
        cachedBytes += frameByteSize(*entry.frame);
    }
    while (!frameCache.empty() && (frameCache.size() >= MAX_CACHE_SIZE || cachedBytes + incomingBytes > MAX_CACHE_BYTES)) {
        cachedBytes -= frameByteSize(*frameCache.front().frame);
        frameCache.erase(frameCache.begin());
    }
    frameCache.push_back({ clipId, timestamp, std::move(frame) });
}

void VideoEngine::invalidateCacheForClip(const std::string& clipId) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    std::erase_if(frameCache, [&clipId](const CacheEntry& entry) {
        return entry.clipId == clipId;
    });
}

size_t VideoEngine::frameByteSize(const DecodedVideoFrame& frame) {
    return frame.rgbData.size() + frame.alphaData.size();
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
            decoder = decoderForClipLocked(clipId, true);
        }

        if (!decoder) {
            continue;
        }

        double fps = std::max(1.0, decoder->getFps());
        double frameDuration = 1.0 / fps;
        {
            std::lock_guard<std::mutex> lock(workerMutex);
            workerPrefetchUntil = startTimestamp + (60.0 * frameDuration);
        }

        // A two-second horizon gives the decoder time to absorb packet-level
        // Datamosh work before playback reaches a frame. The cache's byte
        // budget remains authoritative, so high-resolution sources retain a
        // smaller window instead of causing memory growth.
        for (int i = 0; i < 60; ++i) {
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
            if (decoder->canUseAsyncFrameCache()) {
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
