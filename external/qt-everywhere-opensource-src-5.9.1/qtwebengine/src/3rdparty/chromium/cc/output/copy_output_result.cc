// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/copy_output_result.h"

#include "base/logging.h"
#include "cc/resources/texture_mailbox.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

CopyOutputResult::CopyOutputResult() {}

CopyOutputResult::CopyOutputResult(std::unique_ptr<SkBitmap> bitmap)
    : size_(bitmap->width(), bitmap->height()), bitmap_(std::move(bitmap)) {
  DCHECK(bitmap_);
}

CopyOutputResult::CopyOutputResult(
    const gfx::Size& size,
    const TextureMailbox& texture_mailbox,
    std::unique_ptr<SingleReleaseCallback> release_callback)
    : size_(size),
      texture_mailbox_(texture_mailbox),
      release_callback_(std::move(release_callback)) {
  DCHECK(texture_mailbox_.IsTexture());
}

CopyOutputResult::~CopyOutputResult() {
  if (release_callback_)
    release_callback_->Run(gpu::SyncToken(), false);
}

std::unique_ptr<SkBitmap> CopyOutputResult::TakeBitmap() {
  return std::move(bitmap_);
}

void CopyOutputResult::TakeTexture(
    TextureMailbox* texture_mailbox,
    std::unique_ptr<SingleReleaseCallback>* release_callback) {
  *texture_mailbox = texture_mailbox_;
  *release_callback = std::move(release_callback_);

  texture_mailbox_ = TextureMailbox();
}

}  // namespace cc
