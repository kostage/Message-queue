#pragma once

#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace zodiactest {
    
enum class RetCode : int {
    OK = 0,
    HWM = -1,
    NO_SPACE = -2,
    STOPPED = -3
};

class IMessageQueueEvents 
{
public:
    IMessageQueueEvents() {}
    virtual ~IMessageQueueEvents() {}
    
    virtual void on_start() = 0;
    virtual void on_hwm() = 0;
    virtual void on_lwm() = 0;
    virtual void on_stop() noexcept = 0; // call in destructor
};

template <typename MessageType>
class MessageQueue 
{
    using MessageTypePrior = std::pair<int, MessageType>;

    template<typename Y>
    using Convertible
    = typename std::enable_if<std::is_convertible<Y, MessageType*>::value>::type;

    enum class QueueState : int {
        RUNNING = 0,
        STOPPED
    };

public:
    MessageQueue(int queue_size, int lwm, int hwm);
    ~MessageQueue();
    
    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;
    MessageQueue(MessageQueue&&) = default;
    MessageQueue& operator=(MessageQueue&&) = default;

    RetCode
    put(const MessageType & message,
        int priority);

/* overdesign
   RetCode
   put(MessageType && message,
   int priority);

   template<class Y, typename = Convertible<Y*>>
   RetCode
   put(Y && message,
   int priority)
   {
   return put(std::forward<Y>(message), priority, blocking);
   }
*/
    RetCode
    get(MessageType * message);

    void
    set_events(std::shared_ptr<IMessageQueueEvents> events);

    void stop();
    void run();
    int size() const noexcept;
    
private:
    static bool comparePrior(int lhs, int rhs) 
    {
        return lhs < rhs;
    }
    void _notifyReaders() const noexcept;
    void _notifyWriters() const noexcept;
    
private:
    int _queue_size;
    int _lwm;
    int _hwm;
    QueueState _queue_state;
    std::vector<MessageTypePrior> _data;
    std::shared_ptr<IMessageQueueEvents> _events;
    mutable std::mutex _data_mtx;
    mutable std::condition_variable _rd_notify;
    mutable std::condition_variable _wr_notify;
};

template<typename MessageType>
MessageQueue<MessageType>::MessageQueue(int queue_size,
                                        int lwm, int hwm) :
    _queue_state{QueueState::STOPPED}
{
    assert(queue_size > 0);
    _queue_size = queue_size;

    assert(lwm >= 0 && lwm < _queue_size);
    assert(hwm >= 0 && hwm <= _queue_size);
    assert(lwm  < hwm);

    _lwm = lwm;
    _hwm = hwm;
  
    _data.reserve(_queue_size);
}

template<typename MessageType>
MessageQueue<MessageType>::~MessageQueue()
{
    stop();
}

template<typename MessageType>
RetCode
MessageQueue<MessageType>::put(const MessageType & message,
                               int priority)
{
    std::unique_lock<std::mutex> lock(_data_mtx);
    
    if (_queue_state == QueueState::STOPPED) {
        return RetCode::STOPPED;
    }
    
    if (_events && static_cast<int>(_data.size()) >= _hwm)
    {
        /* hwm condition and events mechanism active */
        /* increment use count since need to access
           _events in unlocked context */
        auto events = _events;
        lock.unlock();
        events->on_hwm();
        lock.lock();
        /* after unlock/lock */
        /* anything could happen - recheck */
        if (_queue_state == QueueState::STOPPED) {
            return RetCode::STOPPED;
        }
        /* here I intentionally don't check
           that HWM condition is not true
           because that would inject high level logic 
           into queue, assuming on_hwm() is blocking all writers
           *
           HENCE - writers have ability to race
           for writing higher than HWM level*/
    }
    if (static_cast<int>(_data.size()) == _queue_size) {
        /* no free space -
           wait writers notification */
        _wr_notify.wait(lock, [this] {
                return _queue_state == QueueState::STOPPED ||
                    static_cast<int>(_data.size()) != _queue_size;
            });
        /* anything could happen - recheck */
        if (_queue_state == QueueState::STOPPED) {
            return RetCode::STOPPED;
        }
    }
    
    _data.push_back(std::make_pair(priority, message));
    std::push_heap(_data.begin(),
                   _data.end(),
                   [](const MessageTypePrior & lhs, const MessageTypePrior & rhs) {
                       return comparePrior(lhs.first, rhs.first);
                   });
    _notifyReaders();
    return RetCode::OK;
}

template<typename MessageType>
RetCode
MessageQueue<MessageType>::get(MessageType * message)
{
    std::unique_lock<std::mutex> lock(_data_mtx);
    
    if (_queue_state == QueueState::STOPPED) {
        return RetCode::STOPPED;
    }
    
    if (_data.size() == 0) {
        /* emty queue - wait notififcation from writers */
        _rd_notify.wait(lock, [this] {
                return _queue_state == QueueState::STOPPED ||
                    !_data.empty();
            });
    }

    /* anything could happen - recheck */
    if (_queue_state == QueueState::STOPPED) {
        return RetCode::STOPPED;
    }
    
    std::pop_heap(_data.begin(),
                  _data.end(),
                  [](const MessageTypePrior & lhs, const MessageTypePrior & rhs) {
                      return comparePrior(lhs.first, rhs.first);
                  });

    *message = std::move(_data.back().second);
    _data.pop_back();

    if (_events &&
        static_cast<int>(_data.size()) == _lwm)
    {
        /* increment use count since need to access
           _events in unlocked context */
        auto events = _events;
        lock.unlock();
        events->on_lwm();
    }
    _notifyWriters();
    return RetCode::OK;
}

template<typename MessageType>
void MessageQueue<MessageType>::run()
{
    std::unique_lock<std::mutex> lock(_data_mtx);
    _queue_state = QueueState::RUNNING; // make atomic?
    if (_events) {
        /* increment use count since need to access
           _events in unlocked context */
        auto events = _events;
        lock.unlock();
        events->on_start();
    }
    _notifyWriters();
    _notifyReaders();
}

template<typename MessageType>
void MessageQueue<MessageType>::stop()
{
    std::unique_lock<std::mutex> lock(_data_mtx);
    _queue_state = QueueState::STOPPED;  // make atomic?
    if (_events) {
        /* increment use count since need to access
           _events in unlocked context */
        auto events = _events;
        lock.unlock();
        events->on_stop();
    }
    _notifyWriters();
    _notifyReaders();
}

template<typename MessageType>
void MessageQueue<MessageType>::_notifyReaders() const noexcept
{
    _rd_notify.notify_all();
}

template<typename MessageType>
void MessageQueue<MessageType>::_notifyWriters() const noexcept
{
    _wr_notify.notify_all();
}

template<typename MessageType>
void MessageQueue<MessageType>::set_events(
    std::shared_ptr<IMessageQueueEvents> events)
{
    std::unique_lock<std::mutex> lock(_data_mtx);
    _events = events;
}

template<typename MessageType>
int MessageQueue<MessageType>::size() const noexcept
{
    std::unique_lock<std::mutex> lock(_data_mtx);
    return static_cast<int>(_data.size());
}

} // namespace zodiactest 
