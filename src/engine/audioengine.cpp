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
    AudioEngine::instance().processAudio(static_cast<float*>(outputBuffer), framesPerBuffer);
    return paContinue;
}

bool AudioEngine::init() {
    if (isInitialized) return true;

    PaError err = Pa_Initialize();
    if (err != paNoError) {
        qWarning() << "PortAudio: Failed to initialize:" << Pa_GetErrorText(err);
        return false;
    }

    isInitialized = true;
    start();
    activePlayback = false; 
    return true;
}

void AudioEngine::shutdown() {
    if (!isInitialized) return;
    activePlayback = false;
    if (isRunning && paStream) {
        PaStream* stream = static_cast<PaStream*>(paStream);
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        paStream = nullptr;
        isRunning = false;
    }

    Pa_Terminate();
    isInitialized = false;
}

bool AudioEngine::start() {
    if (!isInitialized) return false;
    if (!isRunning) {
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
        isRunning = true;
    }

    activePlayback = true;
    return true;
}

void AudioEngine::stop() {
    activePlayback = false;
}

void AudioEngine::setPlayheadTime(double time) {
    std::lock_guard<std::mutex> lock(audioMutex);
    playheadTime = time;
}

double AudioEngine::getPlayheadTime() const {
    std::lock_guard<std::mutex> lock(audioMutex);
    return playheadTime;
}

void AudioEngine::loadClipSamples(const std::vector<float>& samples, double timelineOffset) {
    std::lock_guard<std::mutex> lock(audioMutex);
    clipSamples = samples;
    clipTimelineOffset = timelineOffset;
}

void AudioEngine::clearClipSamples() {
    std::lock_guard<std::mutex> lock(audioMutex);
    clipSamples.clear();
}

void AudioEngine::processAudio(float* outputBuffer, unsigned long framesPerBuffer) {
    if (!activePlayback) {
        for (unsigned long i = 0; i < framesPerBuffer * 2; ++i) {
            outputBuffer[i] = 0.0f;
        }
        std::lock_guard<std::mutex> lock(analysisMutex);
        bassValue *= 0.85;
        midValue *= 0.85;
        highValue *= 0.85;
        return;
    }

    std::lock_guard<std::mutex> lock(audioMutex);

    double bassSum = 0.0;
    double midSum = 0.0;
    double highSum = 0.0;

    if (!clipSamples.empty() && playheadTime >= clipTimelineOffset) {
        double localTime = playheadTime - clipTimelineOffset;
        size_t startSampleIdx = static_cast<size_t>(localTime * 44100.0) * 2;

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


    std::lock_guard<std::mutex> analysisLock(analysisMutex);
    double newBass = (bassSum / framesPerBuffer) * 12.0;
    double newMid = (midSum / framesPerBuffer) * 8.0;
    double newHigh = (highSum / framesPerBuffer) * 20.0;

    bassValue = bassValue * 0.8 + std::clamp(newBass, 0.0, 1.0) * 0.2;
    midValue = midValue * 0.8 + std::clamp(newMid, 0.0, 1.0) * 0.2;
    highValue = highValue * 0.8 + std::clamp(newHigh, 0.0, 1.0) * 0.2;
}
