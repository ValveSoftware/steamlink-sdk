// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_MESSAGE_FILTER_ANDROID_H_
#define CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_MESSAGE_FILTER_ANDROID_H_

#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class ScreenOrientationMessageFilterAndroid : public BrowserMessageFilter {
 public:
  ScreenOrientationMessageFilterAndroid();

  // BrowserMessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  ~ScreenOrientationMessageFilterAndroid() override;

  void OnStartListening();
  void OnStopListening();

  int listeners_count_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationMessageFilterAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_MESSAGE_FILTER_ANDROID_H_
