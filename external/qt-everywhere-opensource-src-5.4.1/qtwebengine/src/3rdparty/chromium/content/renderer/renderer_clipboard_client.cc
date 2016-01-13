// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the embedder's side of the Clipboard interface.

#include "content/renderer/renderer_clipboard_client.h"

#include "base/memory/shared_memory.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string16.h"
#include "content/common/clipboard_messages.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/scoped_clipboard_writer_glue.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/size.h"

namespace content {

namespace {

class RendererClipboardWriteContext : public ClipboardClient::WriteContext {
 public:
  RendererClipboardWriteContext();
  virtual ~RendererClipboardWriteContext();
  virtual void WriteBitmapFromPixels(ui::Clipboard::ObjectMap* objects,
                                     const void* pixels,
                                     const gfx::Size& size) OVERRIDE;
  virtual void Flush(const ui::Clipboard::ObjectMap& objects) OVERRIDE;

 private:
  scoped_ptr<base::SharedMemory> shared_buf_;
  DISALLOW_COPY_AND_ASSIGN(RendererClipboardWriteContext);
};

RendererClipboardWriteContext::RendererClipboardWriteContext() {
}

RendererClipboardWriteContext::~RendererClipboardWriteContext() {
}

// This definition of WriteBitmapFromPixels uses shared memory to communicate
// across processes.
void RendererClipboardWriteContext::WriteBitmapFromPixels(
    ui::Clipboard::ObjectMap* objects,
    const void* pixels,
    const gfx::Size& size) {
  // Do not try to write a bitmap more than once
  if (shared_buf_)
    return;

  base::CheckedNumeric<uint32> checked_buf_size = 4;
  checked_buf_size *= size.width();
  checked_buf_size *= size.height();
  if (!checked_buf_size.IsValid())
    return;

  uint32 buf_size = checked_buf_size.ValueOrDie();

  // Allocate a shared memory buffer to hold the bitmap bits.
  shared_buf_.reset(ChildThread::current()->AllocateSharedMemory(buf_size));
  if (!shared_buf_)
    return;

  // Copy the bits into shared memory
  DCHECK(shared_buf_->memory());
  memcpy(shared_buf_->memory(), pixels, buf_size);
  shared_buf_->Unmap();

  ui::Clipboard::ObjectMapParam size_param;
  const char* size_data = reinterpret_cast<const char*>(&size);
  for (size_t i = 0; i < sizeof(gfx::Size); ++i)
    size_param.push_back(size_data[i]);

  ui::Clipboard::ObjectMapParams params;

  // The first parameter is replaced on the receiving end with a pointer to
  // a shared memory object containing the bitmap. We reserve space for it here.
  ui::Clipboard::ObjectMapParam place_holder_param;
  params.push_back(place_holder_param);
  params.push_back(size_param);
  (*objects)[ui::Clipboard::CBF_SMBITMAP] = params;
}

// Flushes the objects to the clipboard with an IPC.
void RendererClipboardWriteContext::Flush(
    const ui::Clipboard::ObjectMap& objects) {
  if (shared_buf_) {
    RenderThreadImpl::current()->Send(
        new ClipboardHostMsg_WriteObjectsSync(objects, shared_buf_->handle()));
  } else {
    RenderThreadImpl::current()->Send(
        new ClipboardHostMsg_WriteObjectsAsync(objects));
  }
}

}  // anonymous namespace

RendererClipboardClient::RendererClipboardClient() {
}

RendererClipboardClient::~RendererClipboardClient() {
}

ui::Clipboard* RendererClipboardClient::GetClipboard() {
  return NULL;
}

uint64 RendererClipboardClient::GetSequenceNumber(ui::ClipboardType type) {
  uint64 sequence_number = 0;
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_GetSequenceNumber(type, &sequence_number));
  return sequence_number;
}

bool RendererClipboardClient::IsFormatAvailable(content::ClipboardFormat format,
                                                ui::ClipboardType type) {
  bool result;
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_IsFormatAvailable(format, type, &result));
  return result;
}

void RendererClipboardClient::Clear(ui::ClipboardType type) {
  RenderThreadImpl::current()->Send(new ClipboardHostMsg_Clear(type));
}

void RendererClipboardClient::ReadAvailableTypes(
    ui::ClipboardType type,
    std::vector<base::string16>* types,
    bool* contains_filenames) {
  RenderThreadImpl::current()->Send(new ClipboardHostMsg_ReadAvailableTypes(
      type, types, contains_filenames));
}

void RendererClipboardClient::ReadText(ui::ClipboardType type,
                                       base::string16* result) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadText(type, result));
}

void RendererClipboardClient::ReadHTML(ui::ClipboardType type,
                                       base::string16* markup,
                                       GURL* url, uint32* fragment_start,
                                       uint32* fragment_end) {
  RenderThreadImpl::current()->Send(new ClipboardHostMsg_ReadHTML(
      type, markup, url, fragment_start, fragment_end));
}

void RendererClipboardClient::ReadRTF(ui::ClipboardType type,
                                      std::string* result) {
  RenderThreadImpl::current()->Send(new ClipboardHostMsg_ReadRTF(type, result));
}

void RendererClipboardClient::ReadImage(ui::ClipboardType type,
                                        std::string* data) {
  base::SharedMemoryHandle image_handle;
  uint32 image_size;
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadImage(type, &image_handle, &image_size));
  if (base::SharedMemory::IsHandleValid(image_handle)) {
    base::SharedMemory buffer(image_handle, true);
    buffer.Map(image_size);
    data->append(static_cast<char*>(buffer.memory()), image_size);
  }
}

void RendererClipboardClient::ReadCustomData(ui::ClipboardType clipboard_type,
                                             const base::string16& type,
                                             base::string16* data) {
  RenderThreadImpl::current()->Send(
      new ClipboardHostMsg_ReadCustomData(clipboard_type, type, data));
}

ClipboardClient::WriteContext* RendererClipboardClient::CreateWriteContext() {
  return new RendererClipboardWriteContext;
}

}  // namespace content
