// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_VIBRATION_VIBRATION_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_VIBRATION_VIBRATION_MESSAGE_FILTER_H_

#include "content/public/browser/browser_message_filter.h"

namespace content {

class VibrationProvider;

// VibrationMessageFilter is a browser filter for Vibration messages.
class VibrationMessageFilter : public BrowserMessageFilter {
 public:
  VibrationMessageFilter();

 private:
  virtual ~VibrationMessageFilter();
  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnVibrate(int64 milliseconds);
  void OnCancelVibration();
  static VibrationProvider* CreateProvider();

  scoped_ptr<VibrationProvider> provider_;
  DISALLOW_COPY_AND_ASSIGN(VibrationMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_VIBRATION_VIBRATION_MESSAGE_FILTER_H_
