// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_POWER_SAVE_BLOCK_RESOURCE_THROTTLE_H_
#define CONTENT_BROWSER_LOADER_POWER_SAVE_BLOCK_RESOURCE_THROTTLE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "content/public/browser/resource_throttle.h"

namespace content {

class PowerSaveBlocker;

// This ResourceThrottle blocks power save until large upload request finishes.
class PowerSaveBlockResourceThrottle : public ResourceThrottle {
 public:
  PowerSaveBlockResourceThrottle();
  virtual ~PowerSaveBlockResourceThrottle();

  // ResourceThrottle overrides:
  virtual void WillStartRequest(bool* defer) OVERRIDE;
  virtual void WillProcessResponse(bool* defer) OVERRIDE;
  virtual const char* GetNameForLogging() const OVERRIDE;

 private:
  void ActivatePowerSaveBlocker();

  base::OneShotTimer<PowerSaveBlockResourceThrottle> timer_;
  scoped_ptr<PowerSaveBlocker> power_save_blocker_;

  DISALLOW_COPY_AND_ASSIGN(PowerSaveBlockResourceThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_POWER_SAVE_BLOCK_RESOURCE_THROTTLE_H_
