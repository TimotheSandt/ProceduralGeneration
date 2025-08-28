#include "FPSCounter.h"
#include <algorithm>
#include <iostream>

FPSCounter::FPSCounter() :
    lastTime(std::chrono::high_resolution_clock::now()),
    frameStartTime(std::chrono::high_resolution_clock::now()),
    nextFrameTime(std::chrono::high_resolution_clock::now())
{
    this->fpsBuffer.init(this->BufferSize);
    this->elapseTimeBuffer.init(this->BufferSize);
    this->initializePlatformTimer();
}

FPSCounter::~FPSCounter() {
    this->Destroy();
}

void FPSCounter::Destroy() {
    this->cleanupPlatformTimer();
    this->fpsBuffer.destroy();
    this->elapseTimeBuffer.destroy();
}

void FPSCounter::newFrame(unsigned int maxFPS) {
    this->frame.fetch_add(1, std::memory_order_relaxed);
    this->frameStartTime = this->getTime();
    
    // Calculate elapsed time from previous frame
    this->elapseTime = this->frameStartTime - this->lastTime;
    
    // Frame rate limiting
    if (maxFPS > 0) {
        this->targetFrameTime = std::chrono::nanoseconds(1000000000ULL / maxFPS);
        
        switch (this->frameRateLimitingMode) {
            case 0: // No limiting
                break;
            case 1: // Hybrid sleep (default)
                {
                    auto sleepTime = this->targetFrameTime - this->elapseTime;
                    if (sleepTime.count() > 0) {
                        this->hybridSleep(sleepTime);
                    }
                }
                break;
            case 2: // Adaptive sleep
                {
                    auto sleepTime = this->targetFrameTime - this->elapseTime;
                    if (sleepTime.count() > 0) {
                        this->adaptiveSleep(sleepTime);
                    }
                }
                break;
            case 3: // VSync-like pacing
                this->vsyncLikePacing(maxFPS);
                break;
            default:
                // Fallback to hybrid
                {
                    auto sleepTime = this->targetFrameTime - this->elapseTime;
                    if (sleepTime.count() > 0) {
                        this->hybridSleep(sleepTime);
                    }
                }
                break;
        }
    }
    
    // Calculate final timing metrics
    auto currentTime = this->getTime();
    auto totalElapsed = currentTime - this->lastTime;
    
    // Calculate FPS with safety check
    double newFps = (totalElapsed.count() > 0) ? 1.0e9 / totalElapsed.count() : 0.0;
    this->fps.store(newFps, std::memory_order_relaxed);
    
    // Check for dropped frames
    if (maxFPS > 0 && totalElapsed > this->targetFrameTime * 1.5) {
        this->framesDropped.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Update timing
    // this->elapseTime = totalElapsed;
    this->lastTime = currentTime;
    
    // Update statistics buffers
    this->updateBuffers();
}

void FPSCounter::hybridSleep(std::chrono::nanoseconds sleepTime) {
    // Most accurate method: combines sleep_for with spin-wait
    
    if (sleepTime <= this->spinThreshold) {
        // For very short waits, just spin-wait with yields
        auto targetTime = this->getTime() + sleepTime;
        while (this->getTime() < targetTime) {
            std::this_thread::yield();
        }
        return;
    }
    
    // For longer waits, sleep for most of the time
    auto sleepDuration = sleepTime - this->spinThreshold;
    
    // Use platform-optimized sleep if available
    this->platformOptimizedSleep(sleepDuration);
    
    // Spin-wait for the remaining time for high precision
    auto targetTime = this->getTime() + this->spinThreshold;
    while (this->getTime() < targetTime) {
        std::this_thread::yield();
    }
}

void FPSCounter::adaptiveSleep(std::chrono::nanoseconds sleepTime) {
    // Self-learning sleep that adapts to system sleep overhead
    
    auto requestedSleep = sleepTime - this->sleepOffset;
    
    if (requestedSleep.count() > 0) {
        auto sleepStart = this->getTime();
        
        if (requestedSleep <= this->spinThreshold) {
            // Spin-wait for short durations
            auto targetTime = sleepStart + requestedSleep;
            while (this->getTime() < targetTime) {
                std::this_thread::yield();
            }
        } else {
            // Use sleep_for for longer durations
            this->platformOptimizedSleep(requestedSleep - this->spinThreshold / 2);
        }
        
        auto sleepEnd = this->getTime();
        auto actualSleepTime = sleepEnd - sleepStart;
        auto sleepError = actualSleepTime - requestedSleep;
        
        // Update sleep offset with exponential moving average
        this->adaptiveCounter++;
        if (this->adaptiveCounter >= 10) {
            this->sleepOffset = std::chrono::duration_cast<std::chrono::nanoseconds>(this->sleepOffset * 0.9 + sleepError * 0.1);
            this->sleepAccuracy = std::abs(sleepError.count()) / static_cast<double>(sleepTime.count());
            this->adaptiveCounter = 0;
        }
    }
    
    // Fine-tune with spin-wait
    auto remainingTime = sleepTime - (this->getTime() - this->frameStartTime + this->elapseTime);
    if (remainingTime.count() > 0 && remainingTime < std::chrono::microseconds(200)) {
        auto targetTime = this->getTime() + remainingTime;
        while (this->getTime() < targetTime) {
            std::this_thread::yield();
        }
    }
}

void FPSCounter::vsyncLikePacing(unsigned int maxFPS) {
    // Maintains steady frame intervals, handles dropped frames gracefully
    
    auto frameInterval = std::chrono::nanoseconds(1000000000ULL / maxFPS);
    this->nextFrameTime += frameInterval;
    
    auto currentTime = this->getTime();
    auto waitTime = this->nextFrameTime - currentTime;
    
    if (waitTime.count() > 0) {
        // We're ahead of schedule, wait
        this->hybridSleep(waitTime);
    } else if (waitTime.count() < -frameInterval.count() * 2) {
        // We're running way behind (more than 2 frames), reset timing
        this->nextFrameTime = currentTime + frameInterval;
        this->framesDropped.fetch_add(1, std::memory_order_relaxed);
    }
    // If we're slightly behind (1-2 frames), just continue without reset
}

void FPSCounter::platformOptimizedSleep(std::chrono::nanoseconds sleepTime) {
#ifdef _WIN32
    // Windows: Use high-resolution timers
    if (sleepTime >= std::chrono::milliseconds(1)) {
        // For longer sleeps, use Sleep() with 1ms resolution
        DWORD sleepMs = static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(sleepTime).count());
        Sleep(sleepMs);
    } else {
        // For sub-millisecond sleeps, use busy-wait with QueryPerformanceCounter
        LARGE_INTEGER frequency, start, current;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start);
        
        LONGLONG targetTicks = start.QuadPart + (sleepTime.count() * frequency.QuadPart) / 1000000000LL;
        
        do {
            std::this_thread::yield();
            QueryPerformanceCounter(&current);
        } while (current.QuadPart < targetTicks);
    }
#elif defined(__linux__)
    // Linux: Use nanosleep for better precision
    if (sleepTime >= std::chrono::microseconds(100)) {
        struct timespec ts;
        ts.tv_sec = sleepTime.count() / 1000000000LL;
        ts.tv_nsec = sleepTime.count() % 1000000000LL;
        nanosleep(&ts, nullptr);
    } else {
        // For very short sleeps, use busy-wait
        auto targetTime = this->getTime() + sleepTime;
        while (this->getTime() < targetTime) {
            std::this_thread::yield();
        }
    }
#else
    // Other platforms: fallback to standard sleep_for
    if (sleepTime >= std::chrono::microseconds(100)) {
        std::this_thread::sleep_for(sleepTime);
    } else {
        auto targetTime = this->getTime() + sleepTime;
        while (this->getTime() < targetTime) {
            std::this_thread::yield();
        }
    }
#endif
}

void FPSCounter::initializePlatformTimer() {
#ifdef _WIN32
    // Set Windows timer resolution to 1ms for better sleep precision
    if (timeBeginPeriod(1) == TIMERR_NOERROR) {
        this->timerInitialized = true;
    }
#endif
}

void FPSCounter::cleanupPlatformTimer() {
#ifdef _WIN32
    if (this->timerInitialized) {
        timeEndPeriod(1);
        this->timerInitialized = false;
    }
#endif
}

void FPSCounter::updateBuffers() {
    double currentFps = this->fps.load(std::memory_order_relaxed);
    this->fpsBuffer.push(currentFps);
    this->elapseTimeBuffer.push(static_cast<double>(this->elapseTime.count()));
}

void FPSCounter::updateStat() {
    this->avgFps = this->fpsBuffer.getAverage();
    this->maxFps = this->fpsBuffer.getMax();
    this->minFps = this->fpsBuffer.getMin();
    
    this->avgElapseTimens = this->elapseTimeBuffer.getAverage();
    this->maxElapseTimens = this->elapseTimeBuffer.getMax();
    this->minElapseTimens = this->elapseTimeBuffer.getMin();
}

// Configuration methods
void FPSCounter::setFrameRateLimitingMode(int mode) {
    this->frameRateLimitingMode = std::clamp(mode, 0, 3);
}

void FPSCounter::setSpinThreshold(std::chrono::nanoseconds threshold) {
    this->spinThreshold = std::max(threshold, std::chrono::nanoseconds(0));
}