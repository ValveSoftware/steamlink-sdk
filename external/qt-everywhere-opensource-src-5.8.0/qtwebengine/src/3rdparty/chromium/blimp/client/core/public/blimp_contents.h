// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_PUBLIC_BLIMP_CONTENTS_H_
#define BLIMP_CLIENT_CORE_PUBLIC_BLIMP_CONTENTS_H_

#include "base/macros.h"
#include "url/gurl.h"

namespace blimp {
namespace client {

class BlimpContentsObserver;
class BlimpNavigationController;

// BlimpContents is the core class in //blimp/client/core. It renders web pages
// from an engine in a rectangular area.
// It enables callers to control the blimp engine through the use of the
// navigation controller.
class BlimpContents {
 public:
  virtual ~BlimpContents() = default;

  // Retrives the navigation controller that controls all navigation related
  // to this BlimpContents.
  virtual BlimpNavigationController& GetNavigationController() = 0;

  // Enables adding and removing observers to this BlimpContents.
  virtual void AddObserver(BlimpContentsObserver* observer) = 0;
  virtual void RemoveObserver(BlimpContentsObserver* observer) = 0;

 protected:
  BlimpContents() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpContents);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_PUBLIC_BLIMP_CONTENTS_H_
