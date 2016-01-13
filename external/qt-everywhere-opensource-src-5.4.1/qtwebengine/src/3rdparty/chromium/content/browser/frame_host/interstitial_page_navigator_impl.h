// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_INTERSTITIAL_PAGE_NAVIGATOR_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_INTERSTITIAL_PAGE_NAVIGATOR_IMPL_H_

#include "base/memory/ref_counted.h"
#include "content/browser/frame_host/navigator.h"
#include "content/common/content_export.h"

namespace content {

class NavigationControllerImpl;
class InterstitialPageImpl;

// Navigator implementation specific to InterstialPageImpl. It allows only one
// navigation to commit, since interstitial pages are not allowed to navigate.
class CONTENT_EXPORT InterstitialPageNavigatorImpl : public Navigator {
 public:
  InterstitialPageNavigatorImpl(
      InterstitialPageImpl* interstitial,
      NavigationControllerImpl* navigation_controller);

  virtual NavigationController* GetController() OVERRIDE;
  virtual void DidNavigate(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidCommitProvisionalLoad_Params&
          input_params) OVERRIDE;

 private:
  virtual ~InterstitialPageNavigatorImpl() {}

  // The InterstitialPage with which this navigator object is associated.
  // Non owned pointer.
  InterstitialPageImpl* interstitial_;

  // The NavigationController associated with this navigator.
  NavigationControllerImpl* controller_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialPageNavigatorImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_INTERSTITIAL_PAGE_NAVIGATOR_IMPL_H_
