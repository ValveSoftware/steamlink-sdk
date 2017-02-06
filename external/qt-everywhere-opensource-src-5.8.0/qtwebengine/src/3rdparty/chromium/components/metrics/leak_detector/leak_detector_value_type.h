// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_VALUE_TYPE_
#define COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_VALUE_TYPE_

#include <stddef.h>
#include <stdint.h>

namespace metrics {
namespace leak_detector {

// Used for tracking unique call stacks.
// Not thread-safe.
struct CallStack;

class LeakDetectorValueType {
 public:
  // Supported types.
  enum Type {
    TYPE_NONE,
    TYPE_SIZE,
    TYPE_CALL_STACK,
  };

  LeakDetectorValueType() : type_(TYPE_NONE), size_(0), call_stack_(nullptr) {}
  explicit LeakDetectorValueType(size_t size)
      : type_(TYPE_SIZE), size_(size), call_stack_(nullptr) {}
  explicit LeakDetectorValueType(const CallStack* call_stack)
      : type_(TYPE_CALL_STACK), size_(0), call_stack_(call_stack) {}

  // Accessors.
  Type type() const { return type_; }
  size_t size() const { return size_; }
  const CallStack* call_stack() const { return call_stack_; }

  // Comparators.
  bool operator==(const LeakDetectorValueType& other) const;
  bool operator<(const LeakDetectorValueType& other) const;

 private:
  Type type_;

  size_t size_;
  const CallStack* call_stack_;
};

}  // namespace leak_detector
}  // namespace metrics

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_LEAK_DETECTOR_VALUE_TYPE_
