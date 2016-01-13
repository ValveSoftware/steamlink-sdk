// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "content/common/clipboard_format.h"
#include "content/public/browser/browser_message_filter.h"
#include "ui/base/clipboard/clipboard.h"

class GURL;

namespace content {

class ClipboardMessageFilter : public BrowserMessageFilter {
 public:
  ClipboardMessageFilter();

  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
 private:
  virtual ~ClipboardMessageFilter();

  void OnWriteObjectsAsync(const ui::Clipboard::ObjectMap& objects);
  void OnWriteObjectsSync(const ui::Clipboard::ObjectMap& objects,
                          base::SharedMemoryHandle bitmap_handle);
  static void WriteObjectsOnUIThread(const ui::Clipboard::ObjectMap* objects);

  void OnGetSequenceNumber(const ui::ClipboardType type,
                           uint64* sequence_number);
  void OnIsFormatAvailable(ClipboardFormat format,
                           ui::ClipboardType type,
                           bool* result);
  void OnClear(ui::ClipboardType type);
  void OnReadAvailableTypes(ui::ClipboardType type,
                            std::vector<base::string16>* types,
                            bool* contains_filenames);
  void OnReadText(ui::ClipboardType type, base::string16* result);
  void OnReadHTML(ui::ClipboardType type,
                  base::string16* markup,
                  GURL* url,
                  uint32* fragment_start,
                  uint32* fragment_end);
  void OnReadRTF(ui::ClipboardType type, std::string* result);
  void OnReadImage(ui::ClipboardType type, IPC::Message* reply_msg);
  void OnReadImageReply(const SkBitmap& bitmap, IPC::Message* reply_msg);
  void OnReadCustomData(ui::ClipboardType clipboard_type,
                        const base::string16& type,
                        base::string16* result);
  void OnReadData(const ui::Clipboard::FormatType& format,
                  std::string* data);

#if defined(OS_MACOSX)
  void OnFindPboardWriteString(const base::string16& text);
#endif

  // We have our own clipboard because we want to access the clipboard on the
  // IO thread instead of forwarding (possibly synchronous) messages to the UI
  // thread. This instance of the clipboard should be accessed only on the IO
  // thread.
  static ui::Clipboard* GetClipboard();

  DISALLOW_COPY_AND_ASSIGN(ClipboardMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_MESSAGE_FILTER_H_
