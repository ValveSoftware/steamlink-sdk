// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SNAPSHOT_SCREENSHOT_GRABBER_H_
#define UI_SNAPSHOT_SCREENSHOT_GRABBER_H_

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/snapshot/screenshot_grabber_observer.h"
#include "ui/snapshot/snapshot_export.h"

namespace base {
class TaskRunner;
}

namespace ui {

// TODO(flackr): Componentize google drive so that we don't need the
// ScreenshotGrabberDelegate.
class SNAPSHOT_EXPORT ScreenshotGrabberDelegate {
 public:
  enum FileResult {
    FILE_SUCCESS,
    FILE_CHECK_DIR_FAILED,
    FILE_CREATE_DIR_FAILED,
    FILE_CREATE_FAILED
  };

  // Callback called with the |result| of trying to create a local writable
  // |path| for the possibly remote path.
  using FileCallback =
      base::Callback<void(FileResult result, const base::FilePath& path)>;

  ScreenshotGrabberDelegate() {}
  virtual ~ScreenshotGrabberDelegate() {}

  // Prepares a writable file for |path|. If |path| is a non-local path (i.e.
  // Google drive) and it is supported this will create a local cached copy of
  // the remote file and call the callback with the local path.
  virtual void PrepareFileAndRunOnBlockingPool(
      const base::FilePath& path,
      scoped_refptr<base::TaskRunner> blocking_task_runner,
      const FileCallback& callback_on_blocking_pool);
};

class SNAPSHOT_EXPORT ScreenshotGrabber {
 public:
  ScreenshotGrabber(ScreenshotGrabberDelegate* client,
                    scoped_refptr<base::TaskRunner> blocking_task_runner);
  ~ScreenshotGrabber();

  // Takes a screenshot of |rect| in |window| in that window's coordinate space
  // saving the result to |screenshot_path|.
  void TakeScreenshot(gfx::NativeWindow window,
                      const gfx::Rect& rect,
                      const base::FilePath& screenshot_path);
  bool CanTakeScreenshot();

  void NotifyScreenshotCompleted(
      ScreenshotGrabberObserver::Result screenshot_result,
      const base::FilePath& screenshot_path);

  void AddObserver(ScreenshotGrabberObserver* observer);
  void RemoveObserver(ScreenshotGrabberObserver* observer);
  bool HasObserver(const ScreenshotGrabberObserver* observer) const;

 private:
  void GrabWindowSnapshotAsyncCallback(
      const std::string& window_identifier,
      base::FilePath screenshot_path,
      bool is_partial,
      scoped_refptr<base::RefCountedBytes> png_data);

  // A weak pointer to the screenshot taker client.
  ScreenshotGrabberDelegate* client_;

  // The timestamp when the screenshot task was issued last time.
  base::TimeTicks last_screenshot_timestamp_;

  // Task runner for blocking tasks.
  scoped_refptr<base::TaskRunner> blocking_task_runner_;

  base::ObserverList<ScreenshotGrabberObserver> observers_;
  base::WeakPtrFactory<ScreenshotGrabber> factory_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotGrabber);
};

}  // namespace ui

#endif  // UI_SNAPSHOT_SCREENSHOT_GRABBER_H_
