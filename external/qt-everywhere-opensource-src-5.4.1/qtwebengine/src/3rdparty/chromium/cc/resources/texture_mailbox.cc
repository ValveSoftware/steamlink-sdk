// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/texture_mailbox.h"

#include "base/logging.h"
#include "cc/resources/shared_bitmap.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

TextureMailbox::TextureMailbox() : shared_memory_(NULL) {}

TextureMailbox::TextureMailbox(const gpu::MailboxHolder& mailbox_holder)
    : mailbox_holder_(mailbox_holder),
      shared_memory_(NULL),
      allow_overlay_(false) {}

TextureMailbox::TextureMailbox(const gpu::Mailbox& mailbox,
                               uint32 target,
                               uint32 sync_point)
    : mailbox_holder_(mailbox, target, sync_point),
      shared_memory_(NULL),
      allow_overlay_(false) {}

TextureMailbox::TextureMailbox(base::SharedMemory* shared_memory,
                               const gfx::Size& size)
    : shared_memory_(shared_memory),
      shared_memory_size_(size),
      allow_overlay_(false) {
  // If an embedder of cc gives an invalid TextureMailbox, we should crash
  // here to identify the offender.
  CHECK(SharedBitmap::VerifySizeInBytes(shared_memory_size_));
}

TextureMailbox::~TextureMailbox() {}

bool TextureMailbox::Equals(const TextureMailbox& other) const {
  if (other.IsTexture()) {
    return IsTexture() && !memcmp(mailbox_holder_.mailbox.name,
                                  other.mailbox_holder_.mailbox.name,
                                  sizeof(mailbox_holder_.mailbox.name));
  } else if (other.IsSharedMemory()) {
    return IsSharedMemory() &&
           shared_memory_->handle() == other.shared_memory_->handle();
  }

  DCHECK(!other.IsValid());
  return !IsValid();
}

size_t TextureMailbox::SharedMemorySizeInBytes() const {
  // UncheckedSizeInBytes is okay because we VerifySizeInBytes in the
  // constructor and the field is immutable.
  return SharedBitmap::UncheckedSizeInBytes(shared_memory_size_);
}

}  // namespace cc
