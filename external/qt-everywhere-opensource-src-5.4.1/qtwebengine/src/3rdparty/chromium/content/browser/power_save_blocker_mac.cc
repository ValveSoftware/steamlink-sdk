// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker_impl.h"

#include <IOKit/pwr_mgt/IOPMLib.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"

namespace content {
namespace {

// Power management cannot be done on the UI thread. IOPMAssertionCreate does a
// synchronous MIG call to configd, so if it is called on the main thread the UI
// is at the mercy of another process. See http://crbug.com/79559 and
// http://www.opensource.apple.com/source/IOKitUser/IOKitUser-514.16.31/pwr_mgt.subproj/IOPMLibPrivate.c .
struct PowerSaveBlockerLazyInstanceTraits {
  static const bool kRegisterOnExit = false;
#ifndef NDEBUG
  static const bool kAllowedToAccessOnNonjoinableThread = true;
#endif

  static base::Thread* New(void* instance) {
    base::Thread* thread = new (instance) base::Thread("PowerSaveBlocker");
    thread->Start();
    return thread;
  }
  static void Delete(base::Thread* instance) { }
};
base::LazyInstance<base::Thread, PowerSaveBlockerLazyInstanceTraits>
    g_power_thread = LAZY_INSTANCE_INITIALIZER;

}  // namespace

class PowerSaveBlockerImpl::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlockerImpl::Delegate> {
 public:
  Delegate(PowerSaveBlockerType type, const std::string& reason)
      : type_(type), reason_(reason), assertion_(kIOPMNullAssertionID) {}

  // Does the actual work to apply or remove the desired power save block.
  void ApplyBlock();
  void RemoveBlock();

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  ~Delegate() {}
  PowerSaveBlockerType type_;
  std::string reason_;
  IOPMAssertionID assertion_;
};

void PowerSaveBlockerImpl::Delegate::ApplyBlock() {
  DCHECK_EQ(base::PlatformThread::CurrentId(),
            g_power_thread.Pointer()->thread_id());

  CFStringRef level = NULL;
  // See QA1340 <http://developer.apple.com/library/mac/#qa/qa1340/> for more
  // details.
  switch (type_) {
    case PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension:
      level = kIOPMAssertionTypeNoIdleSleep;
      break;
    case PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep:
      level = kIOPMAssertionTypeNoDisplaySleep;
      break;
    default:
      NOTREACHED();
      break;
  }
  if (level) {
    base::ScopedCFTypeRef<CFStringRef> cf_reason(
        base::SysUTF8ToCFStringRef(reason_));
    IOReturn result = IOPMAssertionCreateWithName(level,
                                                  kIOPMAssertionLevelOn,
                                                  cf_reason,
                                                  &assertion_);
    LOG_IF(ERROR, result != kIOReturnSuccess)
        << "IOPMAssertionCreate: " << result;
  }
}

void PowerSaveBlockerImpl::Delegate::RemoveBlock() {
  DCHECK_EQ(base::PlatformThread::CurrentId(),
            g_power_thread.Pointer()->thread_id());

  if (assertion_ != kIOPMNullAssertionID) {
    IOReturn result = IOPMAssertionRelease(assertion_);
    LOG_IF(ERROR, result != kIOReturnSuccess)
        << "IOPMAssertionRelease: " << result;
  }
}

PowerSaveBlockerImpl::PowerSaveBlockerImpl(PowerSaveBlockerType type,
                                           const std::string& reason)
    : delegate_(new Delegate(type, reason)) {
  g_power_thread.Pointer()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&Delegate::ApplyBlock, delegate_));
}

PowerSaveBlockerImpl::~PowerSaveBlockerImpl() {
  g_power_thread.Pointer()->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&Delegate::RemoveBlock, delegate_));
}

}  // namespace content
