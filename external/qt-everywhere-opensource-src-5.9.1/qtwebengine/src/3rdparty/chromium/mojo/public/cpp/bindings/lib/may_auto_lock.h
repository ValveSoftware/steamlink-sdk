// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/synchronization/lock.h"

namespace mojo {
namespace internal {

// Similar to base::AutoLock, except that it does nothing if |lock| passed into
// the constructor is null.
class MayAutoLock {
 public:
  explicit MayAutoLock(base::Lock* lock) : lock_(lock) {
    if (lock_)
      lock_->Acquire();
  }

  ~MayAutoLock() {
    if (lock_) {
      lock_->AssertAcquired();
      lock_->Release();
    }
  }

 private:
  base::Lock* lock_;
  DISALLOW_COPY_AND_ASSIGN(MayAutoLock);
};

// Similar to base::AutoUnlock, except that it does nothing if |lock| passed
// into the constructor is null.
class MayAutoUnlock {
 public:
  explicit MayAutoUnlock(base::Lock* lock) : lock_(lock) {
    if (lock_) {
      lock_->AssertAcquired();
      lock_->Release();
    }
  }

  ~MayAutoUnlock() {
    if (lock_)
      lock_->Acquire();
  }

 private:
  base::Lock* lock_;
  DISALLOW_COPY_AND_ASSIGN(MayAutoUnlock);
};

}  // namespace internal
}  // namespace mojo
