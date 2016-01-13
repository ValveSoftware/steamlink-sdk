// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_LOG_LOGGER_H_
#define NET_BASE_NET_LOG_LOGGER_H_

#include <stdio.h>

#include "base/files/scoped_file.h"
#include "net/base/net_log.h"

namespace base {
class FilePath;
class Value;
}

namespace net {

// NetLogLogger watches the NetLog event stream, and sends all entries to
// a file specified on creation.
//
// The text file will contain a single JSON object.
class NET_EXPORT NetLogLogger : public NetLog::ThreadSafeObserver {
 public:
  // Takes ownership of |file| and will write network events to it once logging
  // starts.  |file| must be non-NULL handle and be open for writing.
  // |constants| is a legend for decoding constant values used in the log.
  NetLogLogger(FILE* file, const base::Value& constants);
  virtual ~NetLogLogger();

  // Sets the log level to log at. Must be called before StartObserving.
  void set_log_level(NetLog::LogLevel log_level);

  // Starts observing specified NetLog.  Must not already be watching a NetLog.
  // Separate from constructor to enforce thread safety.
  void StartObserving(NetLog* net_log);

  // Stops observing net_log().  Must already be watching.
  void StopObserving();

  // net::NetLog::ThreadSafeObserver implementation:
  virtual void OnAddEntry(const NetLog::Entry& entry) OVERRIDE;

  // Create a dictionary containing legend for net/ constants.  Caller takes
  // ownership of returned value.
  static base::DictionaryValue* GetConstants();

 private:
  base::ScopedFILE file_;

  // The LogLevel to log at.
  NetLog::LogLevel log_level_;

  // True if OnAddEntry() has been called at least once.
  bool added_events_;

  DISALLOW_COPY_AND_ASSIGN(NetLogLogger);
};

}  // namespace net

#endif  // NET_BASE_NET_LOG_LOGGER_H_
