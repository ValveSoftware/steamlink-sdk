// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIFI_POLLING_POLICY_H_
#define CONTENT_BROWSER_GEOLOCATION_WIFI_POLLING_POLICY_H_

namespace content {

// Allows sharing and mocking of the update polling policy function.
class WifiPollingPolicy {
 public:
  WifiPollingPolicy() {}
  virtual ~WifiPollingPolicy() {}
  // Calculates the new polling interval for wiFi scans, given the previous
  // interval and whether the last scan produced new results.
  virtual void UpdatePollingInterval(bool scan_results_differ) = 0;
  virtual int PollingInterval() = 0;
  virtual int NoWifiInterval() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WifiPollingPolicy);
};

// Generic polling policy, constants are compile-time parameterized to allow
// tuning on a per-platform basis.
template<int DEFAULT_INTERVAL,
         int NO_CHANGE_INTERVAL,
         int TWO_NO_CHANGE_INTERVAL,
         int NO_WIFI_INTERVAL>
class GenericWifiPollingPolicy : public WifiPollingPolicy {
 public:
  GenericWifiPollingPolicy() : polling_interval_(DEFAULT_INTERVAL) {}
  // WifiPollingPolicy
  virtual void UpdatePollingInterval(bool scan_results_differ) {
    if (scan_results_differ) {
      polling_interval_ = DEFAULT_INTERVAL;
    } else if (polling_interval_ == DEFAULT_INTERVAL) {
      polling_interval_ = NO_CHANGE_INTERVAL;
    } else {
      DCHECK(polling_interval_ == NO_CHANGE_INTERVAL ||
             polling_interval_ == TWO_NO_CHANGE_INTERVAL);
      polling_interval_ = TWO_NO_CHANGE_INTERVAL;
    }
  }
  virtual int PollingInterval() { return polling_interval_; }
  virtual int NoWifiInterval() { return NO_WIFI_INTERVAL; }

 private:
  int polling_interval_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_WIFI_POLLING_POLICY_H_
