// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_BROWSER_TESTS_BLIMP_CONTENTS_VIEW_READBACK_HELPER_H_
#define BLIMP_ENGINE_BROWSER_TESTS_BLIMP_CONTENTS_VIEW_READBACK_HELPER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "blimp/client/public/contents/blimp_contents_view.h"

namespace blimp {

// A helper class to make grabbing a bitmap from a BlimpContentsView easier.
// This can be used in conjunction with a WaitableContentPump to pause test
// execution while waiting for the readback to finish.  An example use would be:
//
// WaitableContentPump pump;
// BlimpContentsViewReadbackHelper helper;
// blimp_contents->GetView()->CopyCompositingSurface(
//    helper.GetCallback(), true);
// pump.WaitFor(helper.event());
// helper.bitmap();

class BlimpContentsViewReadbackHelper {
 public:
  BlimpContentsViewReadbackHelper();
  virtual ~BlimpContentsViewReadbackHelper();

  client::BlimpContentsView::ReadbackRequestCallback GetCallback();
  SkBitmap* GetBitmap() const;

  base::WaitableEvent* event() { return &event_; }

 private:
  void OnReadbackComplete(std::unique_ptr<SkBitmap> bitmap);

  base::WaitableEvent event_;
  std::unique_ptr<SkBitmap> bitmap_;

  base::WeakPtrFactory<BlimpContentsViewReadbackHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsViewReadbackHelper);
};

}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_TESTS_BLIMP_CONTENTS_VIEW_READBACK_HELPER_H_
