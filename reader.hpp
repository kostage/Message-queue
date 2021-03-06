
#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include "messagequeue.hpp"

namespace zodiactest {

class Reader {
    using Queue = MessageQueue<std::string>;
public:
    Reader(const std::string & name,
           std::shared_ptr<Queue> queue_sp);

    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader(Reader&&) = default;
    Reader& operator=(Reader&&) = default;

    ~Reader();
    
    void run();
    void mainFunc();
    
    static std::atomic<int> gmsg_num;

protected:
    void _handleMessage(const std::string& msg);

private:
    const std::string _name;
    std::shared_ptr<Queue> _queue_sp;
    std::thread _thread;
};

} // namespace zodiactest 
