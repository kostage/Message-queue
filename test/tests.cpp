
#include "../messagequeue.hpp"
#include "gtest/gtest.h"

#include <atomic>
#include <iostream>
#include <memory>
#include <thread>

using ::testing::Test;
using ::testing::InitGoogleTest;

using namespace zodiactest;

namespace {

class QueueTestPriority : public ::testing::Test {
    
    static constexpr int QUEUE_SIZE = 10;
public:
    QueueTestPriority() : _q(QUEUE_SIZE, 0, QUEUE_SIZE) 
    {}

protected:
    void SetUp() override {
        _q.run();
    }
    /* Test elements are popped from queue
       with priority taken into account */
    void TestPriority() {
        int priority_incr = 0;
        for(int i = 0; i != QUEUE_SIZE; i++) {
            _q.put(i, priority_incr++);
        }
        int prevVal = 100500;
        for(int i = 0; i != QUEUE_SIZE; i++) {
            int val;
            /* elements supposed to be popped
               in reversed order */
            ASSERT_EQ(_q.get(&val), RetCode::OK);
            /* new val must be less than previous */
            ASSERT_LT(val, prevVal);
            prevVal = val;
        }
        
        _q.stop();
    }

    MessageQueue<int> _q;
};
class TestWriter 
{
public:
    TestWriter(MessageQueue<int> * qp) : _qp{qp}
    {}
    
    ~TestWriter()
    {
        stop_flag = true;
        join();
    }
    
    void run()
    {
        _thread = std::thread(&TestWriter::mainFunc, this);
    }

    void join()
    {
        if (_thread.joinable()) {
            _thread.join();
        }
    }
    
    void mainFunc()
    {
        RetCode ret;
        do
        {
            ret = _qp->put(1, 1);
            if (ret == RetCode::OK) {
                ++gmsg_num;
            }
        } while (!stop_flag && ret == RetCode::OK);
    }
    
    static std::atomic_bool stop_flag;
    static std::atomic<int> gmsg_num;
private:
    MessageQueue<int> * _qp;
    std::thread _thread;
};

std::atomic_bool TestWriter::stop_flag{false};
std::atomic<int> TestWriter::gmsg_num{0};


class TestReader 
{
public:
    TestReader(MessageQueue<int> * qp) : _qp{qp}
    {}
    
    ~TestReader()
    {
        stop_flag = false;
        join();
    }
    
    void run()
    {
        _thread = std::thread(&TestReader::mainFunc, this);
    }

    void join()
    {
        if (_thread.joinable()) {
            _thread.join();
        }
    }
    
    void mainFunc()
    {
        RetCode ret;
        do
        {
            int val;
            ret = _qp->get(&val);
            if (ret == RetCode::OK) {
                ++gmsg_num;
            }
        } while (!stop_flag && ret == RetCode::OK);
    }
    
    static std::atomic_bool stop_flag;
    static std::atomic<int> gmsg_num;
private:
    MessageQueue<int> * _qp;
    std::thread _thread;
};

std::atomic_bool TestReader::stop_flag{false};
std::atomic<int> TestReader::gmsg_num{0};

class QueueNopEvents : public IMessageQueueEvents
{
public:
    QueueNopEvents() {}
    ~QueueNopEvents() final {}
    
    void on_start() final {}
    void on_stop() noexcept final {}
    void on_hwm() final {}
    void on_lwm() final {}
};

class QueueTestThreadSafety : public ::testing::Test {
    
    static constexpr int QUEUE_SIZE = 10;
public:
    QueueTestThreadSafety() :
        _q(QUEUE_SIZE, 0, QUEUE_SIZE),
        _tr1(&_q),
        _tr2(&_q),
        _tw1(&_q),
        _tw2(&_q)
    {}

protected:
    void SetUp() override {
        TestReader::stop_flag = false;
        TestReader::gmsg_num = 0;
        TestWriter::stop_flag = false;
        TestWriter::gmsg_num = 0;
    }
    /* Test number of elements pushed
       is equal to popped in mt environment */
    void TestThreadSafety(std::shared_ptr<IMessageQueueEvents> events)
    {
        if (events) {
            _q.set_events(events);
        }
        _q.run();
        _tr1.run();
        _tr2.run();
        _tw1.run();
        _tw2.run();
        std::cout << "Start ~1 second queue rape\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
        TestWriter::stop_flag = true;
        _tw1.join();
        _tw2.join();
        std::cout << "Flush queue\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        _q.stop();
        _tr1.join();
        _tr2.join();
        ASSERT_EQ(TestReader::gmsg_num, TestWriter::gmsg_num);
    }

    MessageQueue<int> _q;
    TestReader _tr1;
    TestReader _tr2;
    TestWriter _tw1;
    TestWriter _tw2;
};

class QueueTestWaterMarks : public ::testing::Test {
    static constexpr int QUEUE_SIZE = 10;
    
    class QueueTestEvents : public IMessageQueueEvents
    {
    public:
        QueueTestEvents(MessageQueue<int> * q,
                        TestReader * tr,
                        TestWriter * tw) :
            _q{q},
            _tr1{tr},
            _tw1{tw} 
        {}
        ~QueueTestEvents() final {}
    
        void on_start() final
        {
            _tw1->run();
            ++start_flag;
        }
        void on_stop() noexcept final
        {
            ++stop_flag;
        }
        
        void on_hwm() final
        {
            TestWriter::stop_flag = true;
            _tr1->run();
            ++hwm_flag;
        }
        
        void on_lwm() final
        {
            TestReader::stop_flag = true;
            _q->stop();
            ++lwm_flag;
        }
    private:
        MessageQueue<int> * _q;
        TestReader * _tr1;
        TestWriter * _tw1;
    };

public:
    QueueTestWaterMarks() :
        _q(QUEUE_SIZE, 1, QUEUE_SIZE - 2),
        _tr1(&_q),
        _tw1(&_q)
    {}

    static std::atomic<int> start_flag;
    static std::atomic<int> stop_flag;
    static std::atomic<int> hwm_flag;
    static std::atomic<int> lwm_flag;

protected:
    void SetUp() override {
        TestReader::stop_flag = false;
        TestReader::gmsg_num = 0;
        TestWriter::stop_flag = false;
        TestWriter::gmsg_num = 0;
        start_flag = 0;
        stop_flag = 0;
        hwm_flag = 0;
        lwm_flag = 0;
    }
    /* Test number of elements pushed
       is equal to popped in mt environment */
    void TestWaterMarks()
    {
        _q.set_events(std::make_shared<QueueTestEvents>(
                          &_q, &_tr1, &_tw1));
        _q.run();
        _tw1.join();
        _tr1.join();
        ASSERT_EQ(start_flag, 1);
        ASSERT_EQ(stop_flag, 1);
        ASSERT_EQ(hwm_flag, 1);
        ASSERT_EQ(lwm_flag, 1);
    }

    MessageQueue<int> _q;
    TestReader _tr1;
    TestWriter _tw1;
};

std::atomic<int> QueueTestWaterMarks::start_flag{0};
std::atomic<int> QueueTestWaterMarks::stop_flag{0};
std::atomic<int> QueueTestWaterMarks::hwm_flag{0};
std::atomic<int> QueueTestWaterMarks::lwm_flag{0};

TEST_F(QueueTestPriority, PriorityTest) {
  TestPriority();
}

TEST_F(QueueTestThreadSafety, MTSafeTestWithoutEvents) {
    TestThreadSafety(nullptr);
}

TEST_F(QueueTestThreadSafety, MTSafeTestWithEvents) {
    TestThreadSafety(std::make_shared<QueueNopEvents>());
}

TEST_F(QueueTestWaterMarks, TestWaterMarkNotifiers) {
    TestWaterMarks();
}

}  // namespace

int main(int argc, char **argv) {
  InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
