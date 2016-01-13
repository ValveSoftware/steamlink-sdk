// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CLIPBOARD_CLIENT_H_
#define CONTENT_RENDERER_CLIPBOARD_CLIENT_H_

#include "content/common/clipboard_format.h"
#include "ui/base/clipboard/clipboard.h"

class GURL;

namespace content {

// Interface for the content embedder to implement to support clipboard.
class ClipboardClient {
 public:
  class WriteContext {
   public:
    virtual ~WriteContext() { }

    // Writes bitmap data into the context, updating the ObjectMap.
    virtual void WriteBitmapFromPixels(ui::Clipboard::ObjectMap* objects,
                                       const void* pixels,
                                       const gfx::Size& size) = 0;

    // Flushes all gathered data.
    virtual void Flush(const ui::Clipboard::ObjectMap& objects) = 0;
  };

  virtual ~ClipboardClient() { }

  // Get a clipboard that can be used to construct a ScopedClipboardWriterGlue.
  virtual ui::Clipboard* GetClipboard() = 0;

  // Get a sequence number which uniquely identifies clipboard state.
  virtual uint64 GetSequenceNumber(ui::ClipboardType type) = 0;

  // Tests whether the clipboard contains a certain format
  virtual bool IsFormatAvailable(ClipboardFormat format,
                                 ui::ClipboardType type) = 0;

  // Clear the contents of the clipboard.
  virtual void Clear(ui::ClipboardType type) = 0;

  // Reads the available types from the clipboard, if available.
  virtual void ReadAvailableTypes(ui::ClipboardType type,
                                  std::vector<base::string16>* types,
                                  bool* contains_filenames) = 0;

  // Reads text from the clipboard, trying UNICODE first, then falling back to
  // ASCII.
  virtual void ReadText(ui::ClipboardType type,
                        base::string16* result) = 0;

  // Reads HTML from the clipboard, if available.
  virtual void ReadHTML(ui::ClipboardType type,
                        base::string16* markup,
                        GURL* url,
                        uint32* fragment_start,
                        uint32* fragment_end) = 0;

  // Reads RTF from the clipboard, if available.
  virtual void ReadRTF(ui::ClipboardType type, std::string* result) = 0;

  // Reads and image from the clipboard, if available.
  virtual void ReadImage(ui::ClipboardType type, std::string* data) = 0;

  // Reads a custom data type from the clipboard, if available.
  virtual void ReadCustomData(ui::ClipboardType clipboard_type,
                              const base::string16& type,
                              base::string16* data) = 0;

  // Creates a context to write clipboard data. May return NULL.
  virtual WriteContext* CreateWriteContext() = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_CLIPBOARD_CLIENT_H_

