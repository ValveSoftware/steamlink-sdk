// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_
#define COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_

#include <stdint.h>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class MessageLoop;
class TickClock;
}  // namespace base

namespace client_update_protocol {
class Ecdsa;
}  // namespace client_update_protocol

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}  // namespace net

namespace network_time {

// Clock resolution is platform dependent.
#if defined(OS_WIN)
const int64_t kTicksResolutionMs = base::Time::kMinLowResolutionThresholdMs;
#else
const int64_t kTicksResolutionMs = 1;  // Assume 1ms for non-windows platforms.
#endif

// A class that receives network time updates and can provide the network time
// for a corresponding local time. This class is not thread safe.
class NetworkTimeTracker : public net::URLFetcherDelegate {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Constructor.  Arguments may be stubbed out for tests.  |getter|, if not
  // null, will cause automatic queries to a time server.  Otherwise, time is
  // available only if |UpdateNetworkTime| is called.
  NetworkTimeTracker(std::unique_ptr<base::Clock> clock,
                     std::unique_ptr<base::TickClock> tick_clock,
                     PrefService* pref_service,
                     scoped_refptr<net::URLRequestContextGetter> getter);
  ~NetworkTimeTracker() override;

  // Sets |network_time| to an estimate of the true time.  Returns true if time
  // is available, and false otherwise.  If |uncertainty| is non-NULL, it will
  // be set to an estimate of the error range.
  //
  // Network time may be available on startup if deserialized from a pref.
  // Failing that, a call to |UpdateNetworkTime| is required to make time
  // available to callers of |GetNetworkTime|.  Subsequently, network time may
  // become unavailable if |NetworkTimeTracker| has reason to believe it is no
  // longer accurate.  Consumers should even be prepared to handle the case
  // where calls to |GetNetworkTime| never once succeeds.
  bool GetNetworkTime(base::Time* network_time,
                      base::TimeDelta* uncertainty) const;

  // Calculates corresponding time ticks according to the given parameters.
  // The provided |network_time| is precise at the given |resolution| and
  // represent the time between now and up to |latency| + (now - |post_time|)
  // ago.
  void UpdateNetworkTime(base::Time network_time,
                         base::TimeDelta resolution,
                         base::TimeDelta latency,
                         base::TimeTicks post_time);

  void SetMaxResponseSizeForTesting(size_t limit);

  void SetPublicKeyForTesting(const base::StringPiece& key);

  void SetTimeServerURLForTesting(const GURL& url);

  bool QueryTimeServiceForTesting();

  void WaitForFetchForTesting(uint32_t nonce);

  base::TimeDelta GetTimerDelayForTesting() const;

 private:
  // Checks whether a network time query should be issued, and issues one if so.
  // Upon response, execution resumes in |OnURLFetchComplete|.
  void CheckTime();

  // Updates network time from a time server response, returning true
  // if successful.
  bool UpdateTimeFromResponse();

  // net::URLFetcherDelegate:
  // Called to process responses from the secure time service.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Sets the next time query to be run at the specified time.
  void QueueCheckTime(base::TimeDelta delay);

  // Returns true if there's sufficient reason to suspect that
  // NetworkTimeTracker does not know what time it is.  This returns true
  // unconditionally every once in a long while, just to be on the safe side.
  bool ShouldIssueTimeQuery();

  // State variables for internally-managed secure time service queries.
  GURL server_url_;
  size_t max_response_size_;
  base::TimeDelta backoff_;
  // Timer that runs CheckTime().  All backoff and delay is implemented by
  // changing the delay of this timer, with the result that CheckTime() may
  // assume that if it runs, it is eligible to issue a time query.
  base::RepeatingTimer timer_;
  scoped_refptr<net::URLRequestContextGetter> getter_;
  std::unique_ptr<net::URLFetcher> time_fetcher_;
  base::TimeTicks fetch_started_;
  std::unique_ptr<client_update_protocol::Ecdsa> query_signer_;
  base::MessageLoop* loop_;  // For testing; quit on fetch complete.

  // The |Clock| and |TickClock| are used to sanity-check one another, allowing
  // the NetworkTimeTracker to notice e.g. suspend/resume events and clock
  // resets.
  std::unique_ptr<base::Clock> clock_;
  std::unique_ptr<base::TickClock> tick_clock_;

  PrefService* pref_service_;

  // Network time based on last call to UpdateNetworkTime().
  mutable base::Time network_time_at_last_measurement_;

  // The estimated local times that correspond with |network_time_|. Assumes
  // the actual network time measurement was performed midway through the
  // latency time.  See UpdateNetworkTime(...) implementation for details.  The
  // tick clock is the one actually used to return values to callers, but both
  // clocks must agree to within some tolerance.
  base::Time time_at_last_measurement_;
  base::TimeTicks ticks_at_last_measurement_;

  // Uncertainty of |network_time_| based on added inaccuracies/resolution.  See
  // UpdateNetworkTime(...) implementation for details.
  base::TimeDelta network_time_uncertainty_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTimeTracker);
};

}  // namespace network_time

#endif  // COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_
