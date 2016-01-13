// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_QUOTA_STORAGE_OBSERVER_H_
#define WEBKIT_BROWSER_QUOTA_STORAGE_OBSERVER_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "url/gurl.h"
#include "webkit/browser/quota/quota_client.h"
#include "webkit/common/quota/quota_types.h"

namespace quota {

// This interface is implemented by observers that wish to monitor storage
// events, such as changes in quota or usage.
class WEBKIT_STORAGE_BROWSER_EXPORT StorageObserver {
 public:
  struct WEBKIT_STORAGE_BROWSER_EXPORT Filter {
    // The storage type to monitor. This must not be kStorageTypeUnknown or
    // kStorageTypeQuotaNotManaged.
    StorageType storage_type;

    // The origin to monitor usage for. Must be specified.
    GURL origin;

    Filter();
    Filter(StorageType storage_type, const GURL& origin);
    bool operator==(const Filter& other) const;
  };

  struct WEBKIT_STORAGE_BROWSER_EXPORT MonitorParams {
    // Storage type and origin to monitor.
    Filter filter;

    // The rate at which storage events will be fired. Events will be fired at
    // approximately this rate, or when a storage status change has been
    // detected, whichever is the least frequent.
    base::TimeDelta rate;

    // If set to true, the observer will be dispatched an event when added.
    bool dispatch_initial_state;

    MonitorParams();
    MonitorParams(StorageType storage_type,
                  const GURL& origin,
                  const base::TimeDelta& rate,
                  bool get_initial_state);
    MonitorParams(const Filter& filter,
                  const base::TimeDelta& rate,
                  bool get_initial_state);
  };

  struct WEBKIT_STORAGE_BROWSER_EXPORT Event {
    // The storage type and origin monitored.
    Filter filter;

    // The current usage corresponding to the filter.
    int64 usage;

    // The quota corresponding to the filter.
    int64 quota;

    Event();
    Event(const Filter& filter, int64 usage, int64 quota);
    bool operator==(const Event& other) const;
  };

  // Will be called on the IO thread when a storage event occurs.
  virtual void OnStorageEvent(const Event& event) = 0;

 protected:
  virtual ~StorageObserver() {}
};

}  // namespace quota

#endif  // WEBKIT_BROWSER_QUOTA_STORAGE_OBSERVER_H_
