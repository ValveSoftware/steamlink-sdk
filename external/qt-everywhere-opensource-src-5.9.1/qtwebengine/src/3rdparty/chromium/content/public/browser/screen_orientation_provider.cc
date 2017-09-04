// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/screen_orientation_provider.h"

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/screen_orientation_delegate.h"
#include "content/public/browser/screen_orientation_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebLockOrientationError.h"

namespace content {

ScreenOrientationDelegate* ScreenOrientationProvider::delegate_ = nullptr;

ScreenOrientationProvider::LockInformation::LockInformation(int request_id,
    blink::WebScreenOrientationLockType lock)
    : request_id(request_id),
      lock(lock) {
}

ScreenOrientationProvider::ScreenOrientationProvider(
    ScreenOrientationDispatcherHost* dispatcher_host,
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      dispatcher_(dispatcher_host),
      lock_applied_(false) {
}

ScreenOrientationProvider::~ScreenOrientationProvider() {
}

void ScreenOrientationProvider::LockOrientation(int request_id,
    blink::WebScreenOrientationLockType lock_orientation) {

  if (!delegate_ || !delegate_->ScreenOrientationProviderSupported()) {
    dispatcher_->NotifyLockError(request_id,
                                 blink::WebLockOrientationErrorNotAvailable);
    return;
  }

  if (delegate_->FullScreenRequired(web_contents())) {
    RenderViewHostImpl* rvhi =
        static_cast<RenderViewHostImpl*>(web_contents()->GetRenderViewHost());
    if (!rvhi) {
      dispatcher_->NotifyLockError(request_id,
                                   blink::WebLockOrientationErrorCanceled);
      return;
    }
    if (!static_cast<WebContentsImpl*>(web_contents())
             ->IsFullscreenForCurrentTab()) {
      dispatcher_->NotifyLockError(request_id,
          blink::WebLockOrientationErrorFullscreenRequired);
      return;
    }
  }

  if (lock_orientation == blink::WebScreenOrientationLockNatural) {
    lock_orientation = GetNaturalLockType();
    if (lock_orientation == blink::WebScreenOrientationLockDefault) {
      // We are in a broken state, let's pretend we got canceled.
      dispatcher_->NotifyLockError(request_id,
                                   blink::WebLockOrientationErrorCanceled);
      return;
    }
  }

  lock_applied_ = true;
  delegate_->Lock(web_contents(), lock_orientation);

  // If two calls happen close to each other some platforms will ignore the
  // first. A successful lock will be once orientation matches the latest
  // request.
  pending_lock_.reset();

  // If the orientation we are locking to matches the current orientation, we
  // should succeed immediately.
  if (LockMatchesCurrentOrientation(lock_orientation)) {
    dispatcher_->NotifyLockSuccess(request_id);
    return;
  }

  pending_lock_.reset(new LockInformation(request_id, lock_orientation));
}

void ScreenOrientationProvider::UnlockOrientation() {
  if (!lock_applied_ || !delegate_)
    return;

  delegate_->Unlock(web_contents());

  lock_applied_ = false;
  pending_lock_.reset();
}

void ScreenOrientationProvider::OnOrientationChange() {
  if (!pending_lock_.get())
    return;

  if (LockMatchesCurrentOrientation(pending_lock_->lock)) {
    dispatcher_->NotifyLockSuccess(pending_lock_->request_id);
    pending_lock_.reset();
  }
}

void ScreenOrientationProvider::SetDelegate(
    ScreenOrientationDelegate* delegate) {
  delegate_ = delegate;
}

void ScreenOrientationProvider::DidToggleFullscreenModeForTab(
    bool entered_fullscreen, bool will_cause_resize) {
  if (!lock_applied_ || !delegate_)
    return;

  // If fullscreen is not required in order to lock orientation, don't unlock
  // when fullscreen state changes.
  if (!delegate_->FullScreenRequired(web_contents()))
    return;

  DCHECK(!entered_fullscreen);
  UnlockOrientation();
}

blink::WebScreenOrientationLockType
    ScreenOrientationProvider::GetNaturalLockType() const {
  RenderWidgetHost* rwh = web_contents()->GetRenderViewHost()->GetWidget();
  if (!rwh)
    return blink::WebScreenOrientationLockDefault;

  ScreenInfo screen_info;
  rwh->GetScreenInfo(&screen_info);

  switch (screen_info.orientation_type) {
    case SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY:
    case SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY:
      if (screen_info.orientation_angle == 0 ||
          screen_info.orientation_angle == 180) {
        return blink::WebScreenOrientationLockPortraitPrimary;
      }
      return blink::WebScreenOrientationLockLandscapePrimary;
    case SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY:
    case SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY:
      if (screen_info.orientation_angle == 0 ||
          screen_info.orientation_angle == 180) {
        return blink::WebScreenOrientationLockLandscapePrimary;
      }
      return blink::WebScreenOrientationLockPortraitPrimary;
    default:
      break;
  }

  NOTREACHED();
  return blink::WebScreenOrientationLockDefault;
}

bool ScreenOrientationProvider::LockMatchesCurrentOrientation(
    blink::WebScreenOrientationLockType lock) {
  RenderWidgetHost* rwh = web_contents()->GetRenderViewHost()->GetWidget();
  if (!rwh)
    return false;

  ScreenInfo screen_info;
  rwh->GetScreenInfo(&screen_info);

  switch (lock) {
    case blink::WebScreenOrientationLockPortraitPrimary:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
    case blink::WebScreenOrientationLockPortraitSecondary:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY;
    case blink::WebScreenOrientationLockLandscapePrimary:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY;
    case blink::WebScreenOrientationLockLandscapeSecondary:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY;
    case blink::WebScreenOrientationLockLandscape:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY ||
          screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY;
    case blink::WebScreenOrientationLockPortrait:
      return screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY ||
          screen_info.orientation_type ==
          SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY;
    case blink::WebScreenOrientationLockAny:
      return true;
    case blink::WebScreenOrientationLockNatural:
    case blink::WebScreenOrientationLockDefault:
      NOTREACHED();
      return false;
  }

  NOTREACHED();
  return false;
}

} // namespace content
