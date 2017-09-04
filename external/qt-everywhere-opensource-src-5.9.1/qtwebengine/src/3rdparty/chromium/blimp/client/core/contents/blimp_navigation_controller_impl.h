// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_NAVIGATION_CONTROLLER_IMPL_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_NAVIGATION_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "blimp/client/core/contents/navigation_feature.h"
#include "blimp/client/public/contents/blimp_navigation_controller.h"
#include "url/gurl.h"

class SkBitmap;

namespace blimp {
namespace client {

class BlimpContentsObserver;
class BlimpNavigationControllerDelegate;

class BlimpNavigationControllerImpl
    : public BlimpNavigationController,
      public NavigationFeature::NavigationFeatureDelegate {
 public:
  // The life time of |feature| must remain valid throughout |this|.
  BlimpNavigationControllerImpl(int blimp_contents_id,
                                BlimpNavigationControllerDelegate* delegate,
                                NavigationFeature* feature);
  ~BlimpNavigationControllerImpl() override;

  // BlimpNavigationController implementation.
  void LoadURL(const GURL& url) override;
  void Reload() override;
  bool CanGoBack() const override;
  bool CanGoForward() const override;
  void GoBack() override;
  void GoForward() override;
  const GURL& GetURL() override;
  const std::string& GetTitle() override;

 private:
  // NavigationFeatureDelegate implementation.
  void OnUrlChanged(int tab_id, const GURL& url) override;
  void OnFaviconChanged(int tab_id, const SkBitmap& favicon) override;
  void OnTitleChanged(int tab_id, const std::string& title) override;
  void OnLoadingChanged(int tab_id, bool loading) override;
  void OnPageLoadStatusUpdate(int tab_id, bool completed) override;

  // The ID for the BlimpContentsImpl this is a controller for.
  int blimp_contents_id_;

  // The |navigation_feature_| is responsible for sending and receiving the
  // navigation state to the server over the proto channel.
  NavigationFeature* navigation_feature_;

  // The delegate contains functionality required for the navigation controller
  // to function correctly. It is also invoked on relevant state changes of the
  // navigation controller.
  BlimpNavigationControllerDelegate* delegate_;

  // The currently active URL.
  GURL current_url_;

  // Title of the currently active page.
  std::string current_title_;

  DISALLOW_COPY_AND_ASSIGN(BlimpNavigationControllerImpl);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_NAVIGATION_CONTROLLER_IMPL_H_
