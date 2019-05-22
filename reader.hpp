
#pragma once

#include <atomic>
#include <memory>
#include <thread>

#include "messagequeue.hpp"

class Reader 
{
    using Queue = MessageQueue<std::string>;
public:
    Reader(const std::string & name,
           std::shared_ptr<Queue> queueSP);

    ~Reader();
    
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    Reader(Reader&&) = default;
    Reader& operator=(Reader&&) = default;

    void run();
    void mainFunc();
    
    static std::atomic<size_t> gmsgNum;

protected:
    void _handleMessage(const std::string & msg);

private:
    const std::string _name;
    std::shared_ptr<Queue> _queueSP;
    std::thread _thread;
};
