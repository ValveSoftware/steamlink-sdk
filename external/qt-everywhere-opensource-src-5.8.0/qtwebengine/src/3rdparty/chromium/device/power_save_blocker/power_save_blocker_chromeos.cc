// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/power_save_blocker/power_save_blocker.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/dbus/power_policy_controller.h"

namespace device {

namespace {

// Converts a PowerSaveBlocker::Reason to a
// chromeos::PowerPolicyController::WakeLockReason.
chromeos::PowerPolicyController::WakeLockReason GetWakeLockReason(
    PowerSaveBlocker::Reason reason) {
  switch (reason) {
    case PowerSaveBlocker::kReasonAudioPlayback:
      return chromeos::PowerPolicyController::REASON_AUDIO_PLAYBACK;
    case PowerSaveBlocker::kReasonVideoPlayback:
      return chromeos::PowerPolicyController::REASON_VIDEO_PLAYBACK;
    case PowerSaveBlocker::kReasonOther:
      return chromeos::PowerPolicyController::REASON_OTHER;
  }
  return chromeos::PowerPolicyController::REASON_OTHER;
}

}  // namespace

class PowerSaveBlocker::Delegate
    : public base::RefCountedThreadSafe<PowerSaveBlocker::Delegate> {
 public:
  Delegate(PowerSaveBlockerType type,
           Reason reason,
           const std::string& description,
           scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
      : type_(type),
        reason_(reason),
        description_(description),
        block_id_(0),
        ui_task_runner_(ui_task_runner) {}

  void ApplyBlock() {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    if (!chromeos::PowerPolicyController::IsInitialized())
      return;

    auto* controller = chromeos::PowerPolicyController::Get();
    switch (type_) {
      case kPowerSaveBlockPreventAppSuspension:
        block_id_ = controller->AddSystemWakeLock(GetWakeLockReason(reason_),
                                                  description_);
        break;
      case kPowerSaveBlockPreventDisplaySleep:
        block_id_ = controller->AddScreenWakeLock(GetWakeLockReason(reason_),
                                                  description_);
        break;
      default:
        NOTREACHED() << "Unhandled block type " << type_;
    }
  }

  void RemoveBlock() {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    if (!chromeos::PowerPolicyController::IsInitialized())
      return;

    chromeos::PowerPolicyController::Get()->RemoveWakeLock(block_id_);
  }

 private:
  friend class base::RefCountedThreadSafe<Delegate>;
  virtual ~Delegate() {}

  PowerSaveBlockerType type_;
  Reason reason_;
  std::string description_;

  // ID corresponding to the block request in PowerPolicyController.
  int block_id_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

PowerSaveBlocker::PowerSaveBlocker(
    PowerSaveBlockerType type,
    Reason reason,
    const std::string& description,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner)
    : delegate_(new Delegate(type, reason, description, ui_task_runner)),
      ui_task_runner_(ui_task_runner),
      blocking_task_runner_(blocking_task_runner) {
  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&Delegate::ApplyBlock, delegate_));
}

PowerSaveBlocker::~PowerSaveBlocker() {
  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&Delegate::RemoveBlock, delegate_));
}

}  // namespace device
