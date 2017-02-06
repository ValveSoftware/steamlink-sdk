// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_TIMED_MAP_H_
#define COMPONENTS_COPRESENCE_TIMED_MAP_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace copresence {

// TimedMap is a map with the added functionality of clearing any
// key/value pair after its specified lifetime is over.
// TODO(ckehoe): Why is this interface so different from std::map?
template <typename KeyType, typename ValueType>
class TimedMap {
 public:
  TimedMap(const base::TimeDelta& lifetime, size_t max_elements)
      : kEmptyValue(ValueType()),
        clock_(new base::DefaultTickClock()),
        lifetime_(lifetime),
        max_elements_(max_elements) {
    timer_.Start(FROM_HERE, lifetime_, this, &TimedMap::ClearExpiredTokens);
  }

  ~TimedMap() {}

  void Add(const KeyType& key, const ValueType& value) {
    map_[key] = value;
    expiry_queue_.push(KeyTimeTuple(key, clock_->NowTicks() + lifetime_));
    while (map_.size() > max_elements_)
      ClearOldestToken();
  }

  bool HasKey(const KeyType& key) {
    ClearExpiredTokens();
    return map_.find(key) != map_.end();
  }

  const ValueType& GetValue(const KeyType& key) {
    ClearExpiredTokens();
    auto elt = map_.find(key);
    return elt == map_.end() ? kEmptyValue : elt->second;
  }

  ValueType* GetMutableValue(const KeyType& key) {
    ClearExpiredTokens();
    auto elt = map_.find(key);
    return elt == map_.end() ? nullptr : &(elt->second);
  }

  // TODO(ckehoe): Add a unit test for this.
  size_t Erase(const KeyType& key) {
    return map_.erase(key);
  }

  void set_clock_for_testing(std::unique_ptr<base::TickClock> clock) {
    clock_ = std::move(clock);
  }

 private:
  void ClearExpiredTokens() {
    while (!expiry_queue_.empty() &&
           expiry_queue_.top().second <= clock_->NowTicks())
      ClearOldestToken();
  }

  void ClearOldestToken() {
    map_.erase(expiry_queue_.top().first);
    expiry_queue_.pop();
  }

  using KeyTimeTuple = std::pair<KeyType, base::TimeTicks>;

  class EarliestFirstComparator {
   public:
    // This will sort our queue with the 'earliest' time being the top.
    bool operator()(const KeyTimeTuple& left, const KeyTimeTuple& right) const {
      return left.second > right.second;
    }
  };

  using ExpiryQueue = std::priority_queue<
      KeyTimeTuple, std::vector<KeyTimeTuple>, EarliestFirstComparator>;

  const ValueType kEmptyValue;

  std::unique_ptr<base::TickClock> clock_;
  base::RepeatingTimer timer_;
  const base::TimeDelta lifetime_;
  const size_t max_elements_;
  std::map<KeyType, ValueType> map_;
  // Priority queue with our element keys ordered by the earliest expiring keys
  // first.
  ExpiryQueue expiry_queue_;

  DISALLOW_COPY_AND_ASSIGN(TimedMap);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_TIMED_MAP_H_
