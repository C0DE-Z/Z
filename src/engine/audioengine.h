#ifndef AUDIOENGINE_H
#define AUDIOENGINE_H

#include <vector>
#include <mutex>

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

    double getBass() const { return bassValue; }
    double getMid() const { return midValue; }
    double getHigh() const { return highValue; }

    void setPlayheadTime(double time);
    double getPlayheadTime() const;

    void loadClipSamples(const std::vector<float>& samples, double timelineOffset);
    void clearClipSamples();

    void processAudio(float* outputBuffer, unsigned long framesPerBuffer);

private:
    AudioEngine() = default;

    bool isInitialized = false;
    void* paStream = nullptr; 
    bool isRunning = false;

    double bassValue = 0.0;
    double midValue = 0.0;
    double highValue = 0.0;

    bool activePlayback = false;
    mutable std::mutex analysisMutex;

    double playheadTime = 0.0;
    std::vector<float> clipSamples;
    double clipTimelineOffset = 0.0;
    mutable std::mutex audioMutex;
};

#endif
