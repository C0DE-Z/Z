#include "audioengine.h"
#include <portaudio.h>
#include <cmath>
#include <QDebug>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int paCallback(const void* inputBuffer, void* outputBuffer,
                      unsigned long framesPerBuffer,
                      const PaStreamCallbackTimeInfo* timeInfo,
                      PaStreamCallbackFlags statusFlags,
                      void* userData) {
    (void)inputBuffer;
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;
    AudioEngine::instance().processAudio(static_cast<float*>(outputBuffer), framesPerBuffer);
    return paContinue;
}

bool AudioEngine::init() {
    if (isInitialized.load(std::memory_order_acquire)) return true;

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        qWarning() << "PortAudio: Failed to initialize:" << Pa_GetErrorText(err);
        return false;
    }

    isInitialized.store(true, std::memory_order_release);
    start();
    activePlayback.store(false, std::memory_order_release); 
    return true;
}

void AudioEngine::shutdown() {
    if (!isInitialized.load(std::memory_order_acquire)) return;
    activePlayback.store(false, std::memory_order_release);
    if (isRunning.load(std::memory_order_acquire) && paStream) {
        PaStream* stream = static_cast<PaStream*>(paStream);
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        paStream = nullptr;
        isRunning.store(false, std::memory_order_release);
    }

    Pa_Terminate();
    isInitialized.store(false, std::memory_order_release);
}

bool AudioEngine::start() {
    if (!isInitialized.load(std::memory_order_acquire)) return false;
    if (!isRunning.load(std::memory_order_acquire)) {
        PaStream* stream = nullptr;
        PaError err = Pa_OpenDefaultStream(
            &stream,
            0, 2, paFloat32, 44100, 512, paCallback, nullptr
        );

        if (err != paNoError) {
            qWarning() << "PortAudio: Failed to open stream:" << Pa_GetErrorText(err);
            return false;
        }

        err = Pa_StartStream(stream);
        if (err != paNoError) {
            qWarning() << "PortAudio: Failed to start stream:" << Pa_GetErrorText(err);
            Pa_CloseStream(stream);
            return false;
        }

        paStream = stream;
        isRunning.store(true, std::memory_order_release);
    }

    activePlayback.store(true, std::memory_order_release);
    return true;
}

void AudioEngine::stop() {
    activePlayback.store(false, std::memory_order_release);
}

void AudioEngine::setPlayheadTime(double time) {
    std::lock_guard<std::mutex> lock(audioMutex);
    playheadTime = time;
}

double AudioEngine::getPlayheadTime() const {
    std::lock_guard<std::mutex> lock(audioMutex);
    return playheadTime;
}

void AudioEngine::loadClipSamples(const std::vector<float>& samples, double timelineOffset, double sourceOffset, double sourceDuration) {
    std::lock_guard<std::mutex> lock(audioMutex);
    clipSamples = samples;
    clipTimelineOffset = timelineOffset;
    clipSourceOffset = std::max(0.0, sourceOffset);
    clipSourceDuration = sourceDuration;
}

void AudioEngine::clearClipSamples() {
    std::lock_guard<std::mutex> lock(audioMutex);
    clipSamples.clear();
    clipTimelineOffset = 0.0;
    clipSourceOffset = 0.0;
    clipSourceDuration = -1.0;
}

void AudioEngine::processAudio(float* outputBuffer, unsigned long framesPerBuffer) {
    if (!outputBuffer) return;

    if (!activePlayback.load(std::memory_order_relaxed)) {
        for (unsigned long i = 0; i < framesPerBuffer * 2; ++i) {
            outputBuffer[i] = 0.0f;
        }
        float b = bassValue.load(std::memory_order_relaxed) * 0.85f;
        float m = midValue.load(std::memory_order_relaxed) * 0.85f;
        float h = highValue.load(std::memory_order_relaxed) * 0.85f;
        bassValue.store(b, std::memory_order_relaxed);
        midValue.store(m, std::memory_order_relaxed);
        highValue.store(h, std::memory_order_relaxed);
        return;
    }

    std::lock_guard<std::mutex> lock(audioMutex);

    double bassSum = 0.0;
    double midSum = 0.0;
    double highSum = 0.0;

    const bool inClipRange = !clipSamples.empty() && 
                             (playheadTime >= clipTimelineOffset) && 
                             (clipSourceDuration < 0.0 || playheadTime < clipTimelineOffset + clipSourceDuration);

    if (inClipRange) {
        double localTime = (playheadTime - clipTimelineOffset) + clipSourceOffset;
        size_t startSampleIdx = static_cast<size_t>(std::max(0.0, localTime) * 44100.0) * 2;

        for (unsigned long i = 0; i < framesPerBuffer; ++i) {
            size_t idx = startSampleIdx + i * 2;
            float leftSample = 0.0f;
            float rightSample = 0.0f;

            if (idx + 1 < clipSamples.size()) {
                leftSample = clipSamples[idx];
                rightSample = clipSamples[idx + 1];
            }

            outputBuffer[i * 2] = leftSample;
            outputBuffer[i * 2 + 1] = rightSample;

            float mono = (leftSample + rightSample) * 0.5f;
            double absVal = std::abs(mono);
            bassSum += absVal * 1.5;
            midSum += absVal * 1.0;
            highSum += absVal * 0.8;
        }

        playheadTime += static_cast<double>(framesPerBuffer) / 44100.0;
    } else {
        for (unsigned long i = 0; i < framesPerBuffer * 2; ++i) {
            outputBuffer[i] = 0.0f;
        }
        playheadTime += static_cast<double>(framesPerBuffer) / 44100.0;
    }

    if (framesPerBuffer > 0) {
        float newBass = static_cast<float>((bassSum / framesPerBuffer) * 12.0);
        float newMid = static_cast<float>((midSum / framesPerBuffer) * 8.0);
        float newHigh = static_cast<float>((highSum / framesPerBuffer) * 20.0);

        float curB = bassValue.load(std::memory_order_relaxed);
        float curM = midValue.load(std::memory_order_relaxed);
        float curH = highValue.load(std::memory_order_relaxed);

        bassValue.store(curB * 0.8f + std::clamp(newBass, 0.0f, 1.0f) * 0.2f, std::memory_order_relaxed);
        midValue.store(curM * 0.8f + std::clamp(newMid, 0.0f, 1.0f) * 0.2f, std::memory_order_relaxed);
        highValue.store(curH * 0.8f + std::clamp(newHigh, 0.0f, 1.0f) * 0.2f, std::memory_order_relaxed);
    }
}
