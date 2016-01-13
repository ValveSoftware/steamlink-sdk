// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_UNIQUE_NOTIFIER_H_
#define CC_BASE_UNIQUE_NOTIFIER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "cc/base/cc_export.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace cc {

class CC_EXPORT UniqueNotifier {
 public:
  // Configure this notifier to issue the |closure| notification when scheduled.
  UniqueNotifier(base::SequencedTaskRunner* task_runner,
                 const base::Closure& closure);

  // Destroying the notifier will ensure that no further notifications will
  // happen from this class.
  ~UniqueNotifier();

  // Schedule a notification to be run. If another notification is already
  // pending, then only one notification will take place.
  void Schedule();

 private:
  void Notify();

  base::SequencedTaskRunner* task_runner_;
  base::Closure closure_;
  bool notification_pending_;

  base::WeakPtrFactory<UniqueNotifier> weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_BASE_UNIQUE_NOTIFIER_H_
