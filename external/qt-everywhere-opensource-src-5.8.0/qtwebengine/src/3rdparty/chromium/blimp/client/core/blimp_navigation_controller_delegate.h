// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_BLIMP_NAVIGATION_CONTROLLER_DELEGATE_H_
#define BLIMP_CLIENT_CORE_BLIMP_NAVIGATION_CONTROLLER_DELEGATE_H_

#include "base/macros.h"
#include "url/gurl.h"

namespace blimp {
namespace client {

// Interface for objects embedding a BlimpNavigationControllerImpl to provide
// the functionality BlimpNavigationControllerImpl needs.
class BlimpNavigationControllerDelegate {
 public:
  virtual ~BlimpNavigationControllerDelegate() = default;

  // Invoked whenever a new URL has been loaded.
  virtual void NotifyURLLoaded(const GURL& url) = 0;

 protected:
  BlimpNavigationControllerDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpNavigationControllerDelegate);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_BLIMP_NAVIGATION_CONTROLLER_DELEGATE_H_
