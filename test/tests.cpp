
#include "../reader.hpp"
#include "../writer.hpp"
#include "../messagequeue.hpp"

#include "gtest/gtest.h"

using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Test;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::TestPartResult;
using ::testing::UnitTest;

using namespace zodiactest;

namespace {

class QueueTestPriority : public testing::Test {
    
    static constexpr int QUEUE_SIZE = 10;
public:
    QueueTestPriority() : _q(QUEUE_SIZE, 0, QUEUE_SIZE) 
    {}

protected:
    void SetUp() override {
        _q.run();
        int priority_incr = 0;
        for(int i = 0; i != QUEUE_SIZE; i++) {
            _q.put(i, priority_incr++);
        }
    }
    /* Test elements are popped from queue
       with priority taken into account */
    void TestPriority() {
        int prevVal = 100500;
        for(int i = 0; i != QUEUE_SIZE; i++) {
            int val;
            /* elements supposed to be popped
               in descending order */
            ASSERT_EQ(_q.get(&val), RetCode::OK);
            ASSERT_LT(val, prevVal);
            prevVal = val;
        }
        
        _q.stop();
    }

    MessageQueue<int> _q;
};

TEST_F(QueueTestPriority, PriorityTest) {
  TestPriority();
}

class QueueTestThreadSafety : public testing::Test {
    
    static constexpr int QUEUE_SIZE = 10;
public:
    QueueTestThreadSafety() : _q(QUEUE_SIZE, 0, QUEUE_SIZE) 
    {}

protected:
    void SetUp() override {
    }
    /* Test elements are popped from queue
       with priority taken into account */
    void TestThreadSafety() {
        _q.stop();
    }

    MessageQueue<int> _q;
};

}  // namespace

int main(int argc, char **argv) {
  InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
