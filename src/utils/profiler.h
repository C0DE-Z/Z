#ifndef PROFILER_H
#define PROFILER_H

#include <string>
#include <chrono>
#include <map>
#include <vector>
#include <mutex>
#include <iostream>

// Lightweight profiling system for identifying performance bottlenecks
// Usage: Profiler::instance().mark("event_name");
class Profiler {
public:
    static Profiler& instance() {
        static Profiler prof;
        return prof;
    }

    // Mark a timing checkpoint
    void mark(const std::string& name) {
        auto now = std::chrono::high_resolution_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        marks_[name] = now;
    }

    // Measure elapsed time since mark
    double elapsed(const std::string& name) {
        auto now = std::chrono::high_resolution_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = marks_.find(name);
        if (it == marks_.end()) return 0.0;

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - it->second);
        return duration.count() / 1000.0; // milliseconds
    }

    // Record a sample (e.g., frame time, effect time)
    void sample(const std::string& category, double milliseconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        samples_[category].push_back(milliseconds);

        // Keep only recent samples (last 300 frames = ~10 seconds at 30fps)
        if (samples_[category].size() > 300) {
            samples_[category].erase(samples_[category].begin());
        }
    }

    // Get average of recent samples
    double average(const std::string& category) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = samples_.find(category);
        if (it == samples_.end() || it->second.empty()) return 0.0;

        double sum = 0.0;
        for (double val : it->second) sum += val;
        return sum / it->second.size();
    }

    // Print performance report
    void report() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\n=== Performance Report ===\n";
        for (const auto& [category, samples] : samples_) {
            if (!samples.empty()) {
                double avg = 0.0;
                double min = samples[0], max = samples[0];
                for (double val : samples) {
                    avg += val;
                    min = std::min(min, val);
                    max = std::max(max, val);
                }
                avg /= samples.size();
                std::cout << category << ": " << avg << "ms avg (min: " << min
                          << "ms, max: " << max << "ms, frames: " << samples.size() << ")\n";
            }
        }
        std::cout << "========================\n";
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        marks_.clear();
        samples_.clear();
    }

private:
    Profiler() = default;
    std::mutex mutex_;
    std::map<std::string, std::chrono::high_resolution_clock::time_point> marks_;
    std::map<std::string, std::vector<double>> samples_;
};

#endif // PROFILER_H
