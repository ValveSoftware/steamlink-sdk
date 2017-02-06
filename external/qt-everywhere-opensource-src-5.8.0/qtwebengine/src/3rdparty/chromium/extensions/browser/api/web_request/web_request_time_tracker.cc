// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_time_tracker.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "extensions/browser/warning_set.h"


// TODO(mpcomplete): tweak all these constants.
namespace {
// The number of requests we keep track of at a time.
const size_t kMaxRequestsLogged = 100u;

// If a request completes faster than this amount (in ms), then we ignore it.
// Any delays on such a request was negligible.
const int kMinRequestTimeToCareMs = 10;

// Thresholds above which we consider a delay caused by an extension to be "too
// much". This is given in percentage of total request time that was spent
// waiting on the extension.
const double kThresholdModerateDelay = 0.20;
const double kThresholdExcessiveDelay = 0.50;

// If this many requests (of the past kMaxRequestsLogged) have had "too much"
// delay, then we will warn the user.
const size_t kNumModerateDelaysBeforeWarning = 50u;
const size_t kNumExcessiveDelaysBeforeWarning = 10u;

// Default implementation for ExtensionWebRequestTimeTrackerDelegate
// that sets a warning in the extension service of |profile|.
class DefaultDelegate : public ExtensionWebRequestTimeTrackerDelegate {
 public:
  ~DefaultDelegate() override {}

  // Implementation of ExtensionWebRequestTimeTrackerDelegate.
  void NotifyExcessiveDelays(
      void* profile,
      size_t num_delayed_messages,
      size_t total_num_messages,
      const std::set<std::string>& extension_ids) override;
  void NotifyModerateDelays(
      void* profile,
      size_t num_delayed_messages,
      size_t total_num_messages,
      const std::set<std::string>& extension_ids) override;
};

void DefaultDelegate::NotifyExcessiveDelays(
    void* profile,
    size_t num_delayed_messages,
    size_t total_num_messages,
    const std::set<std::string>& extension_ids) {
  // TODO(battre) Enable warning the user if extensions misbehave as soon as we
  // have data that allows us to decide on reasonable limits for triggering the
  // warnings.
  // BrowserThread::PostTask(
  //     BrowserThread::UI,
  //     FROM_HERE,
  //     base::Bind(&ExtensionWarningSet::NotifyWarningsOnUI,
  //                profile,
  //                extension_ids,
  //                ExtensionWarningSet::kNetworkDelay));
}

void DefaultDelegate::NotifyModerateDelays(
    void* profile,
    size_t num_delayed_messages,
    size_t total_num_messages,
    const std::set<std::string>& extension_ids) {
  // TODO(battre) Enable warning the user if extensions misbehave as soon as we
  // have data that allows us to decide on reasonable limits for triggering the
  // warnings.
  // BrowserThread::PostTask(
  //     BrowserThread::UI,
  //     FROM_HERE,
  //     base::Bind(&ExtensionWarningSet::NotifyWarningsOnUI,
  //                profile,
  //                extension_ids,
  //                ExtensionWarningSet::kNetworkDelay));
}

}  // namespace

ExtensionWebRequestTimeTracker::RequestTimeLog::RequestTimeLog()
    : profile(NULL), completed(false) {
}

ExtensionWebRequestTimeTracker::RequestTimeLog::RequestTimeLog(
    const RequestTimeLog& other) = default;

ExtensionWebRequestTimeTracker::RequestTimeLog::~RequestTimeLog() {
}

ExtensionWebRequestTimeTracker::ExtensionWebRequestTimeTracker()
    : delegate_(new DefaultDelegate) {
}

ExtensionWebRequestTimeTracker::~ExtensionWebRequestTimeTracker() {
}

void ExtensionWebRequestTimeTracker::LogRequestStartTime(
    int64_t request_id,
    const base::Time& start_time,
    const GURL& url,
    void* profile) {
  // Trim old completed request logs.
  while (request_ids_.size() > kMaxRequestsLogged) {
    int64_t to_remove = request_ids_.front();
    request_ids_.pop();
    std::map<int64_t, RequestTimeLog>::iterator iter =
        request_time_logs_.find(to_remove);
    if (iter != request_time_logs_.end() && iter->second.completed) {
      request_time_logs_.erase(iter);
      moderate_delays_.erase(to_remove);
      excessive_delays_.erase(to_remove);
    }
  }
  request_ids_.push(request_id);

  if (request_time_logs_.find(request_id) != request_time_logs_.end()) {
    RequestTimeLog& log = request_time_logs_[request_id];
    DCHECK(!log.completed);
    return;
  }
  RequestTimeLog& log = request_time_logs_[request_id];
  log.request_start_time = start_time;
  log.url = url;
  log.profile = profile;
}

void ExtensionWebRequestTimeTracker::LogRequestEndTime(
    int64_t request_id,
    const base::Time& end_time) {
  if (request_time_logs_.find(request_id) == request_time_logs_.end())
    return;

  RequestTimeLog& log = request_time_logs_[request_id];
  if (log.completed)
    return;

  log.request_duration = end_time - log.request_start_time;
  log.completed = true;

  if (log.extension_block_durations.empty())
    return;

  UMA_HISTOGRAM_TIMES("Extensions.NetworkDelay", log.block_duration);

  Analyze(request_id);
}

std::set<std::string> ExtensionWebRequestTimeTracker::GetExtensionIds(
    const RequestTimeLog& log) const {
  std::set<std::string> result;
  for (std::map<std::string, base::TimeDelta>::const_iterator i =
           log.extension_block_durations.begin();
       i != log.extension_block_durations.end();
       ++i) {
    result.insert(i->first);
  }
  return result;
}

void ExtensionWebRequestTimeTracker::Analyze(int64_t request_id) {
  RequestTimeLog& log = request_time_logs_[request_id];

  // Ignore really short requests. Time spent on these is negligible, and any
  // extra delay the extension adds is likely to be noise.
  if (log.request_duration.InMilliseconds() < kMinRequestTimeToCareMs)
    return;

  double percentage =
      log.block_duration.InMillisecondsF() /
      log.request_duration.InMillisecondsF();
  UMA_HISTOGRAM_PERCENTAGE("Extensions.NetworkDelayPercentage",
                           static_cast<int>(100*percentage));
  VLOG(1) << "WR percent " << request_id << ": " << log.url << ": " <<
      log.block_duration.InMilliseconds() << "/" <<
      log.request_duration.InMilliseconds() << " = " << percentage;

  // TODO(mpcomplete): blame a specific extension. Maybe go through the list
  // of recent requests and find the extension that has caused the most delays.
  if (percentage > kThresholdExcessiveDelay) {
    excessive_delays_.insert(request_id);
    if (excessive_delays_.size() > kNumExcessiveDelaysBeforeWarning) {
      VLOG(1) << "WR excessive delays:" << excessive_delays_.size();
      if (delegate_.get()) {
        delegate_->NotifyExcessiveDelays(log.profile,
                                         excessive_delays_.size(),
                                         request_ids_.size(),
                                         GetExtensionIds(log));
      }
    }
  } else if (percentage > kThresholdModerateDelay) {
    moderate_delays_.insert(request_id);
    if (moderate_delays_.size() + excessive_delays_.size() >
            kNumModerateDelaysBeforeWarning) {
      VLOG(1) << "WR moderate delays:" << moderate_delays_.size();
      if (delegate_.get()) {
        delegate_->NotifyModerateDelays(
            log.profile,
            moderate_delays_.size() + excessive_delays_.size(),
            request_ids_.size(),
            GetExtensionIds(log));
      }
    }
  }
}

void ExtensionWebRequestTimeTracker::IncrementExtensionBlockTime(
    const std::string& extension_id,
    int64_t request_id,
    const base::TimeDelta& block_time) {
  if (request_time_logs_.find(request_id) == request_time_logs_.end())
    return;
  RequestTimeLog& log = request_time_logs_[request_id];
  log.extension_block_durations[extension_id] += block_time;
}

void ExtensionWebRequestTimeTracker::IncrementTotalBlockTime(
    int64_t request_id,
    const base::TimeDelta& block_time) {
  if (request_time_logs_.find(request_id) == request_time_logs_.end())
    return;
  RequestTimeLog& log = request_time_logs_[request_id];
  log.block_duration += block_time;
}

void ExtensionWebRequestTimeTracker::SetRequestCanceled(int64_t request_id) {
  // Canceled requests won't actually hit the network, so we can't compare
  // their request time to the time spent waiting on the extension. Just ignore
  // them.
  // TODO(mpcomplete): may want to count cancels as "bonuses" for an extension.
  // i.e. if it slows down 50% of requests but cancels 25% of the rest, that
  // might average out to only being "25% slow".
  request_time_logs_.erase(request_id);
}

void ExtensionWebRequestTimeTracker::SetRequestRedirected(int64_t request_id) {
  // When a request is redirected, we have no way of knowing how long the
  // request would have taken, so we can't say how much an extension slowed
  // down this request. Just ignore it.
  request_time_logs_.erase(request_id);
}

void ExtensionWebRequestTimeTracker::SetDelegate(
    ExtensionWebRequestTimeTrackerDelegate* delegate) {
  delegate_.reset(delegate);
}
