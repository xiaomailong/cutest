﻿#include "ProgressListenerManager.h"

#include <cppunit/Test.h>
#include <cppunit/TestFailure.h>

#include "cutest/Runner.h"

CUTEST_NS_BEGIN

ProgressListenerManager::ProgressListenerManager()
  : failure_index( 0 )
{}

void
ProgressListenerManager::add( ProgressListener *listener )
{
  this->listeners.insert( listener );
}

void
ProgressListenerManager::remove( ProgressListener *listener )
{
  this->listeners.erase( listener );
}

class StartTestRunTask : public ProgressListenerManager::TaskBase
{
protected:
  CPPUNIT_NS::Test *test;

public:
  StartTestRunTask( ProgressListenerManager *manager, Event *event, CPPUNIT_NS::Test *test_in )
    : ProgressListenerManager::TaskBase( manager, event )
    , test( test_in )
  {}

  virtual void run()
  {
    this->manager->startTestRunOnMainThread( this->test );
  }
};

void
ProgressListenerManager::startTestRun( CPPUNIT_NS::Test *test, CPPUNIT_NS::TestResult * )
{
  Event *event = Event::createInstance();
  StartTestRunTask *task = new StartTestRunTask( this, event, test );
  Runner::instance()->asyncRunOnMainThread( task, true );
  event->wait();
  event->destroy();
}

void
ProgressListenerManager::startTestRunOnMainThread( CPPUNIT_NS::Test *test )
{
  while ( !this->test_record.empty() )
  {
    this->test_record.pop();
  }
  this->failure_index = 0;

  TestRecord record;
  record.start_ms = Runner::tickCount();
  this->test_record.push( record );

  TestProgressListeners::iterator it = this->listeners.begin();
  while ( it != this->listeners.end() )
  {
    ( *it )->onRunnerStart( test );
    ++it;
  }
}

class EndTestRunTask : public ProgressListenerManager::TaskBase
{
protected:
  CPPUNIT_NS::Test *test;

public:
  EndTestRunTask( ProgressListenerManager *manager, CPPUNIT_NS::Test *test_in )
    : ProgressListenerManager::TaskBase( manager, NULL )
    , test( test_in )
  {
  }

  virtual void run()
  {
    this->manager->endTestRunOnMainThread( this->test );
  }
};

void
ProgressListenerManager::endTestRun( CPPUNIT_NS::Test *test, CPPUNIT_NS::TestResult * )
{
  Runner *runner = Runner::instance();
  EndTestRunTask *task = new EndTestRunTask( this, test );
  runner->asyncRunOnMainThread( task, true );
}

void
ProgressListenerManager::endTestRunOnMainThread( CPPUNIT_NS::Test *test )
{
  TestRecord &record = this->test_record.top();
  unsigned int elapsed_ms = ( unsigned int )( Runner::tickCount() - record.start_ms );
  this->test_record.pop();

  TestProgressListeners::iterator it = this->listeners.begin();
  while ( it != this->listeners.end() )
  {
    ( *it )->onRunnerEnd( test, elapsed_ms );
    ++it;
  }
}

class StartSuiteTask : public ProgressListenerManager::TaskBase
{
protected:
  CPPUNIT_NS::Test *suite;

public:
  StartSuiteTask( ProgressListenerManager *manager, Event *event, CPPUNIT_NS::Test *suite_in )
    : ProgressListenerManager::TaskBase( manager, event )
    , suite( suite_in )
  {}

  virtual void run()
  {
    this->manager->startSuiteOnMainThread( this->suite );
  }
};

void
ProgressListenerManager::startSuite( CPPUNIT_NS::Test *suite )
{
  Event *event = Event::createInstance();
  StartSuiteTask *task = new StartSuiteTask( this, event, suite );
  Runner::instance()->asyncRunOnMainThread( task, true );
  event->wait();
  event->destroy();
}

void
ProgressListenerManager::startSuiteOnMainThread( CPPUNIT_NS::Test *suite )
{
  TestRecord record;
  record.start_ms = Runner::tickCount();
  this->test_record.push( record );

  TestProgressListeners::iterator it = this->listeners.begin();
  while ( it != this->listeners.end() )
  {
    ( *it )->onSuiteStart( suite );
    ++it;
  }
}

class EndSuiteTask : public ProgressListenerManager::TaskBase
{
protected:
  CPPUNIT_NS::Test *suite;

public:
  EndSuiteTask( ProgressListenerManager *manager, Event *event, CPPUNIT_NS::Test *suite_in )
    : ProgressListenerManager::TaskBase( manager, event )
    , suite( suite_in )
  {}

  virtual void run()
  {
    this->manager->endSuiteOnMainThread( this->suite );
  }
};

void
ProgressListenerManager::endSuite( CPPUNIT_NS::Test *suite )
{
  Runner *runner = Runner::instance();
  Event *event = Event::createInstance();
  EndSuiteTask *task = new EndSuiteTask( this, event, suite );
  runner->asyncRunOnMainThread( task, true );
  event->wait();
  event->destroy();
}

void
ProgressListenerManager::endSuiteOnMainThread( CPPUNIT_NS::Test *suite )
{
  TestRecord &record = this->test_record.top();
  unsigned int elapsed_ms = ( unsigned int )( Runner::tickCount() - record.start_ms );
  this->test_record.pop();

  TestProgressListeners::iterator it = this->listeners.begin();
  while ( it != this->listeners.end() )
  {
    ( *it )->onSuiteEnd( suite, elapsed_ms );
    ++it;
  }
}

class StartTestTask : public ProgressListenerManager::TaskBase
{
protected:
  CPPUNIT_NS::Test *test;

public:
  StartTestTask( ProgressListenerManager *manager, Event *event, CPPUNIT_NS::Test *test_in )
    : ProgressListenerManager::TaskBase( manager, event )
    , test( test_in )
  {}

  virtual void run()
  {
    this->manager->StartTestOnMainThread( this->test );
  }
};

void
ProgressListenerManager::startTest( CPPUNIT_NS::Test *test )
{
  Event *event = Event::createInstance();
  StartTestTask *task = new StartTestTask( this, event, test );
  Runner::instance()->asyncRunOnMainThread( task, true );
  event->wait();
  event->destroy();

  // 在这记录开始时间，避免把线程切换的时间也计算在内
  TestRecord record;
  record.start_ms = Runner::tickCount();
  this->test_record.push( record );
}

void
ProgressListenerManager::StartTestOnMainThread( CPPUNIT_NS::Test *test )
{
  TestProgressListeners::iterator it = this->listeners.begin();
  while ( it != this->listeners.end() )
  {
    ( *it )->onTestStart( test );
    ++it;
  }
}

class AddFailureTask : public Runnable
{
protected:
  ProgressListenerManager *manager;
  CPPUNIT_NS::TestFailure *failure;

public:
  AddFailureTask( ProgressListenerManager *manager_in, const CPPUNIT_NS::TestFailure &failure_in )
    : manager( manager_in )
    , failure( NULL )
  {
    this->failure = failure_in.clone();
  }

  virtual ~AddFailureTask()
  {
    delete this->failure;
  }

  virtual void run()
  {
    this->manager->addFailureOnMainThread( *this->failure );
  }
};

void
ProgressListenerManager::addFailure( const CPPUNIT_NS::TestFailure &failure )
{
  AddFailureTask *task = new AddFailureTask( this, failure );
  Runner::instance()->asyncRunOnMainThread( task, true );
}

void
ProgressListenerManager::addFailureOnMainThread( const CPPUNIT_NS::TestFailure &failure )
{
  TestRecord &record = this->test_record.top();

  if ( failure.isError() )
  {
    record.errors++;
  }
  else
  {
    record.failures++;
  }

  TestProgressListeners::iterator it = this->listeners.begin();
  while ( it != this->listeners.end() )
  {
    ( *it )->onFailureAdd( this->failure_index, failure );
    ++it;
  }

  ++this->failure_index;
}

class EndTestTask : public ProgressListenerManager::TaskBase
{
protected:
  CPPUNIT_NS::Test *test;
  unsigned int elapsed_ms;

public:
  EndTestTask( ProgressListenerManager *manager, Event *event, CPPUNIT_NS::Test *test_in, unsigned int elapsed_ms_in )
    : ProgressListenerManager::TaskBase( manager, event )
    , test( test_in )
    , elapsed_ms( elapsed_ms_in )
  {}

  virtual void run()
  {
    this->manager->endTestOnMainThread( this->test, this->elapsed_ms );
  }
};

void
ProgressListenerManager::endTest( CPPUNIT_NS::Test *test )
{
  Runner *runner = Runner::instance();
  // 在这记录用例耗时，避免把线程切换的时间也计算在内
  TestRecord &record = this->test_record.top();
  unsigned int elapsed_ms = ( unsigned int )( Runner::tickCount() - record.start_ms );

  Event *event = Event::createInstance();
  EndTestTask *task = new EndTestTask( this, event, test, elapsed_ms );
  runner->asyncRunOnMainThread( task, true );
  event->wait();
  event->destroy();
}

void
ProgressListenerManager::endTestOnMainThread( CPPUNIT_NS::Test *test, unsigned int elapsed_ms )
{
  TestRecord &record = this->test_record.top();
  TestProgressListeners::iterator it = this->listeners.begin();
  while ( it != this->listeners.end() )
  {
    ( *it )->onTestEnd( test, record.errors, record.failures, elapsed_ms );
    ++it;
  }

  this->test_record.pop();
}

CUTEST_NS_END
