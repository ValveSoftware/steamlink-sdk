// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/power_save_blocker_impl.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class PowerSaveBlockerImpl::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlockerImpl::Delegate> {
 public:
  Delegate(PowerSaveBlockerType type, const std::string& reason)
      : type_(type),
        reason_(reason),
        block_id_(0) {}

  void ApplyBlock() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!chromeos::DBusThreadManager::IsInitialized())
      return;

    chromeos::PowerPolicyController* controller =
        chromeos::DBusThreadManager::Get()->GetPowerPolicyController();
    switch (type_) {
      case kPowerSaveBlockPreventAppSuspension:
        block_id_ = controller->AddSystemWakeLock(reason_);
        break;
      case kPowerSaveBlockPreventDisplaySleep:
        block_id_ = controller->AddScreenWakeLock(reason_);
        break;
      default:
        NOTREACHED() << "Unhandled block type " << type_;
    }
  }

  void RemoveBlock() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!chromeos::DBusThreadManager::IsInitialized())
      return;

    chromeos::DBusThreadManager::Get()->GetPowerPolicyController()->
        RemoveWakeLock(block_id_);
  }

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  virtual ~Delegate() {}

  PowerSaveBlockerType type_;
  std::string reason_;

  // ID corresponding to the block request in PowerPolicyController.
  int block_id_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

PowerSaveBlockerImpl::PowerSaveBlockerImpl(PowerSaveBlockerType type,
                                           const std::string& reason)
    : delegate_(new Delegate(type, reason)) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&Delegate::ApplyBlock, delegate_));
}

PowerSaveBlockerImpl::~PowerSaveBlockerImpl() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&Delegate::RemoveBlock, delegate_));
}

}  // namespace content
