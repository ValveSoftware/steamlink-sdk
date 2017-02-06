// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_BLIMP_NAVIGATION_CONTROLLER_IMPL_H_
#define BLIMP_CLIENT_CORE_BLIMP_NAVIGATION_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "blimp/client/core/public/blimp_navigation_controller.h"
#include "url/gurl.h"

namespace blimp {
namespace client {

class BlimpContentsObserver;
class BlimpNavigationControllerDelegate;

// BlimpContentsImpl is the implementation of the core class in
// //blimp/client/core, the BlimpContents.
// The BlimpNavigationControllerImpl is the implementation part of
// BlimpNavigationController. It maintains the back-forward list for a
// BlimpContentsImpl and manages all navigation within that list.
class BlimpNavigationControllerImpl : public BlimpNavigationController {
 public:
  explicit BlimpNavigationControllerImpl(
      BlimpNavigationControllerDelegate* delegate);
  ~BlimpNavigationControllerImpl() override;

  // BlimpNavigationController implementation.
  void LoadURL(const GURL& url) override;
  const GURL& GetURL() override;

 private:
  void NotifyDelegateURLLoaded(const GURL& url);

  // The delegate contains functionality required for the navigation controller
  // to function correctly. It is also invoked on relevant state changes of the
  // navigation controller.
  BlimpNavigationControllerDelegate* delegate_;

  // The currently active URL.
  GURL current_url_;

  // TODO(shaktisahu): Remove this after integration with NavigationFeature.
  base::WeakPtrFactory<BlimpNavigationControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpNavigationControllerImpl);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_BLIMP_NAVIGATION_CONTROLLER_IMPL_H_
