// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker_impl.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"

namespace content {

// TODO(rjkroege): Add display power saving control to the ozone interface.
// This implementation is necessary to satisfy linkage.
class PowerSaveBlockerImpl::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlockerImpl::Delegate> {
 public:
  Delegate() {}

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  virtual ~Delegate() {}

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

PowerSaveBlockerImpl::PowerSaveBlockerImpl(PowerSaveBlockerType type,
                                           const std::string& reason)
    : delegate_(new Delegate()) {
  NOTIMPLEMENTED();
}

PowerSaveBlockerImpl::~PowerSaveBlockerImpl() { NOTIMPLEMENTED(); }

}  // namespace content
