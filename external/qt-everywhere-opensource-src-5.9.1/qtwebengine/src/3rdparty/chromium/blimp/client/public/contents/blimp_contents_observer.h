// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_OBSERVER_H_
#define BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_OBSERVER_H_

#include "base/macros.h"
#include "url/gurl.h"

namespace blimp {
namespace client {

class BlimpContents;

// An observer API implemented by classes which are interested in various events
// related to BlimpContents.
class BlimpContentsObserver {
 public:
  virtual ~BlimpContentsObserver();

  // Invoked when the navigation state of the BlimpContents has changed.
  virtual void OnNavigationStateChanged() {}

  // Invoked when the BlimpContents starts or stops loading resources.
  virtual void OnLoadingStateChanged(bool loading) {}

  // Invoked when the BlimpContents starts or stops loading a page.
  virtual void OnPageLoadingStateChanged(bool loading) {}

  // Called by BlimpContentsDying().
  virtual void OnContentsDestroyed() {}

  // Invoke when the destructor of blimp contents is called. This will clear
  // the contents_ to nullptr.
  void BlimpContentsDying();

  BlimpContents* blimp_contents() { return contents_; }

 protected:
  explicit BlimpContentsObserver(BlimpContents* blimp_contents);

 private:
  // The BlimpContents being tracked by this BlimpContentsObserver.
  BlimpContents* contents_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsObserver);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_PUBLIC_CONTENTS_BLIMP_CONTENTS_OBSERVER_H_
