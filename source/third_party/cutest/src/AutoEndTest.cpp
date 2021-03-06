﻿#include "AutoEndTest.h"

#include "cutest/ExplicitEndTest.h"
#include "cutest/Helper.h"
#include "cutest/Runner.h"

#include "gtest/gtest.h"

CUTEST_NS_BEGIN

TestTimeoutCounter::TestTimeoutCounter(ExplicitEndTest* test)
    : test(test)
    , callback(NULL)
    , timeout_ms(0)
    , start_ms(0) {}

void
TestTimeoutCounter::start(unsigned int timeout_ms_in, Callback* callback_in) {
    if (timeout_ms_in) {
        this->callback = callback_in;
        this->timeout_ms = timeout_ms_in;
        this->start_ms = CUTEST_NS::tickCount64();
        Runner::instance()->delayRunOnMainThread(timeout_ms_in, this, false);
    } else {
        // 超时时间为0表示无限等待，这种Test不需要启动Timer
        delete this;
    }
}

void
TestTimeoutCounter::run() {
    // 计算ExplicitEndTest当前的执行时长
    unsigned int elapsed_ms = (unsigned int)(CUTEST_NS::tickCount64() - this->start_ms);
    if (elapsed_ms < this->timeout_ms) {
        /*
            超时补偿逻辑：
            Timer存在误差，比如Runner::instance()->delayRunOnMainThread(1000, ...)指定的是1000ms，
            但有可能在980ms左右就调用TestTimeoutCounter::run()了，为了避免因误差导致ExplicitEndTest被提前结束，
            此处增加一个100ms的补偿逻辑。
        */
        Runner::instance()->delayRunOnMainThread(100, this, false);
    } else {
        this->callback->onTimeout(this->test, this);
        delete this;
    }
}

void
TestTimeoutCounter::addFailure() {
    // 计算ExplicitEndTest当前的执行时长
    unsigned int elapsed_ms = (unsigned int)(CUTEST_NS::tickCount64() - this->start_ms);

    if (elapsed_ms < this->timeout_ms) {
        // ExplicitEndTest还没超时就被自动结束了？需要Review AutoEndTest的设计。
        EXPECT_GE(elapsed_ms, this->timeout_ms) << "AutoEndTest early end the TestCase!";
    } else if (Runner::instance()->treatTimeoutAsError()) {
        EXPECT_LE(elapsed_ms, this->timeout_ms) << "ExplicitEndTest Timeout!";
    }
}

AutoEndTest::AutoEndTest()
    : test(NULL)
    , counter(NULL) {}

void
AutoEndTest::check(ExplicitEndTest* test, unsigned int timeout_ms) {
    if (test && timeout_ms) {
        this->test = test;
        this->counter = new TestTimeoutCounter(test);
        this->counter->start(timeout_ms, this);
    } else {
        this->test = NULL;
        this->counter = NULL;
    }
}

void
AutoEndTest::cancel() {
    this->test = NULL;
    this->counter = NULL;
}

void
AutoEndTest::onTimeout(ExplicitEndTest* test, TestTimeoutCounter* counter) {
    if (this->test == test && this->counter == counter) {
        counter->addFailure();
        this->test->endTest();
    }
}

CUTEST_NS_END
