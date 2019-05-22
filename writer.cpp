
#include "writer.hpp"

#include <cassert>
#include <iostream>

typename Writer::WriterState Writer::_state = WriterState::SUSPENDED;

std::mutex Writer::_gMtx;

std::condition_variable Writer::_gNotify;

std::atomic<size_t> Writer::gmsgNum{0};

Writer::Writer(int priority,
               const std::string & name, 
               std::shared_ptr<Queue> queueSP) :
    _priority{priority},
    _name(name),
    _queueSP(queueSP)
{
    assert(queueSP != nullptr);
}

Writer::~Writer()
{
    if (_thread.joinable()) {
        _queueSP->stop();
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
    size_t localMsgNum = 0;
    while (true)
    {
        auto msg = _name + " string #" + std::to_string(localMsgNum++);
        RetCode ret;
        do
        {
            ret = _queueSP->put(msg, prior);
            if (ret == RetCode::OK) {
                ++gmsgNum;
                std::clog << (msg + "\n");
            } else if (ret == RetCode::STOPPED) {
                break;
            }
        } while (ret == RetCode::HWM ||
                 ret == RetCode::NO_SPACE);
        
        if (ret == RetCode::STOPPED) {
            break;
        }
    }
    std::clog << (_name + " detected queue stop\n");
}

void Writer::wakeAll()
{
    std::unique_lock<std::mutex> lock(_gMtx);
    _state = WriterState::RUNNING;
    _gNotify.notify_all();
}

void Writer::suspendAll()
{
    std::unique_lock<std::mutex> lock(_gMtx);
    _state = WriterState::SUSPENDED;
    _gNotify.wait(lock, []() {
            return _state == WriterState::RUNNING;
        });
}
