
#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

#include "messagequeue.hpp"

namespace ZodiacTest {

class Writer
{
    enum class WriterState : int 
    {
        SUSPENDED = 0,
        RUNNING
    };
    
    using Queue = MessageQueue<std::string>;
public:
    Writer(int priority,
           const std::string & name, 
           std::shared_ptr<Queue> queueSP);

    ~Writer();
    
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    Writer(Writer&&) = default;
    Writer& operator=(Writer&&) = default;

    void run();
    void mainFunc();

    static void wakeAll();
    static void suspendAll();

    static std::atomic<size_t> gmsgNum;

private:
    static WriterState _state;
    static std::mutex _gMtx;
    static std::condition_variable _gNotify;
    static std::atomic<size_t> _gmsgNum;

    const int _priority;
    const std::string _name;
    std::shared_ptr<Queue> _queueSP;
    std::thread _thread;
};

} // namespace ZodiacTest 
