// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_QUOTA_QUOTA_CALLBACKS_H_
#define WEBKIT_BROWSER_QUOTA_QUOTA_CALLBACKS_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/tuple.h"
#include "webkit/common/quota/quota_status_code.h"
#include "webkit/common/quota/quota_types.h"

class GURL;

namespace quota {

struct UsageInfo;
typedef std::vector<UsageInfo> UsageInfoEntries;

// Common callback types that are used throughout in the quota module.
typedef base::Callback<void(int64 usage,
                            int64 unlimited_usage)> GlobalUsageCallback;
typedef base::Callback<void(QuotaStatusCode status, int64 quota)> QuotaCallback;
typedef base::Callback<void(int64 usage)> UsageCallback;
typedef base::Callback<void(QuotaStatusCode, int64)> AvailableSpaceCallback;
typedef base::Callback<void(QuotaStatusCode)> StatusCallback;
typedef base::Callback<void(const std::set<GURL>& origins,
                            StorageType type)> GetOriginsCallback;
typedef base::Callback<void(const UsageInfoEntries&)> GetUsageInfoCallback;

template<typename CallbackType, typename Args>
void DispatchToCallback(const CallbackType& callback,
                        const Args& args) {
  DispatchToMethod(&callback, &CallbackType::Run, args);
}

// Simple template wrapper for a callback queue.
template <typename CallbackType, typename Args>
class CallbackQueue {
 public:
  // Returns true if the given |callback| is the first one added to the queue.
  bool Add(const CallbackType& callback) {
    callbacks_.push_back(callback);
    return (callbacks_.size() == 1);
  }

  bool HasCallbacks() const {
    return !callbacks_.empty();
  }

  // Runs the callbacks added to the queue and clears the queue.
  void Run(const Args& args) {
    typedef typename std::vector<CallbackType>::iterator iterator;
    for (iterator iter = callbacks_.begin();
         iter != callbacks_.end(); ++iter)
      DispatchToCallback(*iter, args);
    callbacks_.clear();
  }

 private:
  std::vector<CallbackType> callbacks_;
};

typedef CallbackQueue<GlobalUsageCallback,
                      Tuple2<int64, int64> >
    GlobalUsageCallbackQueue;
typedef CallbackQueue<UsageCallback, Tuple1<int64> >
    UsageCallbackQueue;
typedef CallbackQueue<AvailableSpaceCallback,
                      Tuple2<QuotaStatusCode, int64> >
    AvailableSpaceCallbackQueue;
typedef CallbackQueue<QuotaCallback,
                      Tuple2<QuotaStatusCode, int64> >
    GlobalQuotaCallbackQueue;
typedef CallbackQueue<base::Closure, Tuple0> ClosureQueue;

template <typename CallbackType, typename Key, typename Args>
class CallbackQueueMap {
 public:
  typedef CallbackQueue<CallbackType, Args> CallbackQueueType;
  typedef std::map<Key, CallbackQueueType> CallbackMap;
  typedef typename CallbackMap::iterator iterator;

  bool Add(const Key& key, const CallbackType& callback) {
    return callback_map_[key].Add(callback);
  }

  bool HasCallbacks(const Key& key) const {
    return (callback_map_.find(key) != callback_map_.end());
  }

  bool HasAnyCallbacks() const {
    return !callback_map_.empty();
  }

  iterator Begin() { return callback_map_.begin(); }
  iterator End() { return callback_map_.end(); }

  void Clear() { callback_map_.clear(); }

  // Runs the callbacks added for the given |key| and clears the key
  // from the map.
  void Run(const Key& key, const Args& args) {
    if (!this->HasCallbacks(key))
      return;
    CallbackQueueType& queue = callback_map_[key];
    queue.Run(args);
    callback_map_.erase(key);
  }

 private:
  CallbackMap callback_map_;
};

typedef CallbackQueueMap<UsageCallback, std::string, Tuple1<int64> >
    HostUsageCallbackMap;
typedef CallbackQueueMap<QuotaCallback, std::string,
                         Tuple2<QuotaStatusCode, int64> >
    HostQuotaCallbackMap;

}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_TYPES_H_
