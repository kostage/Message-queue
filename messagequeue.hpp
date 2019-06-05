#pragma once

#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <map>
#include <queue>
#include <memory>
#include <mutex>

namespace zodiactest {
    
enum class RetCode : int {
    OK = 0,
    HWM = -1,
    NO_SPACE = -2,
    STOPPED = -3
};

class IMessageQueueEvents {
public:
    IMessageQueueEvents() {}
    virtual ~IMessageQueueEvents() {}
    
    virtual void on_start() = 0;
    virtual void on_hwm() = 0;
    virtual void on_lwm() = 0;
    virtual void on_stop() = 0;
};

template <typename MessageType>
class MessageQueue {
public:
    MessageQueue(int queue_size, int lwm, int hwm);
    
    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;
    MessageQueue(MessageQueue&&) = default;
    MessageQueue& operator=(MessageQueue&&) = default;

    ~MessageQueue();

    RetCode put(const MessageType& message, int priority);
    RetCode get(MessageType* message);
    void setEvents(std::shared_ptr<IMessageQueueEvents> events);

    void stop();
    void run();
    int size() const noexcept;
    
private:
    using MessageTypePrior = std::pair<int, MessageType>;
    enum class QueueState : int {
        RUNNING = 0,
        STOPPED
    };

    void _notifyReaders() const noexcept;
    void _notifyWriters() const noexcept;
    void _push(const MessageType& message, int priority);
    void _pop(MessageType* message);
    
    inline int _size() const noexcept {
        return _current_size;
    }
    
    int _current_size;
    int _queue_size;
    int _lwm;
    int _hwm;
    QueueState _queue_state;
    bool _hwm_flag; // solves multiple LWM notification problem
    /* would be effective when number of priorities is not high */
    std::map<int, std::queue<MessageType>> _map_of_queue;
    std::shared_ptr<IMessageQueueEvents> _events;
    mutable std::mutex _mtx;
    mutable std::condition_variable _rd_notify;
    mutable std::condition_variable _wr_notify;
};

template<typename MessageType>
MessageQueue<MessageType>::MessageQueue(int queue_size,
                                        int lwm, int hwm)
    : _current_size{0},
      _queue_state{QueueState::STOPPED},
      _hwm_flag{false} {
    assert(queue_size > 0);
    _queue_size = queue_size;

    assert(lwm >= 0 && lwm < _queue_size);
    assert(hwm >= 0 && hwm <= _queue_size);
    assert(lwm  < hwm);
    _lwm = lwm;
    _hwm = hwm;
}

template<typename MessageType>
MessageQueue<MessageType>::~MessageQueue() {
    stop();
}

template<typename MessageType>
RetCode MessageQueue<MessageType>::put(const MessageType& message,
                                       int priority) {
    std::unique_lock<std::mutex> lock(_mtx);
    
    if (_queue_state == QueueState::STOPPED) {
        return RetCode::STOPPED;
    }
    
    /* hwm condition and events mechanism active */
    if (_events && _size() >= _hwm) {
        _hwm_flag = true;
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
    if (_size() == _queue_size) {
        /* no free space -
           wait writers notification */
        _wr_notify.wait(lock, [this] {
                return _queue_state == QueueState::STOPPED ||
                    _size() != _queue_size;
            });
        /* anything could happen - recheck */
        if (_queue_state == QueueState::STOPPED) {
            return RetCode::STOPPED;
        }
    }

    _push(message, priority);
    
    _notifyReaders();
    return RetCode::OK;
}

template<typename MessageType>
RetCode MessageQueue<MessageType>::get(MessageType* message) {
    std::unique_lock<std::mutex> lock(_mtx);
    
    if (_queue_state == QueueState::STOPPED) {
        return RetCode::STOPPED;
    }
    
    if (_size() == 0) {
        /* emty queue - wait notififcation from writers */
        _rd_notify.wait(lock, [this] {
                return _queue_state == QueueState::STOPPED ||
                    _size() != 0;
            });
    }

    /* anything could happen - recheck */
    if (_queue_state == QueueState::STOPPED) {
        return RetCode::STOPPED;
    }
    
    _pop(message);
    
    if (_events && _hwm_flag && _size() == _lwm) {
        _hwm_flag = false;
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
void MessageQueue<MessageType>::run() {
    std::unique_lock<std::mutex> lock(_mtx);
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
void MessageQueue<MessageType>::stop() {
    std::unique_lock<std::mutex> lock(_mtx);
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
void MessageQueue<MessageType>::_notifyReaders() const noexcept {
    _rd_notify.notify_all();
}

template<typename MessageType>
void MessageQueue<MessageType>::_notifyWriters() const noexcept {
    _wr_notify.notify_all();
}

template<typename MessageType>
void MessageQueue<MessageType>::setEvents(
    std::shared_ptr<IMessageQueueEvents> events) {
    std::unique_lock<std::mutex> lock(_mtx);
    _events = events;
}

template<typename MessageType>
int MessageQueue<MessageType>::size() const noexcept {
    std::unique_lock<std::mutex> lock(_mtx);
    return _size();
}

template<typename MessageType>
void MessageQueue<MessageType>::_push(const MessageType& message, int priority) {
    auto& queue_ref = _map_of_queue[priority];
    queue_ref.push(message);
    ++_current_size;
}

template<typename MessageType>
void MessageQueue<MessageType>::_pop(MessageType* message) {
    assert(message != nullptr);
    auto max_priority_pair_it = _map_of_queue.end();
    /* max element of map is at the end */
    --max_priority_pair_it;
    auto& max_priority_queue = max_priority_pair_it->second;
    
    *message = std::move(max_priority_queue.front());
    max_priority_queue.pop();

    /* if queue's become empty get rid of unneeded map node */
    if (!max_priority_queue.size())
        _map_of_queue.erase(max_priority_pair_it);
    
    --_current_size;
}

} // namespace zodiactest 
