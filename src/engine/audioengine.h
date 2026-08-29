#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>

class AudioEngine {
public:
    static AudioEngine& instance() {
        static AudioEngine inst;
        return inst;
    }

    bool init();
    void shutdown();

    bool start();
    void stop();

    float getBass() const { return bassValue.load(std::memory_order_relaxed); }
    float getMid() const { return midValue.load(std::memory_order_relaxed); }
    float getHigh() const { return highValue.load(std::memory_order_relaxed); }

    void setPlayheadTime(double time);
    double getPlayheadTime() const;

    void loadClipSamples(const std::vector<float>& samples, double timelineOffset, double sourceOffset = 0.0, double sourceDuration = -1.0);
    void clearClipSamples();

    void processAudio(float* outputBuffer, unsigned long framesPerBuffer);

private:
    AudioEngine() = default;
    ~AudioEngine() { shutdown(); }
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    std::atomic<bool> isInitialized{false};
    void* paStream = nullptr; 
    std::atomic<bool> isRunning{false};
    std::atomic<bool> activePlayback{false};

    std::atomic<float> bassValue{0.0f};
    std::atomic<float> midValue{0.0f};
    std::atomic<float> highValue{0.0f};

    double playheadTime = 0.0;
    std::vector<float> clipSamples;
    double clipTimelineOffset = 0.0;
    double clipSourceOffset = 0.0;
    double clipSourceDuration = -1.0;
    mutable std::mutex audioMutex;
};

#endif
