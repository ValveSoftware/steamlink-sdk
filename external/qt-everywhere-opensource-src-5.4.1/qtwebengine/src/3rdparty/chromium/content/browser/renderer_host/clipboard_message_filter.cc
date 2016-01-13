// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/clipboard_messages.h"
#include "content/public/browser/browser_context.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

namespace content {

namespace {

enum BitmapPolicy {
  kFilterBitmap,
  kAllowBitmap,
};
void SanitizeObjectMap(ui::Clipboard::ObjectMap* objects,
                       BitmapPolicy bitmap_policy) {
  if (bitmap_policy != kAllowBitmap)
    objects->erase(ui::Clipboard::CBF_SMBITMAP);

  ui::Clipboard::ObjectMap::iterator data_it =
      objects->find(ui::Clipboard::CBF_DATA);
  if (data_it != objects->end()) {
    const ui::Clipboard::FormatType& web_custom_format =
        ui::Clipboard::GetWebCustomDataFormatType();
    if (data_it->second.size() != 2 ||
        !web_custom_format.Equals(
            ui::Clipboard::FormatType::Deserialize(std::string(
                &data_it->second[0].front(),
                data_it->second[0].size())))) {
      // CBF_DATA should always have two parameters associated with it, and the
      // associated FormatType should always be web custom data. If not, then
      // data is malformed and we'll ignore it.
      objects->erase(ui::Clipboard::CBF_DATA);
    }
  }
}

}  // namespace


ClipboardMessageFilter::ClipboardMessageFilter()
    : BrowserMessageFilter(ClipboardMsgStart) {}

void ClipboardMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, BrowserThread::ID* thread) {
  // Clipboard writes should always occur on the UI thread due the restrictions
  // of various platform APIs. In general, the clipboard is not thread-safe, so
  // all clipboard calls should be serviced from the UI thread.
  //
  // Windows needs clipboard reads to be serviced from the IO thread because
  // these are sync IPCs which can result in deadlocks with NPAPI plugins if
  // serviced from the UI thread. Note that Windows clipboard calls ARE
  // thread-safe so it is ok for reads and writes to be serviced from different
  // threads.
#if !defined(OS_WIN)
  if (IPC_MESSAGE_CLASS(message) == ClipboardMsgStart)
    *thread = BrowserThread::UI;
#endif

#if defined(OS_WIN)
  if (message.type() == ClipboardHostMsg_ReadImage::ID)
    *thread = BrowserThread::FILE;
#endif
}

bool ClipboardMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ClipboardMessageFilter, message)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_WriteObjectsAsync, OnWriteObjectsAsync)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_WriteObjectsSync, OnWriteObjectsSync)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_GetSequenceNumber, OnGetSequenceNumber)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_IsFormatAvailable, OnIsFormatAvailable)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_Clear, OnClear)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadAvailableTypes,
                        OnReadAvailableTypes)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadText, OnReadText)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadHTML, OnReadHTML)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadRTF, OnReadRTF)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ClipboardHostMsg_ReadImage, OnReadImage)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_ReadCustomData, OnReadCustomData)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ClipboardHostMsg_FindPboardWriteStringAsync,
                        OnFindPboardWriteString)
#endif
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

ClipboardMessageFilter::~ClipboardMessageFilter() {
}

void ClipboardMessageFilter::OnWriteObjectsSync(
    const ui::Clipboard::ObjectMap& objects,
    base::SharedMemoryHandle bitmap_handle) {
  DCHECK(base::SharedMemory::IsHandleValid(bitmap_handle))
      << "Bad bitmap handle";

  // On Windows, we can't write directly from the IO thread, so we copy the data
  // into a heap allocated map and post a task to the UI thread. On other
  // platforms, to lower the amount of time the renderer has to wait for the
  // sync IPC to complete, we also take a copy and post a task to flush the data
  // to the clipboard later.
  scoped_ptr<ui::Clipboard::ObjectMap> long_living_objects(
      new ui::Clipboard::ObjectMap(objects));
  SanitizeObjectMap(long_living_objects.get(), kAllowBitmap);
  // Splice the shared memory handle into the data. |long_living_objects| now
  // contains a heap-allocated SharedMemory object that references
  // |bitmap_handle|. This reference will keep the shared memory section alive
  // when this IPC returns, and the SharedMemory object will eventually be
  // freed by ui::Clipboard::WriteObjects().
  if (!ui::Clipboard::ReplaceSharedMemHandle(
           long_living_objects.get(), bitmap_handle, PeerHandle()))
    return;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ClipboardMessageFilter::WriteObjectsOnUIThread,
                 base::Owned(long_living_objects.release())));
}

// On Windows, the write must be performed on the UI thread because the
// clipboard object from the IO thread cannot create windows so it cannot be
// the "owner" of the clipboard's contents. See http://crbug.com/5823.
// TODO(dcheng): Temporarily a member of ClipboardMessageFilter so it can access
// ui::Clipboard::WriteObjects().
void ClipboardMessageFilter::WriteObjectsOnUIThread(
    const ui::Clipboard::ObjectMap* objects) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  clipboard->WriteObjects(ui::CLIPBOARD_TYPE_COPY_PASTE, *objects);
}

void ClipboardMessageFilter::OnWriteObjectsAsync(
    const ui::Clipboard::ObjectMap& objects) {
  // This async message doesn't support shared-memory based bitmaps; they must
  // be removed otherwise we might dereference a rubbish pointer.
  scoped_ptr<ui::Clipboard::ObjectMap> sanitized_objects(
      new ui::Clipboard::ObjectMap(objects));
  SanitizeObjectMap(sanitized_objects.get(), kFilterBitmap);

#if defined(OS_WIN)
  // We cannot write directly from the IO thread, and cannot service the IPC
  // on the UI thread. We'll copy the relevant data and post a task to preform
  // the write on the UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &WriteObjectsOnUIThread, base::Owned(sanitized_objects.release())));
#else
  GetClipboard()->WriteObjects(
      ui::CLIPBOARD_TYPE_COPY_PASTE, *sanitized_objects.get());
#endif
}

void ClipboardMessageFilter::OnGetSequenceNumber(ui::ClipboardType type,
                                                 uint64* sequence_number) {
  *sequence_number = GetClipboard()->GetSequenceNumber(type);
}

void ClipboardMessageFilter::OnReadAvailableTypes(
    ui::ClipboardType type,
    std::vector<base::string16>* types,
    bool* contains_filenames) {
  GetClipboard()->ReadAvailableTypes(type, types, contains_filenames);
}

void ClipboardMessageFilter::OnIsFormatAvailable(ClipboardFormat format,
                                                 ui::ClipboardType type,
                                                 bool* result) {
  switch (format) {
    case CLIPBOARD_FORMAT_PLAINTEXT:
      *result = GetClipboard()->IsFormatAvailable(
                    ui::Clipboard::GetPlainTextWFormatType(), type) ||
                GetClipboard()->IsFormatAvailable(
                    ui::Clipboard::GetPlainTextFormatType(), type);
      break;
    case CLIPBOARD_FORMAT_HTML:
      *result = GetClipboard()->IsFormatAvailable(
          ui::Clipboard::GetHtmlFormatType(), type);
      break;
    case CLIPBOARD_FORMAT_SMART_PASTE:
      *result = GetClipboard()->IsFormatAvailable(
          ui::Clipboard::GetWebKitSmartPasteFormatType(), type);
      break;
    case CLIPBOARD_FORMAT_BOOKMARK:
#if defined(OS_WIN) || defined(OS_MACOSX)
      *result = GetClipboard()->IsFormatAvailable(
          ui::Clipboard::GetUrlWFormatType(), type);
#else
      *result = false;
#endif
      break;
  }
}

void ClipboardMessageFilter::OnClear(ui::ClipboardType type) {
  GetClipboard()->Clear(type);
}

void ClipboardMessageFilter::OnReadText(ui::ClipboardType type,
                                        base::string16* result) {
  if (GetClipboard()->IsFormatAvailable(
          ui::Clipboard::GetPlainTextWFormatType(), type)) {
    GetClipboard()->ReadText(type, result);
  } else if (GetClipboard()->IsFormatAvailable(
                 ui::Clipboard::GetPlainTextFormatType(), type)) {
    std::string ascii;
    GetClipboard()->ReadAsciiText(type, &ascii);
    *result = base::ASCIIToUTF16(ascii);
  } else {
    result->clear();
  }
}

void ClipboardMessageFilter::OnReadHTML(ui::ClipboardType type,
                                        base::string16* markup,
                                        GURL* url,
                                        uint32* fragment_start,
                                        uint32* fragment_end) {
  std::string src_url_str;
  GetClipboard()->ReadHTML(type, markup, &src_url_str, fragment_start,
                           fragment_end);
  *url = GURL(src_url_str);
}

void ClipboardMessageFilter::OnReadRTF(ui::ClipboardType type,
                                       std::string* result) {
  GetClipboard()->ReadRTF(type, result);
}

void ClipboardMessageFilter::OnReadImage(ui::ClipboardType type,
                                         IPC::Message* reply_msg) {
  SkBitmap bitmap = GetClipboard()->ReadImage(type);

#if defined(USE_X11)
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &ClipboardMessageFilter::OnReadImageReply, this, bitmap, reply_msg));
#else
  OnReadImageReply(bitmap, reply_msg);
#endif
}

void ClipboardMessageFilter::OnReadImageReply(
    const SkBitmap& bitmap, IPC::Message* reply_msg) {
  base::SharedMemoryHandle image_handle = base::SharedMemory::NULLHandle();
  uint32 image_size = 0;
  if (!bitmap.isNull()) {
    std::vector<unsigned char> png_data;
    if (gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &png_data)) {
      base::SharedMemory buffer;
      if (buffer.CreateAndMapAnonymous(png_data.size())) {
        memcpy(buffer.memory(), vector_as_array(&png_data), png_data.size());
        if (buffer.GiveToProcess(PeerHandle(), &image_handle)) {
          image_size = png_data.size();
        }
      }
    }
  }
  ClipboardHostMsg_ReadImage::WriteReplyParams(reply_msg, image_handle,
                                               image_size);
  Send(reply_msg);
}

void ClipboardMessageFilter::OnReadCustomData(ui::ClipboardType clipboard_type,
                                              const base::string16& type,
                                              base::string16* result) {
  GetClipboard()->ReadCustomData(clipboard_type, type, result);
}

// static
ui::Clipboard* ClipboardMessageFilter::GetClipboard() {
  // We have a static instance of the clipboard service for use by all message
  // filters.  This instance lives for the life of the browser processes.
  static ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  return clipboard;
}

}  // namespace content
