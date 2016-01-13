// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SCOPED_CLIPBOARD_WRITER_GLUE_H_
#define CONTENT_RENDERER_SCOPED_CLIPBOARD_WRITER_GLUE_H_

#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/clipboard_client.h"

namespace content {

class ScopedClipboardWriterGlue
    : public ui::ScopedClipboardWriter {
 public:
  explicit ScopedClipboardWriterGlue(ClipboardClient* client);

  virtual ~ScopedClipboardWriterGlue();

  void WriteBitmapFromPixels(const void* pixels, const gfx::Size& size);

 private:
  scoped_ptr<ClipboardClient::WriteContext> context_;
  DISALLOW_COPY_AND_ASSIGN(ScopedClipboardWriterGlue);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SCOPED_CLIPBOARD_WRITER_GLUE_H_
