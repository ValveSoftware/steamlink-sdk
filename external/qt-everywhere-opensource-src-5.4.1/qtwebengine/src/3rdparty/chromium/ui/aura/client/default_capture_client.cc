// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/default_capture_client.h"

#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
namespace client {

DefaultCaptureClient::DefaultCaptureClient(Window* root_window)
    : root_window_(root_window),
      capture_window_(NULL) {
  SetCaptureClient(root_window_, this);
}

DefaultCaptureClient::~DefaultCaptureClient() {
  SetCaptureClient(root_window_, NULL);
}

void DefaultCaptureClient::SetCapture(Window* window) {
  if (capture_window_ == window)
    return;
  if (window) {
    ui::GestureRecognizer::Get()->TransferEventsTo(
        capture_window_, window);
  }

  Window* old_capture_window = capture_window_;
  capture_window_ = window;

  CaptureDelegate* capture_delegate = root_window_->GetHost()->dispatcher();
  if (capture_window_)
    capture_delegate->SetNativeCapture();
  else
    capture_delegate->ReleaseNativeCapture();

  capture_delegate->UpdateCapture(old_capture_window, capture_window_);
}

void DefaultCaptureClient::ReleaseCapture(Window* window) {
  if (capture_window_ != window)
    return;
  SetCapture(NULL);
}

Window* DefaultCaptureClient::GetCaptureWindow() {
  return capture_window_;
}

Window* DefaultCaptureClient::GetGlobalCaptureWindow() {
  return capture_window_;
}

}  // namespace client
}  // namespace aura
