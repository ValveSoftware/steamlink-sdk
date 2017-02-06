// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_GLES2_COMMAND_BUFFER_LOCAL_CLIENT_H_
#define COMPONENTS_MUS_GLES2_COMMAND_BUFFER_LOCAL_CLIENT_H_

#include <stdint.h>

#include "ui/gfx/swap_result.h"

namespace base {
class TimeDelta;
class TimeTicks;
}

namespace mus {

class CommandBufferLocalClient {
 public:
  virtual void UpdateVSyncParameters(const base::TimeTicks& timebase,
                                     const base::TimeDelta& interval) = 0;
  virtual void GpuCompletedSwapBuffers(gfx::SwapResult result) = 0;

 protected:
  virtual ~CommandBufferLocalClient() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_GLES2_COMMAND_BUFFER_LOCAL_CLIENT_H_
