#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include "RingBuffer.h"

#define MAX_QUERIES 12
#define BUFFER_SIZE 512
#define SAVE_HISTORY_EVERY_MS 3000
#define QUERY_TIMEOUT_MS 10000

class Profiler
{
private:
    Profiler() = delete;
    ~Profiler() = delete;
    
    // Delete copy and move operations
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;
    Profiler(Profiler&&) = delete;
    Profiler& operator=(Profiler&&) = delete;


public:
    static void Profile(std::string name, std::function<void()> func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        AddTime(name, duration);
    }

    template<typename Func, typename... Args>
    static auto Profile(const std::string& name, Func&& func, Args&&... args) -> std::invoke_result_t<Func, Args...> {
        auto start = std::chrono::high_resolution_clock::now();
        
        if  constexpr (std::is_same_v<std::invoke_result_t<Func, Args...>, void>) {
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            AddTime(name, duration);
        } else {
            auto result = std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            AddTime(name, duration);
            
            return result;
        }
    }

    template<typename Func, typename... Args>
    static std::chrono::nanoseconds Profile(Func&& func, int times, Args&&... args) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < times; i++) {
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        }
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    }

    static void ProfileGPU(std::string name, std::function<void()> func) {
        std::array<GLuint, 2> queries;
        glGenQueries(2, queries.data());

        glQueryCounter(queries[0], GL_TIMESTAMP);
        func();
        glQueryCounter(queries[1], GL_TIMESTAMP);

        AddQuery(name, queries);

        ProcessQueries(name);
    }

    template <typename Func, typename... Args>
    static auto ProfileGPU(const std::string& name, Func&& func, Args&&... args) -> std::invoke_result_t<Func, Args...> {
        std::array<GLuint, 2> queries;
        glGenQueries(2, queries.data());

        glQueryCounter(queries[0], GL_TIMESTAMP);
        if constexpr (std::is_void_v<std::invoke_result_t<Func, Args...>>) {
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
            glQueryCounter(queries[1], GL_TIMESTAMP);

            AddQuery(name, queries);
            ProcessQueries(name);
        } else {
            auto result = std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
            glQueryCounter(queries[1], GL_TIMESTAMP);

            AddQuery(name, queries);
            ProcessQueries(name);
            
            return result;
        }
    }

    static void Process() {
        for (auto& [name, queriesData] : getQueryData()) {
            ProcessQueries(name);
        }

        for (auto& [name, profilerData] : getProfilerData()) {
            auto now = std::chrono::high_resolution_clock::now();
            if (profilerData.history.size() != 0 && 
            (now - profilerData.history.back().time) > std::chrono::milliseconds(SAVE_HISTORY_EVERY_MS)) {
                profilerData.buffer.Clear();
            }
        }
    }




    static std::chrono::nanoseconds GetLastTime(std::string name) {
        return getTimer(name).Get(0);
    }
    static std::chrono::nanoseconds GetAverageTime(std::string name) {
        return getTimer(name).GetAverage();
    }
    static std::chrono::nanoseconds GetMaxTime(std::string name) {
        return getTimer(name).GetMax();
    }
    static std::chrono::nanoseconds GetMinTime(std::string name) {
        return getTimer(name).GetMin();
    }

private:
    static void ProcessQueries(std::string name) {
        while (!getQueries(name).empty()) {
            GLint available;
            std::array<GLuint, 2> q = getQueries(name).front();
            glGetQueryObjectiv(q[1], GL_QUERY_RESULT_AVAILABLE, &available);
            if (available == GL_TRUE) {
                GLuint64 startTime, endTime;
                glGetQueryObjectui64v(q[0], GL_QUERY_RESULT, &startTime);
                glGetQueryObjectui64v(q[1], GL_QUERY_RESULT, &endTime);
                
                std::chrono::nanoseconds GPUTime = std::chrono::nanoseconds((endTime - startTime));

                AddTime(name, GPUTime);
                glDeleteQueries(1, &q[0]);
                glDeleteQueries(1, &q[1]);
                PopQuery(name);
            } else {
                auto now = std::chrono::high_resolution_clock::now();
                if ((now - getQueryData()[name].lastUpdate) >= std::chrono::milliseconds(QUERY_TIMEOUT_MS)) {
                    glDeleteQueries(1, &q[0]);
                    glDeleteQueries(1, &q[1]);
                    PopQuery(name);
                } else {
                    break;
                }
            }
        }
    }

    static void AddTime(std::string name, std::chrono::nanoseconds time) {
        std::lock_guard<std::mutex> lock(getProfilerMutex());
        getTimer(name).Push(time);
        auto now = std::chrono::high_resolution_clock::now();
        if ( getProfilerData()[name].history.size() == 0 || (now - getProfilerData()[name].history.back().time).count() >= SAVE_HISTORY_EVERY_MS) {
            getProfilerData()[name].history.push_back({now, getTimer(name).GetAverage(), getTimer(name).GetMax(), getTimer(name).GetMin()});
        }
    }

    static void AddQuery(std::string name, std::array<GLuint, 2> queries) {
        std::lock_guard<std::mutex> lock(getQueryMutex()); 
        if (getQueries(name).size() >= MAX_QUERIES) {
            glDeleteQueries(2, getQueries(name).front().data());
            getQueries(name).pop();
        }
        getQueries(name).push(queries); 
        getQueryData()[name].lastUpdate = std::chrono::high_resolution_clock::now();
    }

    static void PopQuery(std::string name) { 
        std::lock_guard<std::mutex> lock(getQueryMutex());
        getQueries(name).pop();
        getQueryData()[name].lastUpdate = std::chrono::high_resolution_clock::now();
    }


    struct TimerData
    {
        std::chrono::high_resolution_clock::time_point time;
        std::chrono::nanoseconds avg;
        std::chrono::nanoseconds max;
        std::chrono::nanoseconds min;
    };

    struct ProfilerData
    {
        RingBuffer<std::chrono::nanoseconds> buffer{BUFFER_SIZE};
        std::vector<TimerData> history;
    };

    struct QueryData
    {
        std::queue<std::array<GLuint, 2>> queries;
        std::chrono::high_resolution_clock::time_point lastUpdate;
    };

    static std::unordered_map<std::string, ProfilerData>& getProfilerData() {
        static std::unordered_map<std::string, ProfilerData> profilerData;
        return profilerData;
    }

    static std::unordered_map<std::string, QueryData>& getQueryData() {
        static std::unordered_map<std::string, QueryData> queryData;
        return queryData;
    }

    static RingBuffer<std::chrono::nanoseconds>& getTimer(std::string name) {
        return getProfilerData()[name].buffer;
    }

    static std::queue<std::array<GLuint, 2>>& getQueries(std::string name) {
        return getQueryData()[name].queries;
    }


    static std::mutex& getProfilerMutex() {
        static std::mutex mutex;
        return mutex;
    }

    static std::mutex& getQueryMutex() {
        static std::mutex mutex;
        return mutex;
    }
};
