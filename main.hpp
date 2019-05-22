
#pragma once

#include <memory>
#include <thread>
#include <vector>

#include "messagequeue.hpp"
#include "reader.hpp"
#include "writer.hpp"

namespace ZodiacTest {

class QueueEvents : public IMessageQueueEvents
{
public:
    QueueEvents() {}
    ~QueueEvents() final {}
    
    void on_start() final;
    void on_stop() noexcept final;
    void on_hwm() final;
    void on_lwm() final;
};

class Main
{
public:
    Main(size_t rnum, size_t wnum);
    ~Main();
    
    void main();
    void stop() noexcept;
    void flush();

private:
    std::shared_ptr<MessageQueue<std::string>> _mqueueSP;
    std::vector<Reader> _readers;
    std::vector<Writer> _writers;
};

} // namespace ZodiacTest 
