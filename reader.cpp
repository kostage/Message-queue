
#include "reader.hpp"

#include <cassert>

#include "console.hpp"

namespace zodiactest {

std::atomic<int> Reader::gmsg_num{0};

Reader::Reader(const std::string & name,
               std::shared_ptr<Queue> queue_sp) :
    _name(name),
    _queue_sp(queue_sp)
{
    assert(queue_sp != nullptr);
}

Reader::~Reader()
{
    if (_thread.joinable()) {
        _queue_sp->stop();
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
        ret = _queue_sp->get(&msg);
        if (ret == RetCode::OK)
            _handleMessage(msg);
    } while (ret == RetCode::OK);
    
    logConsole(_name + " detected queue stop\n");
}

void Reader::_handleMessage(const std::string & msg)
{
    ++gmsg_num;
    logConsole(_name + " read >>> " + msg + "\n");
}

} // namespace zodiactest 
