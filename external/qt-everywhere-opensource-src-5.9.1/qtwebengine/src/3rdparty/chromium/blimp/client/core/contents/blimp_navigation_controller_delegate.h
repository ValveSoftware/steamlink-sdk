// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_NAVIGATION_CONTROLLER_DELEGATE_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_NAVIGATION_CONTROLLER_DELEGATE_H_

#include "base/macros.h"
#include "url/gurl.h"

namespace blimp {
namespace client {

// Interface for objects embedding a BlimpNavigationControllerImpl to provide
// the functionality BlimpNavigationControllerImpl needs.
class BlimpNavigationControllerDelegate {
 public:
  virtual ~BlimpNavigationControllerDelegate() = default;

  // TODO(dtrainor): Pull apart this method into more fine grained notifications
  // or add an enum to detect exactly which state changed.
  // Informs the delegate that navigation state has changed.
  virtual void OnNavigationStateChanged() = 0;

  // Informs the delegate that navigation loading has started or stopped.
  virtual void OnLoadingStateChanged(bool loading) = 0;

  // Informs the delegate that page loading has started or stopped.
  virtual void OnPageLoadingStateChanged(bool loading) = 0;

 protected:
  BlimpNavigationControllerDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpNavigationControllerDelegate);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_NAVIGATION_CONTROLLER_DELEGATE_H_
