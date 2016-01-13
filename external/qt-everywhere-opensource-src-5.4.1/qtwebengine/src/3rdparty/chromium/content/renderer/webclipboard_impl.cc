// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webclipboard_impl.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/clipboard_format.h"
#include "content/public/common/drop_data.h"
#include "content/renderer/clipboard_utils.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/scoped_clipboard_writer_glue.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "url/gurl.h"

using blink::WebClipboard;
using blink::WebData;
using blink::WebDragData;
using blink::WebImage;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

WebClipboardImpl::WebClipboardImpl(ClipboardClient* client)
    : client_(client) {
}

WebClipboardImpl::~WebClipboardImpl() {
}

uint64 WebClipboardImpl::sequenceNumber(Buffer buffer) {
  ui::ClipboardType clipboard_type;
  if (!ConvertBufferType(buffer, &clipboard_type))
    return 0;

  return client_->GetSequenceNumber(clipboard_type);
}

bool WebClipboardImpl::isFormatAvailable(Format format, Buffer buffer) {
  ui::ClipboardType clipboard_type = ui::CLIPBOARD_TYPE_COPY_PASTE;

  if (!ConvertBufferType(buffer, &clipboard_type))
    return false;

  switch (format) {
    case FormatPlainText:
      return client_->IsFormatAvailable(CLIPBOARD_FORMAT_PLAINTEXT,
                                        clipboard_type);
    case FormatHTML:
      return client_->IsFormatAvailable(CLIPBOARD_FORMAT_HTML, clipboard_type);
    case FormatSmartPaste:
      return client_->IsFormatAvailable(CLIPBOARD_FORMAT_SMART_PASTE,
                                        clipboard_type);
    case FormatBookmark:
      return client_->IsFormatAvailable(CLIPBOARD_FORMAT_BOOKMARK,
                                        clipboard_type);
    default:
      NOTREACHED();
  }

  return false;
}

WebVector<WebString> WebClipboardImpl::readAvailableTypes(
    Buffer buffer, bool* contains_filenames) {
  ui::ClipboardType clipboard_type;
  std::vector<base::string16> types;
  if (ConvertBufferType(buffer, &clipboard_type)) {
    client_->ReadAvailableTypes(clipboard_type, &types, contains_filenames);
  }
  return types;
}

WebString WebClipboardImpl::readPlainText(Buffer buffer) {
  ui::ClipboardType clipboard_type;
  if (!ConvertBufferType(buffer, &clipboard_type))
    return WebString();

  base::string16 text;
  client_->ReadText(clipboard_type, &text);
  return text;
}

WebString WebClipboardImpl::readHTML(Buffer buffer, WebURL* source_url,
                                     unsigned* fragment_start,
                                     unsigned* fragment_end) {
  ui::ClipboardType clipboard_type;
  if (!ConvertBufferType(buffer, &clipboard_type))
    return WebString();

  base::string16 html_stdstr;
  GURL gurl;
  client_->ReadHTML(clipboard_type, &html_stdstr, &gurl,
                    static_cast<uint32*>(fragment_start),
                    static_cast<uint32*>(fragment_end));
  *source_url = gurl;
  return html_stdstr;
}

WebData WebClipboardImpl::readImage(Buffer buffer) {
  ui::ClipboardType clipboard_type;
  if (!ConvertBufferType(buffer, &clipboard_type))
    return WebData();

  std::string png_data;
  client_->ReadImage(clipboard_type, &png_data);
  return WebData(png_data);
}

WebString WebClipboardImpl::readCustomData(Buffer buffer,
                                           const WebString& type) {
  ui::ClipboardType clipboard_type;
  if (!ConvertBufferType(buffer, &clipboard_type))
    return WebString();

  base::string16 data;
  client_->ReadCustomData(clipboard_type, type, &data);
  return data;
}

void WebClipboardImpl::writePlainText(const WebString& plain_text) {
  ScopedClipboardWriterGlue scw(client_);
  scw.WriteText(plain_text);
}

void WebClipboardImpl::writeHTML(
    const WebString& html_text, const WebURL& source_url,
    const WebString& plain_text, bool write_smart_paste) {
  ScopedClipboardWriterGlue scw(client_);
  scw.WriteHTML(html_text, source_url.spec());
  scw.WriteText(plain_text);

  if (write_smart_paste)
    scw.WriteWebSmartPaste();
}

void WebClipboardImpl::writeImage(const WebImage& image,
                                  const WebURL& url,
                                  const WebString& title) {
  ScopedClipboardWriterGlue scw(client_);

  if (!image.isNull()) {
    const SkBitmap& bitmap = image.getSkBitmap();
    // WriteBitmapFromPixels expects 32-bit data.
    DCHECK_EQ(bitmap.config(), SkBitmap::kARGB_8888_Config);

    SkAutoLockPixels locked(bitmap);
    void *pixels = bitmap.getPixels();
    // TODO(piman): this should not be NULL, but it is. crbug.com/369621
    if (!pixels)
      return;
    scw.WriteBitmapFromPixels(pixels, image.size());
  }

  if (!url.isEmpty()) {
    scw.WriteBookmark(title, url.spec());
#if !defined(OS_MACOSX)
    // When writing the image, we also write the image markup so that pasting
    // into rich text editors, such as Gmail, reveals the image. We also don't
    // want to call writeText(), since some applications (WordPad) don't pick
    // the image if there is also a text format on the clipboard.
    // We also don't want to write HTML on a Mac, since Mail.app prefers to use
    // the image markup over attaching the actual image. See
    // http://crbug.com/33016 for details.
    scw.WriteHTML(base::UTF8ToUTF16(URLToImageMarkup(url, title)),
                  std::string());
#endif
  }
}

void WebClipboardImpl::writeDataObject(const WebDragData& data) {
  ScopedClipboardWriterGlue scw(client_);

  const DropData& data_object = DropDataBuilder::Build(data);
  // TODO(dcheng): Properly support text/uri-list here.
  if (!data_object.text.is_null())
    scw.WriteText(data_object.text.string());
  if (!data_object.html.is_null())
    scw.WriteHTML(data_object.html.string(), std::string());
  // If there is no custom data, avoid calling WritePickledData. This ensures
  // that ScopedClipboardWriterGlue's dtor remains a no-op if the page didn't
  // modify the DataTransfer object, which is important to avoid stomping on
  // any clipboard contents written by extension functions such as
  // chrome.bookmarkManagerPrivate.copy.
  if (!data_object.custom_data.empty()) {
    Pickle pickle;
    ui::WriteCustomDataToPickle(data_object.custom_data, &pickle);
    scw.WritePickledData(pickle, ui::Clipboard::GetWebCustomDataFormatType());
  }
}

bool WebClipboardImpl::ConvertBufferType(Buffer buffer,
                                         ui::ClipboardType* result) {
  *result = ui::CLIPBOARD_TYPE_COPY_PASTE;
  switch (buffer) {
    case BufferStandard:
      break;
    case BufferSelection:
#if defined(USE_X11) && !defined(OS_CHROMEOS)
      *result = ui::CLIPBOARD_TYPE_SELECTION;
      break;
#else
      // Chrome OS and non-X11 unix builds do not support
      // the X selection clipboad.
      // TODO: remove the need for this case, see http://crbug.com/361753
      return false;
#endif
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

}  // namespace content
