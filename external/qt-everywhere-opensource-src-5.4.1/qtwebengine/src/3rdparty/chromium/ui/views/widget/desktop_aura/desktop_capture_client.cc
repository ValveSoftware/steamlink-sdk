// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_capture_client.h"

#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"

namespace views {

// static
DesktopCaptureClient::CaptureClients*
DesktopCaptureClient::capture_clients_ = NULL;

DesktopCaptureClient::DesktopCaptureClient(aura::Window* root)
    : root_(root),
      capture_window_(NULL) {
  if (!capture_clients_)
    capture_clients_ = new CaptureClients;
  capture_clients_->insert(this);
  aura::client::SetCaptureClient(root, this);
}

DesktopCaptureClient::~DesktopCaptureClient() {
  aura::client::SetCaptureClient(root_, NULL);
  capture_clients_->erase(this);
}

void DesktopCaptureClient::SetCapture(aura::Window* new_capture_window) {
  if (capture_window_ == new_capture_window)
    return;

  // We should only ever be told to capture a child of |root_|. Otherwise
  // things are going to be really confused.
  DCHECK(!new_capture_window ||
         (new_capture_window->GetRootWindow() == root_));
  DCHECK(!capture_window_ || capture_window_->GetRootWindow());

  aura::Window* old_capture_window = capture_window_;

  // If we're actually starting capture, then cancel any touches/gestures
  // that aren't already locked to the new window, and transfer any on the
  // old capture window to the new one.  When capture is released we have no
  // distinction between the touches/gestures that were in the window all
  // along (and so shouldn't be canceled) and those that got moved, so
  // just leave them all where they are.
  if (new_capture_window) {
    ui::GestureRecognizer::Get()->TransferEventsTo(old_capture_window,
        new_capture_window);
  }

  capture_window_ = new_capture_window;

  aura::client::CaptureDelegate* delegate = root_->GetHost()->dispatcher();
  delegate->UpdateCapture(old_capture_window, new_capture_window);

  // Initiate native capture updating.
  if (!capture_window_) {
    delegate->ReleaseNativeCapture();
  } else if (!old_capture_window) {
    delegate->SetNativeCapture();

    // Notify the other roots that we got capture. This is important so that
    // they reset state.
    CaptureClients capture_clients(*capture_clients_);
    for (CaptureClients::iterator i = capture_clients.begin();
         i != capture_clients.end(); ++i) {
      if (*i != this) {
        aura::client::CaptureDelegate* delegate =
            (*i)->root_->GetHost()->dispatcher();
        delegate->OnOtherRootGotCapture();
      }
    }
  }  // else case is capture is remaining in our root, nothing to do.
}

void DesktopCaptureClient::ReleaseCapture(aura::Window* window) {
  if (capture_window_ != window)
    return;
  SetCapture(NULL);
}

aura::Window* DesktopCaptureClient::GetCaptureWindow() {
  return capture_window_;
}

aura::Window* DesktopCaptureClient::GetGlobalCaptureWindow() {
  for (CaptureClients::iterator i = capture_clients_->begin();
       i != capture_clients_->end(); ++i) {
    if ((*i)->capture_window_)
      return (*i)->capture_window_;
  }
  return NULL;
}

}  // namespace views
