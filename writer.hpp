
#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

#include "messagequeue.hpp"

namespace zodiactest {

class Writer {
    using Queue = MessageQueue<std::string>;
public:
    Writer(int priority,
           const std::string& name, 
           std::shared_ptr<Queue> queue_sp);
    
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    Writer(Writer&&) = default;
    Writer& operator=(Writer&&) = default;

    ~Writer();

    void run();
    void mainFunc();

    static void wakeAll();
    static void suspendAll();

    static std::atomic<int> gmsg_num;

private:
    enum class WriterState : int {
        SUSPENDED = 0,
        RUNNING
    };
    
    static WriterState _state;
    static std::mutex _g_mtx;
    static std::condition_variable _g_notify;

    const int _priority;
    const std::string _name;
    std::shared_ptr<Queue> _queue_sp;
    std::thread _thread;
};

} // namespace zodiactest 
