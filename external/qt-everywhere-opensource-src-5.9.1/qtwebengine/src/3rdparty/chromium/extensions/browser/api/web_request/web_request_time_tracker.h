// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_TIME_TRACKER_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_TIME_TRACKER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class Time;
}

class ExtensionWebRequestTimeTrackerDelegate {
 public:
  virtual ~ExtensionWebRequestTimeTrackerDelegate() {}

  // Notifies the delegate that |num_delayed_messages| of the last
  // |total_num_messages| inspected messages were excessively/moderately
  // delayed. Every excessively delayed message is also counted as a moderately
  // delayed message.
  virtual void NotifyExcessiveDelays(
      void* profile,
      size_t num_delayed_messages,
      size_t total_num_messages,
      const std::set<std::string>& extension_ids) = 0;
  virtual void NotifyModerateDelays(
      void* profile,
      size_t num_delayed_messages,
      size_t total_num_messages,
      const std::set<std::string>& extension_ids) = 0;
};

// This class keeps monitors how much delay extensions add to network requests
// by using the webRequest API. If the delay is sufficient, we will warn the
// user that extensions are slowing down the browser.
class ExtensionWebRequestTimeTracker {
 public:
  ExtensionWebRequestTimeTracker();
  ~ExtensionWebRequestTimeTracker();

  // Records the time that a request was created.
  void LogRequestStartTime(int64_t request_id,
                           const base::Time& start_time,
                           const GURL& url,
                           void* profile);

  // Records the time that a request either completed or encountered an error.
  void LogRequestEndTime(int64_t request_id, const base::Time& end_time);

  // Records an additional delay for the given request caused by the given
  // extension.
  void IncrementExtensionBlockTime(const std::string& extension_id,
                                   int64_t request_id,
                                   const base::TimeDelta& block_time);

  // Records an additional delay for the given request caused by all extensions
  // combined.
  void IncrementTotalBlockTime(int64_t request_id,
                               const base::TimeDelta& block_time);

  // Called when an extension has canceled the given request.
  void SetRequestCanceled(int64_t request_id);

  // Called when an extension has redirected the given request to another URL.
  void SetRequestRedirected(int64_t request_id);

  // Takes ownership of |delegate|.
  void SetDelegate(ExtensionWebRequestTimeTrackerDelegate* delegate);

 private:
  // Timing information for a single request.
  struct RequestTimeLog {
    GURL url;  // used for debug purposes only
    void* profile;  // profile that created the request
    bool completed;
    base::Time request_start_time;
    base::TimeDelta request_duration;
    base::TimeDelta block_duration;
    std::map<std::string, base::TimeDelta> extension_block_durations;
    RequestTimeLog();
    RequestTimeLog(const RequestTimeLog& other);
    ~RequestTimeLog();
  };

  // Called after a request finishes, to analyze the delays and warn the user
  // if necessary.
  void Analyze(int64_t request_id);

  // Returns a list of all extension IDs that contributed to delay for |log|.
  std::set<std::string> GetExtensionIds(const RequestTimeLog& log) const;

  // A map of recent request IDs to timing info for each request.
  std::map<int64_t, RequestTimeLog> request_time_logs_;

  // A list of recent request IDs that we know about. Used to limit the size of
  // the logs.
  std::queue<int64_t> request_ids_;

  // The set of recent requests that have been delayed either a large or
  // moderate amount by extensions.
  std::set<int64_t> excessive_delays_;
  std::set<int64_t> moderate_delays_;

  // Defaults to a delegate that sets warnings in the extension service.
  std::unique_ptr<ExtensionWebRequestTimeTrackerDelegate> delegate_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTimeTrackerTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTimeTrackerTest,
                           IgnoreFastRequests);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTimeTrackerTest,
                           CancelOrRedirect);
  FRIEND_TEST_ALL_PREFIXES(ExtensionWebRequestTimeTrackerTest, Delays);

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebRequestTimeTracker);
};

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_TIME_TRACKER_H_
