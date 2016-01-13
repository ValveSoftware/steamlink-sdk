// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_REF_COUNTED_MANAGED_H_
#define CC_BASE_REF_COUNTED_MANAGED_H_

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"

namespace cc {

template <typename T> class RefCountedManaged;

template <typename T>
class CC_EXPORT RefCountedManager {
 protected:
  RefCountedManager() : live_object_count_(0) {}
  ~RefCountedManager() {
    CHECK_EQ(0, live_object_count_);
  }

  virtual void Release(T* object) = 0;

 private:
  friend class RefCountedManaged<T>;
  int live_object_count_;
};

template <typename T>
class CC_EXPORT RefCountedManaged : public base::subtle::RefCountedBase {
 public:
  explicit RefCountedManaged(RefCountedManager<T>* manager)
      : manager_(manager) {
    manager_->live_object_count_++;
  }

  void AddRef() const {
    base::subtle::RefCountedBase::AddRef();
  }

  void Release() {
    if (base::subtle::RefCountedBase::Release()) {
      DCHECK_GT(manager_->live_object_count_, 0);
      manager_->live_object_count_--;

      // This must be the last statement in case manager deletes
      // the object immediately.
      manager_->Release(static_cast<T*>(this));
    }
  }

 protected:
  ~RefCountedManaged() {}

 private:
  RefCountedManager<T>* manager_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedManaged<T>);
};

}  // namespace cc

#endif  // CC_BASE_REF_COUNTED_MANAGED_H_
