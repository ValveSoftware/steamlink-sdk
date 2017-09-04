// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_CLIPBOARD_DELEGATE_H_
#define CONTENT_RENDERER_RENDERER_CLIPBOARD_DELEGATE_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/common/clipboard_format.h"
#include "ui/base/clipboard/clipboard_types.h"

class GURL;
class SkBitmap;

namespace content {

// Renderer interface to read/write from the clipboard over IPC.
class RendererClipboardDelegate {
 public:
  RendererClipboardDelegate();

  uint64_t GetSequenceNumber(ui::ClipboardType type);
  bool IsFormatAvailable(ClipboardFormat format, ui::ClipboardType type);
  void Clear(ui::ClipboardType type);
  void ReadAvailableTypes(ui::ClipboardType type,
                          std::vector<base::string16>* types,
                          bool* contains_filenames);
  void ReadText(ui::ClipboardType type, base::string16* result);
  void ReadHTML(ui::ClipboardType type,
                base::string16* markup,
                GURL* url,
                uint32_t* fragment_start,
                uint32_t* fragment_end);
  void ReadRTF(ui::ClipboardType type, std::string* result);
  void ReadImage(ui::ClipboardType type,
                 std::string* blob_uuid,
                 std::string* mime_type,
                 int64_t* size);
  void ReadCustomData(ui::ClipboardType clipboard_type,
                      const base::string16& type,
                      base::string16* data);

  void WriteText(ui::ClipboardType type, const base::string16& text);
  void WriteHTML(ui::ClipboardType type,
                 const base::string16& markup,
                 const GURL& url);
  void WriteSmartPasteMarker(ui::ClipboardType type);
  void WriteCustomData(ui::ClipboardType type,
                       const std::map<base::string16, base::string16>& data);
  void WriteBookmark(ui::ClipboardType type,
                     const GURL& url,
                     const base::string16& title);
  bool WriteImage(ui::ClipboardType type, const SkBitmap& bitmap);
  void CommitWrite(ui::ClipboardType type);

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererClipboardDelegate);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_CLIPBOARD_DELEGATE_H_
