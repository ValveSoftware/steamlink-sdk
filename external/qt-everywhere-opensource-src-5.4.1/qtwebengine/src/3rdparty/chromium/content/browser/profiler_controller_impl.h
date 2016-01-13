// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PROFILER_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PROFILER_CONTROLLER_IMPL_H_

#include "base/memory/singleton.h"
#include "base/process/process.h"
#include "content/common/content_export.h"
#include "content/public/browser/profiler_controller.h"
#include "content/public/common/process_type.h"

namespace tracked_objects {
struct ProcessDataSnapshot;
}

namespace content {

// ProfilerController's implementation.
class ProfilerControllerImpl : public ProfilerController {
 public:
  static ProfilerControllerImpl* GetInstance();

  // Normally instantiated when the child process is launched. Only one instance
  // should be created per process.
  ProfilerControllerImpl();
  virtual ~ProfilerControllerImpl();

  // Notify the |subscriber_| that it should expect at least |pending_processes|
  // additional calls to OnProfilerDataCollected().  OnPendingProcess() may be
  // called repeatedly; the last call will have |end| set to true, indicating
  // that there is no longer a possibility for the count of pending processes to
  // increase.  This is called on the UI thread.
  void OnPendingProcesses(int sequence_number, int pending_processes, bool end);

  // Send the |profiler_data| back to the |subscriber_|.
  // This can be called from any thread.
  void OnProfilerDataCollected(
      int sequence_number,
      const tracked_objects::ProcessDataSnapshot& profiler_data,
      int process_type);

  // ProfilerController implementation:
  virtual void Register(ProfilerSubscriber* subscriber) OVERRIDE;
  virtual void Unregister(const ProfilerSubscriber* subscriber) OVERRIDE;
  virtual void GetProfilerData(int sequence_number) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<ProfilerControllerImpl>;

  // Contact child processes and get their profiler data.
  void GetProfilerDataFromChildProcesses(int sequence_number);

  ProfilerSubscriber* subscriber_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PROFILER_CONTROLLER_IMPL_H_
