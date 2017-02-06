// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_PUBLIC_BLIMP_NAVIGATION_CONTROLLER_H_
#define BLIMP_CLIENT_CORE_PUBLIC_BLIMP_NAVIGATION_CONTROLLER_H_

#include "base/macros.h"
#include "url/gurl.h"

namespace blimp {
namespace client {

class BlimpContentsObserver;

// The BlimpNavigationController maintains the back-forward list for a
// BlimpContents and manages all navigation within that list.
//
// Each BlimpNavigationController belongs to one BlimpContents, and each
// BlimpContents has exactly one BlimpNavigationController.
class BlimpNavigationController {
 public:
  virtual ~BlimpNavigationController() = default;

  // Calls to LoadURL informs the engine that it should start navigating to the
  // provided |url|.
  virtual void LoadURL(const GURL& url) = 0;

  // Retrieves the URL of the currently selected item in the navigation list.
  virtual const GURL& GetURL() = 0;

 protected:
  BlimpNavigationController() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpNavigationController);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_PUBLIC_BLIMP_NAVIGATION_CONTROLLER_H_
