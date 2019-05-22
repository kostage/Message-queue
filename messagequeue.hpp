#pragma once

#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

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
    virtual void on_stop() = 0;
    virtual void on_hwm() = 0;
    virtual void on_lwm() = 0;
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
    enum class QueueFill : int {
        NORMAL = 0,
        LWM
    };

public:
    MessageQueue(int queueSize, int lwm, int hwm);
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
   
private:
    static bool comparePrior(int lhs, int rhs) 
    {
        return lhs < rhs;
    }
    void _notifyReaders() const noexcept;
    void _notifyWriters() const noexcept;
    
private:
    size_t _queueSize;
    size_t _lwm;
    size_t _hwm;
    QueueState _queueState;
    QueueFill _queueFill;
    std::vector<MessageTypePrior> _data;
    std::shared_ptr<IMessageQueueEvents> _events;
    mutable std::mutex _dataMtx;
    mutable std::condition_variable _rdNotify;
    mutable std::condition_variable _wrNotify;
};

template<typename MessageType>
MessageQueue<MessageType>::MessageQueue(int queueSize,
                                        int lwm, int hwm) :
    _queueState{QueueState::STOPPED},
    _queueFill{QueueFill::NORMAL}
{
    assert(queueSize > 0);
    _queueSize = queueSize;

    assert(lwm >= 0 && lwm < static_cast<int>(_queueSize));
    assert(hwm >= 0 && hwm <= static_cast<int>(_queueSize));
    assert(lwm  < hwm);

    _lwm = static_cast<size_t>(lwm);
    _hwm = static_cast<size_t>(hwm);
  
    _data.reserve(_queueSize);
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
    std::unique_lock<std::mutex> lock(_dataMtx);
    
    if (_queueState == QueueState::STOPPED) {
        return RetCode::STOPPED;
    }
    
    if (_events && _data.size() >= _hwm)
    {
        /* hwm condition and events mechanism active */
        /* increment use count since need to access
           _events in unlocked context */
        auto events = _events.get();
        lock.unlock();
        events->on_hwm();
        lock.lock();
        /* after unlock/lock */
        /* anything could happen - recheck */
        if (_queueState == QueueState::STOPPED) {
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
    if (_data.size() == _queueSize) {
        /* no free space but events not set
           fall back to simple blocking put,
           wait writers notification */
        _wrNotify.wait(lock, [this] {
                return _queueState == QueueState::STOPPED ||
                    _data.size() != _queueSize;
            });
        /* anything could happen - recheck */
        if (_queueState == QueueState::STOPPED) {
            return RetCode::STOPPED;
        }
    }
    
    _data.push_back(std::make_pair(priority, message));
    std::push_heap(_data.begin(),
                   _data.end(),
                   [](const auto lhs, const auto rhs) {
                       return comparePrior(lhs.first, rhs.first);
                   });
    _notifyReaders();
    return RetCode::OK;
}

template<typename MessageType>
RetCode
MessageQueue<MessageType>::get(MessageType * message)
{
    std::unique_lock<std::mutex> lock(_dataMtx);
    
    if (_queueState == QueueState::STOPPED) {
        return RetCode::STOPPED;
    }
    
    if (_data.size() == 0) {
        /* emty queue - wait notififcation from writers */
        _rdNotify.wait(lock, [this] {
                return _queueState == QueueState::STOPPED ||
                    !_data.empty();
            });
    }

    /* anything could happen - recheck */
    if (_queueState == QueueState::STOPPED) {
        return RetCode::STOPPED;
    }
    
    std::pop_heap(_data.begin(),
                  _data.end(),
                  [](const auto lhs, const auto rhs) {
                      return comparePrior(lhs.first, rhs.first);
                  });
    *message = std::move(_data.back().second);
    _data.pop_back();

    /* multiple LWM notification problem here
       must be solved*/
    if (_events &&
        _data.size() == _lwm)
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
    std::unique_lock<std::mutex> lock(_dataMtx);
    _queueState = QueueState::RUNNING; // make atomic?
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
    std::unique_lock<std::mutex> lock(_dataMtx);
    _queueState = QueueState::STOPPED;  // make atomic?
    if (_events) {
        /* increment use count since need to access
           _events in unlocked context */
        auto events = _events.get();
        lock.unlock();
        events->on_stop();
    }
    _notifyWriters();
    _notifyReaders();
}

template<typename MessageType>
void MessageQueue<MessageType>::_notifyReaders() const noexcept
{
    _rdNotify.notify_all();
}

template<typename MessageType>
void MessageQueue<MessageType>::_notifyWriters() const noexcept
{
    _wrNotify.notify_all();
}

template<typename MessageType>
void MessageQueue<MessageType>::set_events(
    std::shared_ptr<IMessageQueueEvents> events)
{
    std::unique_lock<std::mutex> lock(_dataMtx);
    _events = events;
}
