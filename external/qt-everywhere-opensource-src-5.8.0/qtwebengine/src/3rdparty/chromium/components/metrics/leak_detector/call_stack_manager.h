// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_MANAGER_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "components/metrics/leak_detector/custom_allocator.h"
#include "components/metrics/leak_detector/stl_allocator.h"

// Summary of structures:
//
// struct CallStack:
//   Represents a unique call stack, defined by its raw call stack (array of
//   pointers), and hash value. All CallStack objects are owned by class
//   CallStackManager. Other classes may hold pointers to them but should not
//   attempt to create or modify any CallStack objects.
//
// class CallStackManager:
//   Creates CallStack objects to represent unique call stacks. Owns all
//   CallStack objects that it creates, storing them internally.
//
//   Returns the call stacks as const pointers because no caller should take
//   ownership of them and modify or delete them. The lifetime of all CallStack
//   objects is limited to that of the CallStackManager object that created
//   them. When the CallStackManager is destroyed, the CallStack objects will be
//   invalidated. Hence the caller should make sure that it does not use
//   CallStack objects beyond the lifetime of the CallStackManager that created
//   them.

namespace metrics {
namespace leak_detector {

// Struct to represent a call stack.
struct CallStack {
  // Call stack as an array of pointers, |stack|. The array length is stored in
  // |depth|. There is no null terminator.
  const void** stack;
  size_t depth;

  // Hash of call stack. It is cached here so that it doesn not have to be
  // recomputed each time.
  size_t hash;
};

// Maintains and owns all unique call stack objects.
// Not thread-safe.
class CallStackManager {
 public:
  CallStackManager();
  ~CallStackManager();

  // Returns a CallStack object for a given raw call stack. The first time a
  // particular raw call stack is passed into this function, it will create a
  // new CallStack object to hold the raw call stack data, and then return it.
  // The CallStack object is stored internally.
  //
  // On subsequent calls with the same raw call stack, this function will return
  // the previously created CallStack object.
  const CallStack* GetCallStack(size_t depth, const void* const stack[]);

  size_t size() const { return call_stacks_.size(); }

 private:
  // Allocator class for unique call stacks. Uses CustomAllocator to avoid
  // recursive malloc hook invocation when analyzing allocs and frees.
  using CallStackPointerAllocator = STLAllocator<CallStack*, CustomAllocator>;

  // Hash operator for call stack object given as a pointer.
  // Does not actually compute the hash. Instead, returns the already computed
  // hash value stored in a CallStack object.
  struct CallStackPointerStoredHash {
    size_t operator()(const CallStack* call_stack) const {
      return call_stack->hash;
    }
  };

  // Equality comparator for call stack objects given as pointers. Compares
  // their stack trace contents.
  struct CallStackPointerEqual {
    bool operator()(const CallStack* c1, const CallStack* c2) const;
  };

  // Holds all call stack objects. Each object is allocated elsewhere and stored
  // as a pointer because the container may rearrange itself internally.
  base::hash_set<CallStack*,
                 CallStackPointerStoredHash,
                 CallStackPointerEqual,
                 CallStackPointerAllocator> call_stacks_;

  DISALLOW_COPY_AND_ASSIGN(CallStackManager);
};

}  // namespace leak_detector
}  // namespace metrics

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_CALL_STACK_MANAGER_H_
