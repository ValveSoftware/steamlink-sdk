// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_
#define CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_

#include <stdint.h>

#include <string>

#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebClipboard.h"
#include "ui/base/clipboard/clipboard.h"

namespace content {
class RendererClipboardDelegate;

class WebClipboardImpl : public blink::WebClipboard {
 public:
  explicit WebClipboardImpl(RendererClipboardDelegate* delegate);

  virtual ~WebClipboardImpl();

  // WebClipboard methods:
  uint64_t sequenceNumber(Buffer buffer) override;
  bool isFormatAvailable(Format format, Buffer buffer) override;
  blink::WebVector<blink::WebString> readAvailableTypes(
      Buffer buffer,
      bool* contains_filenames) override;
  blink::WebString readPlainText(Buffer buffer) override;
  blink::WebString readHTML(Buffer buffer,
                            blink::WebURL* source_url,
                            unsigned* fragment_start,
                            unsigned* fragment_end) override;
  blink::WebString readRTF(Buffer buffer) override;
  blink::WebBlobInfo readImage(Buffer buffer) override;
  blink::WebString readCustomData(Buffer buffer,
                                  const blink::WebString& type) override;
  void writePlainText(const blink::WebString& plain_text) override;
  void writeHTML(const blink::WebString& html_text,
                 const blink::WebURL& source_url,
                 const blink::WebString& plain_text,
                 bool write_smart_paste) override;
  void writeImage(const blink::WebImage& image,
                  const blink::WebURL& source_url,
                  const blink::WebString& title) override;
  void writeDataObject(const blink::WebDragData& data) override;

 private:
  bool ConvertBufferType(Buffer, ui::ClipboardType*);
  RendererClipboardDelegate* const delegate_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_
