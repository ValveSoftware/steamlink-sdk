// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SNAPSHOT_SCREENSHOT_GRABBER_OBSERVER_H_
#define UI_SNAPSHOT_SCREENSHOT_GRABBER_OBSERVER_H_

#include "base/files/file_path.h"
#include "ui/snapshot/snapshot_export.h"

namespace ui {

class SNAPSHOT_EXPORT ScreenshotGrabberObserver {
 public:
  enum Result {
    SCREENSHOT_SUCCESS = 0,
    SCREENSHOT_GRABWINDOW_PARTIAL_FAILED,
    SCREENSHOT_GRABWINDOW_FULL_FAILED,
    SCREENSHOT_CREATE_DIR_FAILED,
    SCREENSHOT_GET_DIR_FAILED,
    SCREENSHOT_CHECK_DIR_FAILED,
    SCREENSHOT_CREATE_FILE_FAILED,
    SCREENSHOT_WRITE_FILE_FAILED,
    SCREENSHOTS_DISABLED,
    SCREENSHOT_RESULT_COUNT
  };

  // Dispatched after attempting to take a screenshot with the |result| and
  // |screenshot_path| of the taken screenshot (if successful).
  virtual void OnScreenshotCompleted(Result screenshot_result,
                                     const base::FilePath& screenshot_path) = 0;

 protected:
  virtual ~ScreenshotGrabberObserver() {}
};

}  // namespace ui

#endif  // UI_SNAPSHOT_SCREENSHOT_GRABBER_OBSERVER_H_
