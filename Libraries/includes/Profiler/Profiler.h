#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <functional>
#include <queue>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include "RingBuffer.h"

#define MAX_QUERIES 12

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


    // TODO : Profile GPU

    static std::chrono::nanoseconds GetLastTime(std::string name) { return getTimers()[name].get(0); }
    static std::chrono::nanoseconds GetAverageTime(std::string name) { return getTimers()[name].getAverage(); }
    static std::chrono::nanoseconds GetMaxTime(std::string name) { return getTimers()[name].getMax(); }
    static std::chrono::nanoseconds GetMinTime(std::string name) { return getTimers()[name].getMin(); }

private:
    static void ProcessQueries(std::string name) {
        while (!getQueries()[name].empty()) {
            GLint available;
            std::array<GLuint, 2> q = getQueries()[name].front();
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
                break;
            }
        }
    }

    static void AddTime(std::string name, std::chrono::nanoseconds time) { 
        getTimers()[name].push(time); 
    }

    static void AddQuery(std::string name, std::array<GLuint, 2> queries) { 
        if (getQueries()[name].size() >= MAX_QUERIES) {
            glDeleteQueries(2, getQueries()[name].front().data());
            getQueries()[name].pop();
        }
        getQueries()[name].push(queries); 
    }

    static void PopQuery(std::string name) { 
        getQueries()[name].pop();
        if (getQueries()[name].empty()) {
            getQueries().erase(name);
        }
    }





    static std::unordered_map<std::string, RingBuffer<std::chrono::nanoseconds>>& getTimers() {
        static std::unordered_map<std::string, RingBuffer<std::chrono::nanoseconds>> timers;
        return timers;
    }
    static std::unordered_map<std::string, std::queue<std::array<GLuint, 2>>>& getQueries() {
        static std::unordered_map<std::string, std::queue<std::array<GLuint, 2>>> queries;
        return queries;
    }
};
