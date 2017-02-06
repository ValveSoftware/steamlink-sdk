// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webclipboard_impl.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/common/clipboard_format.h"
#include "content/public/common/drop_data.h"
#include "content/renderer/clipboard_utils.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/renderer_clipboard_delegate.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "url/gurl.h"

using blink::WebBlobInfo;
using blink::WebClipboard;
using blink::WebDragData;
using blink::WebImage;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

WebClipboardImpl::WebClipboardImpl(RendererClipboardDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

WebClipboardImpl::~WebClipboardImpl() {
}

uint64_t WebClipboardImpl::sequenceNumber(Buffer buffer) {
  ui::ClipboardType clipboard_type;
  if (!ConvertBufferType(buffer, &clipboard_type))
    return 0;

  return delegate_->GetSequenceNumber(clipboard_type);
}

bool WebClipboardImpl::isFormatAvailable(Format format, Buffer buffer) {
  ui::ClipboardType clipboard_type = ui::CLIPBOARD_TYPE_COPY_PASTE;

  if (!ConvertBufferType(buffer, &clipboard_type))
    return false;

  switch (format) {
    case FormatPlainText:
      return delegate_->IsFormatAvailable(CLIPBOARD_FORMAT_PLAINTEXT,
                                          clipboard_type);
    case FormatHTML:
      return delegate_->IsFormatAvailable(CLIPBOARD_FORMAT_HTML,
                                          clipboard_type);
    case FormatSmartPaste:
      return delegate_->IsFormatAvailable(CLIPBOARD_FORMAT_SMART_PASTE,
                                          clipboard_type);
    case FormatBookmark:
      return delegate_->IsFormatAvailable(CLIPBOARD_FORMAT_BOOKMARK,
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
    delegate_->ReadAvailableTypes(clipboard_type, &types, contains_filenames);
  }
  return types;
}

WebString WebClipboardImpl::readPlainText(Buffer buffer) {
  ui::ClipboardType clipboard_type;
  if (!ConvertBufferType(buffer, &clipboard_type))
    return WebString();

  base::string16 text;
  delegate_->ReadText(clipboard_type, &text);
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
  delegate_->ReadHTML(clipboard_type, &html_stdstr, &gurl,
                      static_cast<uint32_t*>(fragment_start),
                      static_cast<uint32_t*>(fragment_end));
  *source_url = gurl;
  return html_stdstr;
}

WebBlobInfo WebClipboardImpl::readImage(Buffer buffer) {
  ui::ClipboardType clipboard_type;
  if (!ConvertBufferType(buffer, &clipboard_type))
    return WebBlobInfo();

  std::string blob_uuid;
  std::string type;
  int64_t size;
  delegate_->ReadImage(clipboard_type, &blob_uuid, &type, &size);
  if (size < 0)
    return WebBlobInfo();
  return WebBlobInfo(base::ASCIIToUTF16(blob_uuid), base::UTF8ToUTF16(type),
                     size);
}

WebString WebClipboardImpl::readCustomData(Buffer buffer,
                                           const WebString& type) {
  ui::ClipboardType clipboard_type;
  if (!ConvertBufferType(buffer, &clipboard_type))
    return WebString();

  base::string16 data;
  delegate_->ReadCustomData(clipboard_type, type, &data);
  return data;
}

void WebClipboardImpl::writePlainText(const WebString& plain_text) {
  delegate_->WriteText(ui::CLIPBOARD_TYPE_COPY_PASTE, plain_text);
  delegate_->CommitWrite(ui::CLIPBOARD_TYPE_COPY_PASTE);
}

void WebClipboardImpl::writeHTML(
    const WebString& html_text, const WebURL& source_url,
    const WebString& plain_text, bool write_smart_paste) {
  delegate_->WriteHTML(ui::CLIPBOARD_TYPE_COPY_PASTE, html_text, source_url);
  delegate_->WriteText(ui::CLIPBOARD_TYPE_COPY_PASTE, plain_text);

  if (write_smart_paste)
    delegate_->WriteSmartPasteMarker(ui::CLIPBOARD_TYPE_COPY_PASTE);
  delegate_->CommitWrite(ui::CLIPBOARD_TYPE_COPY_PASTE);
}

void WebClipboardImpl::writeImage(const WebImage& image,
                                  const WebURL& url,
                                  const WebString& title) {
  DCHECK(!image.isNull());
  const SkBitmap& bitmap = image.getSkBitmap();
  if (!delegate_->WriteImage(ui::CLIPBOARD_TYPE_COPY_PASTE, bitmap))
    return;

  if (!url.isEmpty()) {
    delegate_->WriteBookmark(ui::CLIPBOARD_TYPE_COPY_PASTE, url, title);
#if !defined(OS_MACOSX)
    // When writing the image, we also write the image markup so that pasting
    // into rich text editors, such as Gmail, reveals the image. We also don't
    // want to call writeText(), since some applications (WordPad) don't pick
    // the image if there is also a text format on the clipboard.
    // We also don't want to write HTML on a Mac, since Mail.app prefers to use
    // the image markup over attaching the actual image. See
    // http://crbug.com/33016 for details.
    delegate_->WriteHTML(ui::CLIPBOARD_TYPE_COPY_PASTE,
                         base::UTF8ToUTF16(URLToImageMarkup(url, title)),
                         GURL());
#endif
  }
  delegate_->CommitWrite(ui::CLIPBOARD_TYPE_COPY_PASTE);
}

void WebClipboardImpl::writeDataObject(const WebDragData& data) {
  const DropData& data_object = DropDataBuilder::Build(data);
  // TODO(dcheng): Properly support text/uri-list here.
  // Avoid calling the WriteFoo functions if there is no data associated with a
  // type. This prevents stomping on clipboard contents that might have been
  // written by extension functions such as chrome.bookmarkManagerPrivate.copy.
  if (!data_object.text.is_null())
    delegate_->WriteText(ui::CLIPBOARD_TYPE_COPY_PASTE,
                         data_object.text.string());
  if (!data_object.html.is_null())
    delegate_->WriteHTML(ui::CLIPBOARD_TYPE_COPY_PASTE,
                         data_object.html.string(), GURL());
  if (!data_object.custom_data.empty())
    delegate_->WriteCustomData(ui::CLIPBOARD_TYPE_COPY_PASTE,
                               data_object.custom_data);
  delegate_->CommitWrite(ui::CLIPBOARD_TYPE_COPY_PASTE);
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
