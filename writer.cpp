
#include "writer.hpp"

#include <cassert>

#include "console.hpp"

namespace zodiactest {

typename Writer::WriterState Writer::_state = WriterState::SUSPENDED;

std::mutex Writer::_g_mtx;

std::condition_variable Writer::_g_notify;

std::atomic<int> Writer::gmsg_num{0};

Writer::Writer(int priority,
               const std::string & name, 
               std::shared_ptr<Queue> queue_sp) :
    _priority{priority},
    _name(name),
    _queue_sp(queue_sp)
{
    assert(queue_sp != nullptr);
}

Writer::~Writer()
{
    if (_thread.joinable()) {
        _queue_sp->stop();
        _thread.join();
    }
}

void Writer::run()
{
    assert(!_thread.joinable());
    _state = WriterState::RUNNING;
    _thread = std::thread(&Writer::mainFunc, this);
}

void Writer::mainFunc()
{
    auto prior = _priority;
    int localMsgNum = 0;
    while (true)
    {
        auto msg = _name + " string #" + std::to_string(localMsgNum++);
        RetCode ret;

        ret = _queue_sp->put(msg, prior);
        
        if (ret == RetCode::OK) {
            ++gmsg_num;
            logConsole(msg + "\n");
        } else if (ret == RetCode::STOPPED) {
            break;
        }
    }
    logConsole(_name + " detected queue stop\n");
}

void Writer::wakeAll()
{
    std::unique_lock<std::mutex> lock(_g_mtx);
    _state = WriterState::RUNNING;
    _g_notify.notify_all();
}

void Writer::suspendAll()
{
    std::unique_lock<std::mutex> lock(_g_mtx);
    _state = WriterState::SUSPENDED;
    _g_notify.wait(lock, []() {
            return _state == WriterState::RUNNING;
        });
}

} // namespace zodiactest 
