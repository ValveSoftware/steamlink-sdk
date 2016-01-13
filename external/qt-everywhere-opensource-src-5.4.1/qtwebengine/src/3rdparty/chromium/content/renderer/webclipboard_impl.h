// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_
#define CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_

#include "base/compiler_specific.h"

#include "third_party/WebKit/public/platform/WebClipboard.h"
#include "ui/base/clipboard/clipboard.h"

#include <string>

namespace content {
class ClipboardClient;

class WebClipboardImpl : public blink::WebClipboard {
 public:
  explicit WebClipboardImpl(ClipboardClient* client);

  virtual ~WebClipboardImpl();

  // WebClipboard methods:
  virtual uint64 sequenceNumber(Buffer buffer);
  virtual bool isFormatAvailable(Format format, Buffer buffer);
  virtual blink::WebVector<blink::WebString> readAvailableTypes(
      Buffer buffer, bool* contains_filenames);
  virtual blink::WebString readPlainText(Buffer buffer);
  virtual blink::WebString readHTML(
      Buffer buffer,
      blink::WebURL* source_url,
      unsigned* fragment_start,
      unsigned* fragment_end);
  virtual blink::WebData readImage(Buffer buffer);
  virtual blink::WebString readCustomData(
      Buffer buffer, const blink::WebString& type);
  virtual void writePlainText(const blink::WebString& plain_text);
  virtual void writeHTML(
      const blink::WebString& html_text,
      const blink::WebURL& source_url,
      const blink::WebString& plain_text,
      bool write_smart_paste);
  virtual void writeImage(
      const blink::WebImage& image,
      const blink::WebURL& source_url,
      const blink::WebString& title);
  virtual void writeDataObject(const blink::WebDragData& data);

 private:
  bool ConvertBufferType(Buffer, ui::ClipboardType*);
  ClipboardClient* client_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBCLIPBOARD_IMPL_H_
