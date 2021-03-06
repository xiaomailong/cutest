﻿#pragma once

#include <cppunit/cppunit.h>
#include "cutest/Runnable.h"

class ExampleManualEndTest
  : public CPPUNIT_NS::ManualEndTestFixture
  , public CUTEST_NS::Runnable
{
  CPPUNIT_TEST_SUITE( ExampleManualEndTest );
  {
    CPPUNIT_MANUAL_END_TEST( manual_end_test_after_1s );
    CPPUNIT_MANUAL_END_TEST_WITH_TIMEOUT( auto_end_test_after_1s, 1000 );
  }
  CPPUNIT_TEST_SUITE_END();

public:
  void manual_end_test_after_1s();

  // 实现Runnable::run()
  virtual void run();

  void auto_end_test_after_1s();
};

CPPUNIT_TEST_SUITE_REGISTRATION( ExampleManualEndTest );
