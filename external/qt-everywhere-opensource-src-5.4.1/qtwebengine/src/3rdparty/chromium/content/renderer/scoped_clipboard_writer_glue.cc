// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/scoped_clipboard_writer_glue.h"
#include "base/logging.h"

namespace content {

ScopedClipboardWriterGlue::ScopedClipboardWriterGlue(ClipboardClient* client)
    : ui::ScopedClipboardWriter(client->GetClipboard(),
                                ui::CLIPBOARD_TYPE_COPY_PASTE),
      context_(client->CreateWriteContext()) {
  // We should never have an instance where both are set.
  DCHECK((clipboard_ && !context_.get()) ||
         (!clipboard_ && context_.get()));
}

ScopedClipboardWriterGlue::~ScopedClipboardWriterGlue() {
  if (!objects_.empty() && context_) {
    context_->Flush(objects_);
  }
}

void ScopedClipboardWriterGlue::WriteBitmapFromPixels(const void* pixels,
                                                      const gfx::Size& size) {
  if (context_) {
    context_->WriteBitmapFromPixels(&objects_, pixels, size);
  } else {
    NOTREACHED();
  }
}

}  // namespace content

