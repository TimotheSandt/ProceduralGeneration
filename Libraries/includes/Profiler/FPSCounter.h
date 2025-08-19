#include <thread>
#include <chrono>

#include "RingBuffer.h"

class FPSCounter
{
public:
    FPSCounter();
    void Destroy();

    void newFrame(unsigned int maxFPS);
    
public:
    double getFPS() { return this->fps; };

    int getFrame() { return this->frame; };

    std::chrono::time_point<std::chrono::high_resolution_clock> getLastTime() { return this->lastTime; };
    std::chrono::time_point<std::chrono::high_resolution_clock> getTime() { return std::chrono::high_resolution_clock::now(); };
    
    std::chrono::duration<double, std::nano> getElapseTime() { return this->elapseTime; };
    double getElapseTimeInSeconds() { return this->elapseTime.count() / 1e9; };
    double getElapseTimeInMilliseconds() { return this->elapseTime.count() / 1e6; };
    double getElapseTimeInMicroseconds() { return this->elapseTime.count() / 1e3; };
    double getElapseTimeInNanoseconds() { return this->elapseTime.count(); };

    void UpdateStat();
    
    double getAverageFPS() { return this->avgFps; };
    double getMaxFPS() { return this->maxFps; };
    double getMinFPS() { return this->minFps; };

    double getAverageElapseTime() { return this->avgElapseTimens; };
    double getAverageElapseTimeInSeconds() { return this->avgElapseTimens / 1e9; };
    double getAverageElapseTimeInMilliseconds() { return this->avgElapseTimens / 1e6; };
    double getAverageElapseTimeInMicroseconds() { return this->avgElapseTimens / 1e3; };
    double getAverageElapseTimeInNanoseconds() { return this->avgElapseTimens; };

    double getMaxElapseTime() { return this->maxElapseTimens; };
    double getMaxElapseTimeInSeconds() { return this->maxElapseTimens / 1e9; };
    double getMaxElapseTimeInMilliseconds() { return this->maxElapseTimens / 1e6; };
    double getMaxElapseTimeInMicroseconds() { return this->maxElapseTimens / 1e3; };
    double getMaxElapseTimeInNanoseconds() { return this->maxElapseTimens; };

    double getMinElapseTime() { return this->minElapseTimens; };
    double getMinElapseTimeInSeconds() { return this->minElapseTimens / 1e9; };
    double getMinElapseTimeInMilliseconds() { return this->minElapseTimens / 1e6; };
    double getMinElapseTimeInMicroseconds() { return this->minElapseTimens / 1e3; };
    double getMinElapseTimeInNanoseconds() { return this->minElapseTimens; };


private:
    void updateBuffers();

private:
    // Real Time fps, elapse time and frame
    double fps;
    std::chrono::duration<double, std::nano> elapseTime;
    int frame = 0;
    
    // Time
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;
    
    // Stat
    double avgFps;
    double maxFps;
    double minFps;
    double avgElapseTimens;
    double maxElapseTimens;
    double minElapseTimens;


    int BufferSize = 30;
    RingBuffer<double> fpsBuffer;
    RingBuffer<double> elapseTimeBuffer;
};