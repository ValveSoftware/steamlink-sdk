// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HISTOGRAM_CONTROLLER_H_
#define CONTENT_BROWSER_HISTOGRAM_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/singleton.h"

namespace content {

class HistogramSubscriber;

// HistogramController is used on the browser process to collect histogram data.
// Only the browser UI thread is allowed to interact with the
// HistogramController object.
class HistogramController {
 public:
  // Returns the HistogramController object for the current process, or NULL if
  // none.
  static HistogramController* GetInstance();

  // Normally instantiated when the child process is launched. Only one instance
  // should be created per process.
  HistogramController();
  virtual ~HistogramController();

  // Register the subscriber so that it will be called when for example
  // OnHistogramDataCollected is returning histogram data from a child process.
  // This is called on UI thread.
  void Register(HistogramSubscriber* subscriber);

  // Unregister the subscriber so that it will not be called when for example
  // OnHistogramDataCollected is returning histogram data from a child process.
  // Safe to call even if caller is not the current subscriber.
  void Unregister(const HistogramSubscriber* subscriber);

  // Contact all processes and get their histogram data.
  void GetHistogramData(int sequence_number);

  // Notify the |subscriber_| that it should expect at least |pending_processes|
  // additional calls to OnHistogramDataCollected().  OnPendingProcess() may be
  // called repeatedly; the last call will have |end| set to true, indicating
  // that there is no longer a possibility for the count of pending processes to
  // increase.  This is called on the UI thread.
  void OnPendingProcesses(int sequence_number, int pending_processes, bool end);

  // Send the |histogram| back to the |subscriber_|.
  // This can be called from any thread.
  void OnHistogramDataCollected(
      int sequence_number,
      const std::vector<std::string>& pickled_histograms);

 private:
  friend struct base::DefaultSingletonTraits<HistogramController>;

  // Contact PLUGIN and GPU child processes and get their histogram data.
  // TODO(rtenneti): Enable getting histogram data for other processes like
  // PPAPI and NACL.
  void GetHistogramDataFromChildProcesses(int sequence_number);

  HistogramSubscriber* subscriber_;

  DISALLOW_COPY_AND_ASSIGN(HistogramController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_HISTOGRAM_CONTROLLER_H_
