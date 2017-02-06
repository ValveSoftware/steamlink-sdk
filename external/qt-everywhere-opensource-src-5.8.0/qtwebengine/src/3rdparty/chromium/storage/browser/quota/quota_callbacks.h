// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_CALLBACKS_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_CALLBACKS_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "storage/common/quota/quota_status_code.h"
#include "storage/common/quota/quota_types.h"

class GURL;

namespace storage {

struct UsageInfo;
typedef std::vector<UsageInfo> UsageInfoEntries;

// Common callback types that are used throughout in the quota module.
typedef base::Callback<void(int64_t usage, int64_t unlimited_usage)>
    GlobalUsageCallback;
typedef base::Callback<void(QuotaStatusCode status, int64_t quota)>
    QuotaCallback;
typedef base::Callback<void(int64_t usage)> UsageCallback;
typedef base::Callback<void(QuotaStatusCode, int64_t)> AvailableSpaceCallback;
typedef base::Callback<void(QuotaStatusCode)> StatusCallback;
typedef base::Callback<void(const std::set<GURL>& origins,
                            StorageType type)> GetOriginsCallback;
typedef base::Callback<void(const UsageInfoEntries&)> GetUsageInfoCallback;
typedef base::Callback<void(const GURL&)> GetOriginCallback;

// Simple template wrapper for a callback queue.
template <typename CallbackType, typename... Args>
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
  void Run(Args... args) {
    std::vector<CallbackType> callbacks;
    callbacks.swap(callbacks_);
    for (const auto& callback : callbacks)
      callback.Run(args...);
  }

  void Swap(CallbackQueue<CallbackType, Args...>* other) {
    callbacks_.swap(other->callbacks_);
  }

  size_t size() const {
    return callbacks_.size();
  }

 private:
  std::vector<CallbackType> callbacks_;
};

template <typename CallbackType, typename Key, typename... Args>
class CallbackQueueMap {
 public:
  typedef CallbackQueue<CallbackType, Args...> CallbackQueueType;
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
  template <typename... RunArgs>
  void Run(const Key& key, RunArgs&&... args) {
    if (!this->HasCallbacks(key))
      return;
    CallbackQueueType queue;
    queue.Swap(&callback_map_[key]);
    callback_map_.erase(key);
    queue.Run(std::forward<RunArgs>(args)...);
  }

 private:
  CallbackMap callback_map_;
};

}  // namespace storage

#endif  // STORAGE_QUOTA_QUOTA_TYPES_H_
