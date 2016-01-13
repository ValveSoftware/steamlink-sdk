// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_unittest.h"

#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "base/values.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

const int kThreads = 10;
const int kEvents = 100;

base::Value* NetLogLevelCallback(NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("log_level", log_level);
  return dict;
}

TEST(NetLogTest, Basic) {
  CapturingNetLog net_log;
  CapturingNetLog::CapturedEntryList entries;
  net_log.GetEntries(&entries);
  EXPECT_EQ(0u, entries.size());

  net_log.AddGlobalEntry(NetLog::TYPE_CANCELLED);

  net_log.GetEntries(&entries);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(NetLog::TYPE_CANCELLED, entries[0].type);
  EXPECT_EQ(NetLog::SOURCE_NONE, entries[0].source.type);
  EXPECT_NE(NetLog::Source::kInvalidId, entries[0].source.id);
  EXPECT_EQ(NetLog::PHASE_NONE, entries[0].phase);
  EXPECT_GE(base::TimeTicks::Now(), entries[0].time);
  EXPECT_FALSE(entries[0].params);
}

// Check that the correct LogLevel is sent to NetLog Value callbacks.
TEST(NetLogTest, LogLevels) {
  CapturingNetLog net_log;
  for (int log_level = NetLog::LOG_ALL; log_level < NetLog::LOG_NONE;
       ++log_level) {
    net_log.SetLogLevel(static_cast<NetLog::LogLevel>(log_level));
    EXPECT_EQ(log_level, net_log.GetLogLevel());

    net_log.AddGlobalEntry(NetLog::TYPE_SOCKET_ALIVE,
                           base::Bind(NetLogLevelCallback));

    CapturingNetLog::CapturedEntryList entries;
    net_log.GetEntries(&entries);

    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(NetLog::TYPE_SOCKET_ALIVE, entries[0].type);
    EXPECT_EQ(NetLog::SOURCE_NONE, entries[0].source.type);
    EXPECT_NE(NetLog::Source::kInvalidId, entries[0].source.id);
    EXPECT_EQ(NetLog::PHASE_NONE, entries[0].phase);
    EXPECT_GE(base::TimeTicks::Now(), entries[0].time);

    int logged_log_level;
    ASSERT_TRUE(entries[0].GetIntegerValue("log_level", &logged_log_level));
    EXPECT_EQ(log_level, logged_log_level);

    net_log.Clear();
  }
}

class CountingObserver : public NetLog::ThreadSafeObserver {
 public:
  CountingObserver() : count_(0) {}

  virtual ~CountingObserver() {
    if (net_log())
      net_log()->RemoveThreadSafeObserver(this);
  }

  virtual void OnAddEntry(const NetLog::Entry& entry) OVERRIDE {
    ++count_;
  }

  int count() const { return count_; }

 private:
  int count_;
};

class LoggingObserver : public NetLog::ThreadSafeObserver {
 public:
  LoggingObserver() {}

  virtual ~LoggingObserver() {
    if (net_log())
      net_log()->RemoveThreadSafeObserver(this);
  }

  virtual void OnAddEntry(const NetLog::Entry& entry) OVERRIDE {
    base::Value* value = entry.ToValue();
    base::DictionaryValue* dict = NULL;
    ASSERT_TRUE(value->GetAsDictionary(&dict));
    values_.push_back(dict);
  }

  size_t GetNumValues() const { return values_.size(); }
  base::DictionaryValue* GetValue(size_t index) const { return values_[index]; }

 private:
  ScopedVector<base::DictionaryValue> values_;
};

base::Value* LogLevelToValue(NetLog::LogLevel log_level) {
  return new base::FundamentalValue(log_level);
}

void AddEvent(NetLog* net_log) {
  net_log->AddGlobalEntry(NetLog::TYPE_CANCELLED, base::Bind(LogLevelToValue));
}

// A thread that waits until an event has been signalled before calling
// RunTestThread.
class NetLogTestThread : public base::SimpleThread {
 public:
  NetLogTestThread()
      : base::SimpleThread("NetLogTest"),
        net_log_(NULL),
        start_event_(NULL) {
  }

  // We'll wait for |start_event| to be triggered before calling a subclass's
  // subclass's RunTestThread() function.
  void Init(NetLog* net_log, base::WaitableEvent* start_event) {
    start_event_ = start_event;
    net_log_ = net_log;
  }

  virtual void Run() OVERRIDE {
    start_event_->Wait();
    RunTestThread();
  }

  // Subclasses must override this with the code they want to run on their
  // thread.
  virtual void RunTestThread() = 0;

 protected:
  NetLog* net_log_;

 private:
  // Only triggered once all threads have been created, to make it less likely
  // each thread completes before the next one starts.
  base::WaitableEvent* start_event_;

  DISALLOW_COPY_AND_ASSIGN(NetLogTestThread);
};

// A thread that adds a bunch of events to the NetLog.
class AddEventsTestThread : public NetLogTestThread {
 public:
  AddEventsTestThread() {}
  virtual ~AddEventsTestThread() {}

 private:
  virtual void RunTestThread() OVERRIDE {
    for (int i = 0; i < kEvents; ++i)
      AddEvent(net_log_);
  }

  DISALLOW_COPY_AND_ASSIGN(AddEventsTestThread);
};

// A thread that adds and removes an observer from the NetLog repeatedly.
class AddRemoveObserverTestThread : public NetLogTestThread {
 public:
  AddRemoveObserverTestThread() {}

  virtual ~AddRemoveObserverTestThread() {
    EXPECT_TRUE(!observer_.net_log());
  }

 private:
  virtual void RunTestThread() OVERRIDE {
    for (int i = 0; i < kEvents; ++i) {
      ASSERT_FALSE(observer_.net_log());

      net_log_->AddThreadSafeObserver(&observer_, NetLog::LOG_ALL_BUT_BYTES);
      ASSERT_EQ(net_log_, observer_.net_log());
      ASSERT_EQ(NetLog::LOG_ALL_BUT_BYTES, observer_.log_level());
      ASSERT_LE(net_log_->GetLogLevel(), NetLog::LOG_ALL_BUT_BYTES);

      net_log_->SetObserverLogLevel(&observer_, NetLog::LOG_ALL);
      ASSERT_EQ(net_log_, observer_.net_log());
      ASSERT_EQ(NetLog::LOG_ALL, observer_.log_level());
      ASSERT_LE(net_log_->GetLogLevel(), NetLog::LOG_ALL);

      net_log_->RemoveThreadSafeObserver(&observer_);
      ASSERT_TRUE(!observer_.net_log());
    }
  }

  CountingObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(AddRemoveObserverTestThread);
};

// Creates |kThreads| threads of type |ThreadType| and then runs them all
// to completion.
template<class ThreadType>
void RunTestThreads(NetLog* net_log) {
  ThreadType threads[kThreads];
  base::WaitableEvent start_event(true, false);

  for (size_t i = 0; i < arraysize(threads); ++i) {
    threads[i].Init(net_log, &start_event);
    threads[i].Start();
  }

  start_event.Signal();

  for (size_t i = 0; i < arraysize(threads); ++i)
    threads[i].Join();
}

// Makes sure that events on multiple threads are dispatched to all observers.
TEST(NetLogTest, NetLogEventThreads) {
  NetLog net_log;

  // Attach some observers.  Since they're created after |net_log|, they'll
  // safely detach themselves on destruction.
  CountingObserver observers[3];
  for (size_t i = 0; i < arraysize(observers); ++i)
    net_log.AddThreadSafeObserver(&observers[i], NetLog::LOG_ALL);

  // Run a bunch of threads to completion, each of which will emit events to
  // |net_log|.
  RunTestThreads<AddEventsTestThread>(&net_log);

  // Check that each observer saw the emitted events.
  const int kTotalEvents = kThreads * kEvents;
  for (size_t i = 0; i < arraysize(observers); ++i)
    EXPECT_EQ(kTotalEvents, observers[i].count());
}

// Test adding and removing a single observer.
TEST(NetLogTest, NetLogAddRemoveObserver) {
  NetLog net_log;
  CountingObserver observer;

  AddEvent(&net_log);
  EXPECT_EQ(0, observer.count());
  EXPECT_EQ(NULL, observer.net_log());
  EXPECT_EQ(NetLog::LOG_NONE, net_log.GetLogLevel());

  // Add the observer and add an event.
  net_log.AddThreadSafeObserver(&observer, NetLog::LOG_ALL_BUT_BYTES);
  EXPECT_EQ(&net_log, observer.net_log());
  EXPECT_EQ(NetLog::LOG_ALL_BUT_BYTES, observer.log_level());
  EXPECT_EQ(NetLog::LOG_ALL_BUT_BYTES, net_log.GetLogLevel());

  AddEvent(&net_log);
  EXPECT_EQ(1, observer.count());

  // Change the observer's logging level and add an event.
  net_log.SetObserverLogLevel(&observer, NetLog::LOG_ALL);
  EXPECT_EQ(&net_log, observer.net_log());
  EXPECT_EQ(NetLog::LOG_ALL, observer.log_level());
  EXPECT_EQ(NetLog::LOG_ALL, net_log.GetLogLevel());

  AddEvent(&net_log);
  EXPECT_EQ(2, observer.count());

  // Remove observer and add an event.
  net_log.RemoveThreadSafeObserver(&observer);
  EXPECT_EQ(NULL, observer.net_log());
  EXPECT_EQ(NetLog::LOG_NONE, net_log.GetLogLevel());

  AddEvent(&net_log);
  EXPECT_EQ(2, observer.count());

  // Add the observer a final time, and add an event.
  net_log.AddThreadSafeObserver(&observer, NetLog::LOG_ALL);
  EXPECT_EQ(&net_log, observer.net_log());
  EXPECT_EQ(NetLog::LOG_ALL, observer.log_level());
  EXPECT_EQ(NetLog::LOG_ALL, net_log.GetLogLevel());

  AddEvent(&net_log);
  EXPECT_EQ(3, observer.count());
}

// Test adding and removing two observers at different log levels.
TEST(NetLogTest, NetLogTwoObservers) {
  NetLog net_log;
  LoggingObserver observer[2];

  // Add first observer.
  net_log.AddThreadSafeObserver(&observer[0], NetLog::LOG_ALL_BUT_BYTES);
  EXPECT_EQ(&net_log, observer[0].net_log());
  EXPECT_EQ(NULL, observer[1].net_log());
  EXPECT_EQ(NetLog::LOG_ALL_BUT_BYTES, observer[0].log_level());
  EXPECT_EQ(NetLog::LOG_ALL_BUT_BYTES, net_log.GetLogLevel());

  // Add second observer observer.
  net_log.AddThreadSafeObserver(&observer[1], NetLog::LOG_ALL);
  EXPECT_EQ(&net_log, observer[0].net_log());
  EXPECT_EQ(&net_log, observer[1].net_log());
  EXPECT_EQ(NetLog::LOG_ALL_BUT_BYTES, observer[0].log_level());
  EXPECT_EQ(NetLog::LOG_ALL, observer[1].log_level());
  EXPECT_EQ(NetLog::LOG_ALL, net_log.GetLogLevel());

  // Add event and make sure both observers receive it at their respective log
  // levels.
  int param;
  AddEvent(&net_log);
  ASSERT_EQ(1U, observer[0].GetNumValues());
  ASSERT_TRUE(observer[0].GetValue(0)->GetInteger("params", &param));
  EXPECT_EQ(observer[0].log_level(), param);
  ASSERT_EQ(1U, observer[1].GetNumValues());
  ASSERT_TRUE(observer[1].GetValue(0)->GetInteger("params", &param));
  EXPECT_EQ(observer[1].log_level(), param);

  // Remove second observer.
  net_log.RemoveThreadSafeObserver(&observer[1]);
  EXPECT_EQ(&net_log, observer[0].net_log());
  EXPECT_EQ(NULL, observer[1].net_log());
  EXPECT_EQ(NetLog::LOG_ALL_BUT_BYTES, observer[0].log_level());
  EXPECT_EQ(NetLog::LOG_ALL_BUT_BYTES, net_log.GetLogLevel());

  // Add event and make sure only second observer gets it.
  AddEvent(&net_log);
  EXPECT_EQ(2U, observer[0].GetNumValues());
  EXPECT_EQ(1U, observer[1].GetNumValues());

  // Remove first observer.
  net_log.RemoveThreadSafeObserver(&observer[0]);
  EXPECT_EQ(NULL, observer[0].net_log());
  EXPECT_EQ(NULL, observer[1].net_log());
  EXPECT_EQ(NetLog::LOG_NONE, net_log.GetLogLevel());

  // Add event and make sure neither observer gets it.
  AddEvent(&net_log);
  EXPECT_EQ(2U, observer[0].GetNumValues());
  EXPECT_EQ(1U, observer[1].GetNumValues());
}

// Makes sure that adding and removing observers simultaneously on different
// threads works.
TEST(NetLogTest, NetLogAddRemoveObserverThreads) {
  NetLog net_log;

  // Run a bunch of threads to completion, each of which will repeatedly add
  // and remove an observer, and set its logging level.
  RunTestThreads<AddRemoveObserverTestThread>(&net_log);
}

}  // namespace

}  // namespace net
