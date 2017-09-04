// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the embedder's side of the Clipboard interface.

#include "content/renderer/renderer_clipboard_delegate.h"

#include "base/memory/shared_memory.h"
#include "base/numerics/safe_math.h"
#include "content/common/clipboard_messages.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/geometry/size.h"

namespace content {

RendererClipboardDelegate::RendererClipboardDelegate() {
}

uint64_t RendererClipboardDelegate::GetSequenceNumber(ui::ClipboardType type) {
  uint64_t sequence_number = 0;
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_GetSequenceNumber(type, &sequence_number));
  return sequence_number;
}

bool RendererClipboardDelegate::IsFormatAvailable(
    content::ClipboardFormat format,
    ui::ClipboardType type) {
  bool result = false;
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_IsFormatAvailable(format, type, &result));
  return result;
}

void RendererClipboardDelegate::Clear(ui::ClipboardType type) {
  RenderThreadImpl::current()->Send(new ClipboardHostMsg_Clear(type));
}

void RendererClipboardDelegate::ReadAvailableTypes(
    ui::ClipboardType type,
    std::vector<base::string16>* types,
    bool* contains_filenames) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadAvailableTypes(type, types, contains_filenames));
}

void RendererClipboardDelegate::ReadText(ui::ClipboardType type,
                                         base::string16* result) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadText(type, result));
}

void RendererClipboardDelegate::ReadHTML(ui::ClipboardType type,
                                         base::string16* markup,
                                         GURL* url,
                                         uint32_t* fragment_start,
                                         uint32_t* fragment_end) {
  RenderThreadImpl::current()->Send(new ClipboardHostMsg_ReadHTML(
      type, markup, url, fragment_start, fragment_end));
}

void RendererClipboardDelegate::ReadRTF(ui::ClipboardType type,
                                        std::string* result) {
  RenderThreadImpl::current()->Send(new ClipboardHostMsg_ReadRTF(type, result));
}

void RendererClipboardDelegate::ReadImage(ui::ClipboardType type,
                                          std::string* blob_uuid,
                                          std::string* mime_type,
                                          int64_t* size) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadImage(type, blob_uuid, mime_type, size));
}

void RendererClipboardDelegate::ReadCustomData(ui::ClipboardType clipboard_type,
                                               const base::string16& type,
                                               base::string16* data) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadCustomData(clipboard_type, type, data));
}

void RendererClipboardDelegate::WriteText(ui::ClipboardType clipboard_type,
                                          const base::string16& text) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_WriteText(clipboard_type, text));
}

void RendererClipboardDelegate::WriteHTML(ui::ClipboardType clipboard_type,
                                          const base::string16& markup,
                                          const GURL& url) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_WriteHTML(clipboard_type, markup, url));
}

void RendererClipboardDelegate::WriteSmartPasteMarker(
    ui::ClipboardType clipboard_type) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_WriteSmartPasteMarker(clipboard_type));
}

void RendererClipboardDelegate::WriteCustomData(
    ui::ClipboardType clipboard_type,
    const std::map<base::string16, base::string16>& data) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_WriteCustomData(clipboard_type, data));
}

void RendererClipboardDelegate::WriteBookmark(ui::ClipboardType clipboard_type,
                                              const GURL& url,
                                              const base::string16& title) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_WriteBookmark(clipboard_type, url.spec(), title));
}

bool RendererClipboardDelegate::WriteImage(ui::ClipboardType clipboard_type,
                                           const SkBitmap& bitmap) {
  // Only 32-bit bitmaps are supported.
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  const gfx::Size size(bitmap.width(), bitmap.height());
  std::unique_ptr<base::SharedMemory> shared_buf;
  {
    SkAutoLockPixels locked(bitmap);
    void* pixels = bitmap.getPixels();
    // TODO(piman): this should not be NULL, but it is. crbug.com/369621
    if (!pixels)
      return false;

    base::CheckedNumeric<uint32_t> checked_buf_size = 4;
    checked_buf_size *= size.width();
    checked_buf_size *= size.height();
    if (!checked_buf_size.IsValid())
      return false;

    // Allocate a shared memory buffer to hold the bitmap bits.
    uint32_t buf_size = checked_buf_size.ValueOrDie();
    shared_buf = ChildThreadImpl::current()->AllocateSharedMemory(buf_size);
    if (!shared_buf)
      return false;
    if (!shared_buf->Map(buf_size))
      return false;
    // Copy the bits into shared memory
    DCHECK(shared_buf->memory());
    memcpy(shared_buf->memory(), pixels, buf_size);
    shared_buf->Unmap();
  }

  RenderThreadImpl::current()->Send(new ClipboardHostMsg_WriteImage(
      clipboard_type, size, shared_buf->handle()));
  return true;
}

void RendererClipboardDelegate::CommitWrite(ui::ClipboardType clipboard_type) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_CommitWrite(clipboard_type));
}

}  // namespace content
