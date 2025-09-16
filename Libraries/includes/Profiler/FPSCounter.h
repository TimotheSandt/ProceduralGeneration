#pragma once

#include <thread>
#include <chrono>
#include <atomic>

#include "RingBuffer.h"

// Platform-specific includes for high-resolution sleep
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <time.h>
#include <unistd.h>
#endif

class FPSCounter
{
public:
    FPSCounter();
    ~FPSCounter();
    void Destroy() noexcept;

    void newFrame(unsigned int maxFPS);
    
    // High-precision timing control
    void setFrameRateLimitingMode(int mode); // 0=none, 1=hybrid, 2=adaptive, 3=vsync-like
    void setSpinThreshold(std::chrono::nanoseconds threshold);
    
    // Current frame metrics
    double getFPS() const noexcept { return this->fps.load(std::memory_order_relaxed); }
    int getFrame() const noexcept { return this->frame.load(std::memory_order_relaxed); }
    
    // Timing accessors
    std::chrono::high_resolution_clock::time_point getLastTime() const noexcept { return this->lastTime; }
    std::chrono::high_resolution_clock::time_point getTime() const noexcept { return std::chrono::high_resolution_clock::now(); }
    
    // Elapsed time accessors
    std::chrono::nanoseconds getElapseTime() const noexcept { return this->elapseTime; }
    double getElapseTimeInSeconds() const noexcept { return this->elapseTime.count() * 1e-9; }
    double getElapseTimeInMilliseconds() const noexcept { return this->elapseTime.count() * 1e-6; }
    double getElapseTimeInMicroseconds() const noexcept { return this->elapseTime.count() * 1e-3; }
    double getElapseTimeInNanoseconds() const noexcept { return this->elapseTime.count(); }

    // Statistics
    void updateStat();
    
    double getAverageFPS() const noexcept { return this->avgFps; }
    double getMaxFPS() const noexcept { return this->maxFps; }
    double getMinFPS() const noexcept { return this->minFps; }

    double getAverageElapseTime() const noexcept { return this->avgElapseTimens; }
    double getAverageElapseTimeInSeconds() const noexcept { return this->avgElapseTimens * 1e-9; }
    double getAverageElapseTimeInMilliseconds() const noexcept { return this->avgElapseTimens * 1e-6; }
    double getAverageElapseTimeInMicroseconds() const noexcept { return this->avgElapseTimens * 1e-3; }
    double getAverageElapseTimeInNanoseconds() const noexcept { return this->avgElapseTimens; }

    double getMaxElapseTime() const noexcept { return this->maxElapseTimens; }
    double getMaxElapseTimeInSeconds() const noexcept { return this->maxElapseTimens * 1e-9; }
    double getMaxElapseTimeInMilliseconds() const noexcept { return this->maxElapseTimens * 1e-6; }
    double getMaxElapseTimeInMicroseconds() const noexcept { return this->maxElapseTimens * 1e-3; }
    double getMaxElapseTimeInNanoseconds() const noexcept { return this->maxElapseTimens; }

    double getMinElapseTime() const noexcept { return this->minElapseTimens; }
    double getMinElapseTimeInSeconds() const noexcept { return this->minElapseTimens * 1e-9; }
    double getMinElapseTimeInMilliseconds() const noexcept { return this->minElapseTimens * 1e-6; }
    double getMinElapseTimeInMicroseconds() const noexcept { return this->minElapseTimens * 1e-3; }
    double getMinElapseTimeInNanoseconds() const noexcept { return this->minElapseTimens; }

    // Performance metrics
    double getSleepAccuracy() const noexcept { return this->sleepAccuracy; }
    int64_t getTotalFramesDropped() const noexcept { return this->framesDropped; }

private:
    // Frame rate limiting strategies
    void hybridSleep(std::chrono::nanoseconds sleepTime);
    void adaptiveSleep(std::chrono::nanoseconds sleepTime);
    void vsyncLikePacing(unsigned int maxFPS);
    void platformOptimizedSleep(std::chrono::nanoseconds sleepTime);
    
    // Helper methods
    void updateBuffers();
    void initializePlatformTimer();
    void cleanupPlatformTimer();
    
    // Core timing data (atomic for thread safety)
    std::atomic<double> fps{0.0};
    std::atomic<int> frame{0};
    std::chrono::nanoseconds elapseTime{0};
    std::chrono::high_resolution_clock::time_point lastTime;
    std::chrono::high_resolution_clock::time_point frameStartTime;
    
    // Frame rate limiting configuration
    int frameRateLimitingMode{1}; // Default to hybrid
    std::chrono::nanoseconds spinThreshold{500000}; // 0.5ms default
    std::chrono::nanoseconds targetFrameTime{0};
    std::chrono::high_resolution_clock::time_point nextFrameTime;
    
    // Adaptive sleep learning
    std::chrono::nanoseconds sleepOffset{0};
    int adaptiveCounter{0};
    double sleepAccuracy{0.0};
    std::atomic<int64_t> framesDropped{0};
    
    // Statistics
    double avgFps{0.0};
    double maxFps{0.0};
    double minFps{0.0};
    double avgElapseTimens{0.0};
    double maxElapseTimens{0.0};
    double minElapseTimens{0.0};

    // Ring buffers for statistics
    static const int BufferSize = 30;
    RingBuffer<double> fpsBuffer;
    RingBuffer<double> elapseTimeBuffer;
    
    // Platform-specific data
#ifdef _WIN32
    bool timerInitialized{false};
#endif
};