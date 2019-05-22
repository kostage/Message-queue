
#include "reader.hpp"

#include <cassert>
#include <iostream>

std::atomic<size_t> Reader::gmsgNum{0};

Reader::Reader(const std::string & name,
               std::shared_ptr<Queue> queueSP) :
    _name(name),
    _queueSP(queueSP)
{
    assert(queueSP != nullptr);
}

Reader::~Reader()
{
    if (_thread.joinable()) {
        _queueSP->stop();
        _thread.join();
    }
}

void Reader::run()
{
    assert(!_thread.joinable());
    _thread = std::thread(&Reader::mainFunc, this);
}

void Reader::mainFunc()
{
    RetCode ret;
    std::string msg;
    do
    {
        ret = _queueSP->get(&msg);
        if (ret == RetCode::OK)
            _handleMessage(msg);
    } while (ret == RetCode::OK);
    
    std::clog << (_name + " detected queue stop\n");
}

void Reader::_handleMessage(const std::string & msg)
{
    ++gmsgNum;
    std::clog << (_name + " read >>> " + msg + "\n");
}
