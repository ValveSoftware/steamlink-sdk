// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser_tests/blimp_contents_view_readback_helper.h"

#include "base/bind.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blimp {

BlimpContentsViewReadbackHelper::BlimpContentsViewReadbackHelper()
    : event_(base::WaitableEvent::ResetPolicy::MANUAL,
             base::WaitableEvent::InitialState::NOT_SIGNALED),
      weak_ptr_factory_(this) {}

BlimpContentsViewReadbackHelper::~BlimpContentsViewReadbackHelper() = default;

client::BlimpContentsView::ReadbackRequestCallback
BlimpContentsViewReadbackHelper::GetCallback() {
  return base::Bind(&BlimpContentsViewReadbackHelper::OnReadbackComplete,
                    weak_ptr_factory_.GetWeakPtr());
}

SkBitmap* BlimpContentsViewReadbackHelper::GetBitmap() const {
  return bitmap_.get();
}

void BlimpContentsViewReadbackHelper::OnReadbackComplete(
    std::unique_ptr<SkBitmap> bitmap) {
  bitmap_ = std::move(bitmap);
  event_.Signal();
}

}  // namespace blimp
