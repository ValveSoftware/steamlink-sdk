// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_POWER_SAVE_BLOCK_RESOURCE_THROTTLE_H_
#define CONTENT_BROWSER_LOADER_POWER_SAVE_BLOCK_RESOURCE_THROTTLE_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "content/public/browser/resource_throttle.h"

namespace device {
class PowerSaveBlocker;
}  // namespace device

namespace content {

// This ResourceThrottle blocks power save until large upload request finishes.
class PowerSaveBlockResourceThrottle : public ResourceThrottle {
 public:
  PowerSaveBlockResourceThrottle(
      const std::string& host,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner);
  ~PowerSaveBlockResourceThrottle() override;

  // ResourceThrottle overrides:
  void WillStartRequest(bool* defer) override;
  void WillProcessResponse(bool* defer) override;
  const char* GetNameForLogging() const override;

 private:
  void ActivatePowerSaveBlocker();

  const std::string host_;
  base::OneShotTimer timer_;
  std::unique_ptr<device::PowerSaveBlocker> power_save_blocker_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> blocking_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PowerSaveBlockResourceThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_POWER_SAVE_BLOCK_RESOURCE_THROTTLE_H_
