// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SYNC_CONTROL_VSYNC_PROVIDER_H_
#define UI_GL_SYNC_CONTROL_VSYNC_PROVIDER_H_

#include <stdint.h>

#include <queue>

#include "base/macros.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_export.h"

namespace gl {

// Base class for providers based on extensions like GLX_OML_sync_control and
// EGL_CHROMIUM_sync_control.
class GL_EXPORT SyncControlVSyncProvider : public gfx::VSyncProvider {
 public:
  SyncControlVSyncProvider();
  ~SyncControlVSyncProvider() override;

  void GetVSyncParameters(const UpdateVSyncCallback& callback) override;

 protected:
  virtual bool GetSyncValues(int64_t* system_time,
                             int64_t* media_stream_counter,
                             int64_t* swap_buffer_counter) = 0;

  virtual bool GetMscRate(int32_t* numerator, int32_t* denominator) = 0;

 private:
#if defined(OS_LINUX) || defined(OS_WIN)
  bool AdjustSyncValues(int64_t* system_time, int64_t* media_stream_counter);

  base::TimeTicks last_timebase_;
  base::TimeDelta last_timebase_diff_;
  int64_t last_media_stream_counter_ = 0;
  int64_t last_counter_diff_ = 0;
  base::TimeDelta last_good_interval_;

  // A short history of the last few computed intervals.
  // We use this to filter out the noise in the computation resulting
  // from configuration change (monitor reconfiguration, moving windows
  // between monitors, suspend and resume, etc.).
  std::queue<base::TimeDelta> last_computed_intervals_;
#endif  //  defined(OS_LINUX) || defined(OS_WIN)

#if defined(OS_LINUX)
  bool invalid_msc_ = false;
#endif  // defined(OS_LINUX)

  DISALLOW_COPY_AND_ASSIGN(SyncControlVSyncProvider);
};

}  // namespace gl

#endif  // UI_GL_SYNC_CONTROL_VSYNC_PROVIDER_H_
