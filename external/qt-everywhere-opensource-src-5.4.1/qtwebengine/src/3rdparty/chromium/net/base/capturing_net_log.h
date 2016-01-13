// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CAPTURING_NET_LOG_H_
#define NET_BASE_CAPTURING_NET_LOG_H_

#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "net/base/net_log.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace net {

// CapturingNetLog is a NetLog which instantiates Observer that saves messages
// to a bounded buffer.  It is intended for testing only, and is part of the
// net_test_support project. This is provided for convinience and compatilbility
// with the old unittests.
class CapturingNetLog : public NetLog {
 public:
  struct CapturedEntry {
    CapturedEntry(EventType type,
                  const base::TimeTicks& time,
                  Source source,
                  EventPhase phase,
                  scoped_ptr<base::DictionaryValue> params);
    // Copy constructor needed to store in a std::vector because of the
    // scoped_ptr.
    CapturedEntry(const CapturedEntry& entry);

    ~CapturedEntry();

    // Equality operator needed to store in a std::vector because of the
    // scoped_ptr.
    CapturedEntry& operator=(const CapturedEntry& entry);

    // Attempt to retrieve an value of the specified type with the given name
    // from |params|.  Returns true on success, false on failure.  Does not
    // modify |value| on failure.
    bool GetStringValue(const std::string& name, std::string* value) const;
    bool GetIntegerValue(const std::string& name, int* value) const;
    bool GetListValue(const std::string& name, base::ListValue** value) const;

    // Same as GetIntegerValue, but returns the error code associated with a
    // log entry.
    bool GetNetErrorCode(int* value) const;

    // Returns the parameters as a JSON string, or empty string if there are no
    // parameters.
    std::string GetParamsJson() const;

    EventType type;
    base::TimeTicks time;
    Source source;
    EventPhase phase;
    scoped_ptr<base::DictionaryValue> params;
  };

  // Ordered set of entries that were logged.
  typedef std::vector<CapturedEntry> CapturedEntryList;

  CapturingNetLog();
  virtual ~CapturingNetLog();

  void SetLogLevel(LogLevel log_level);

  // Below methods are forwarded to capturing_net_log_observer_.
  void GetEntries(CapturedEntryList* entry_list) const;
  void GetEntriesForSource(Source source, CapturedEntryList* entry_list) const;
  size_t GetSize() const;
  void Clear();

 private:
  // Observer is an implementation of NetLog::ThreadSafeObserver
  // that saves messages to a bounded buffer. It is intended for testing only,
  // and is part of the net_test_support project.
  class Observer : public NetLog::ThreadSafeObserver {
   public:
    Observer();
    virtual ~Observer();

    // Returns the list of all entries in the log.
    void GetEntries(CapturedEntryList* entry_list) const;

    // Fills |entry_list| with all entries in the log from the specified Source.
    void GetEntriesForSource(Source source,
                             CapturedEntryList* entry_list) const;

    // Returns number of entries in the log.
    size_t GetSize() const;

    void Clear();

   private:
    // ThreadSafeObserver implementation:
    virtual void OnAddEntry(const Entry& entry) OVERRIDE;

    // Needs to be "mutable" so can use it in GetEntries().
    mutable base::Lock lock_;

    CapturedEntryList captured_entries_;

    DISALLOW_COPY_AND_ASSIGN(Observer);
  };

  Observer capturing_net_log_observer_;

  DISALLOW_COPY_AND_ASSIGN(CapturingNetLog);
};

// Helper class that exposes a similar API as BoundNetLog, but uses a
// CapturingNetLog rather than the more generic NetLog.
//
// CapturingBoundNetLog can easily be converted to a BoundNetLog using the
// bound() method.
class CapturingBoundNetLog {
 public:
  CapturingBoundNetLog();
  ~CapturingBoundNetLog();

  // The returned BoundNetLog is only valid while |this| is alive.
  BoundNetLog bound() const { return net_log_; }

  // Fills |entry_list| with all entries in the log.
  void GetEntries(CapturingNetLog::CapturedEntryList* entry_list) const;

  // Fills |entry_list| with all entries in the log from the specified Source.
  void GetEntriesForSource(
      NetLog::Source source,
      CapturingNetLog::CapturedEntryList* entry_list) const;

  // Returns number of entries in the log.
  size_t GetSize() const;

  void Clear();

  // Sets the log level of the underlying CapturingNetLog.
  void SetLogLevel(NetLog::LogLevel log_level);

 private:
  CapturingNetLog capturing_net_log_;
  const BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(CapturingBoundNetLog);
};

}  // namespace net

#endif  // NET_BASE_CAPTURING_NET_LOG_H_
