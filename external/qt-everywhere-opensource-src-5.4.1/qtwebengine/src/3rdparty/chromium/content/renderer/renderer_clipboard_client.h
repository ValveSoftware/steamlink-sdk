// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_CLIPBOARD_CLIENT_H_
#define CONTENT_RENDERER_RENDERER_CLIPBOARD_CLIENT_H_

#include "base/compiler_specific.h"
#include "content/renderer/clipboard_client.h"

namespace content {

// An implementation of ClipboardClient that gets and sends data over IPC.
class RendererClipboardClient : public ClipboardClient {
 public:
  RendererClipboardClient();
  virtual ~RendererClipboardClient();

  virtual ui::Clipboard* GetClipboard() OVERRIDE;
  virtual uint64 GetSequenceNumber(ui::ClipboardType type) OVERRIDE;
  virtual bool IsFormatAvailable(ClipboardFormat format,
                                 ui::ClipboardType type) OVERRIDE;
  virtual void Clear(ui::ClipboardType type) OVERRIDE;
  virtual void ReadAvailableTypes(ui::ClipboardType type,
                                  std::vector<base::string16>* types,
                                  bool* contains_filenames) OVERRIDE;
  virtual void ReadText(ui::ClipboardType type,
                        base::string16* result) OVERRIDE;
  virtual void ReadHTML(ui::ClipboardType type,
                        base::string16* markup,
                        GURL* url,
                        uint32* fragment_start,
                        uint32* fragment_end) OVERRIDE;
  virtual void ReadRTF(ui::ClipboardType type, std::string* result) OVERRIDE;
  virtual void ReadImage(ui::ClipboardType type, std::string* data) OVERRIDE;
  virtual void ReadCustomData(ui::ClipboardType clipboard_type,
                              const base::string16& type,
                              base::string16* data) OVERRIDE;
  virtual WriteContext* CreateWriteContext() OVERRIDE;
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_CLIPBOARD_CLIENT_H_
