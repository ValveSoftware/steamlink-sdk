// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/common/client_native_pixmap_dmabuf.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include "base/memory/ptr_util.h"
#include "base/process/memory.h"
#include "base/trace_event/trace_event.h"

#if defined(OS_CHROMEOS)
// TODO(vignatti): replace the local definitions below with #include
// <linux/dma-buf.h> once kernel version 4.6 becomes widely used.
#include <linux/types.h>

struct local_dma_buf_sync {
  __u64 flags;
};

#define LOCAL_DMA_BUF_SYNC_READ (1 << 0)
#define LOCAL_DMA_BUF_SYNC_WRITE (2 << 0)
#define LOCAL_DMA_BUF_SYNC_START (0 << 2)
#define LOCAL_DMA_BUF_SYNC_END (1 << 2)

#define LOCAL_DMA_BUF_BASE 'b'
#define LOCAL_DMA_BUF_IOCTL_SYNC \
  _IOW(LOCAL_DMA_BUF_BASE, 0, struct local_dma_buf_sync)
#endif

namespace ui {

namespace {

void PrimeSyncStart(int dmabuf_fd) {
  struct local_dma_buf_sync sync_start = {0};

  sync_start.flags = LOCAL_DMA_BUF_SYNC_START | LOCAL_DMA_BUF_SYNC_READ;
  if (drmIoctl(dmabuf_fd, LOCAL_DMA_BUF_IOCTL_SYNC, &sync_start))
    PLOG(ERROR) << "Failed DMA_BUF_SYNC_START";
}

void PrimeSyncEnd(int dmabuf_fd) {
  struct local_dma_buf_sync sync_end = {0};

  sync_end.flags = LOCAL_DMA_BUF_SYNC_END | LOCAL_DMA_BUF_SYNC_WRITE;
  if (drmIoctl(dmabuf_fd, LOCAL_DMA_BUF_IOCTL_SYNC, &sync_end))
    PLOG(ERROR) << "Failed DMA_BUF_SYNC_END";
}

}  // namespace

// static
std::unique_ptr<ClientNativePixmap> ClientNativePixmapDmaBuf::ImportFromDmabuf(
    int dmabuf_fd,
    const gfx::Size& size,
    int stride) {
  DCHECK_GE(dmabuf_fd, 0);
  return base::WrapUnique(
      new ClientNativePixmapDmaBuf(dmabuf_fd, size, stride));
}

ClientNativePixmapDmaBuf::ClientNativePixmapDmaBuf(int dmabuf_fd,
                                                   const gfx::Size& size,
                                                   int stride)
    : dmabuf_fd_(dmabuf_fd), size_(size), stride_(stride) {
  TRACE_EVENT0("drm", "ClientNativePixmapDmaBuf");
  size_t map_size = stride_ * size_.height();
  data_ = mmap(nullptr, map_size, (PROT_READ | PROT_WRITE), MAP_SHARED,
               dmabuf_fd, 0);
  if (data_ == MAP_FAILED) {
    PLOG(ERROR) << "Failed mmap().";
    base::TerminateBecauseOutOfMemory(map_size);
  }
}

ClientNativePixmapDmaBuf::~ClientNativePixmapDmaBuf() {
  TRACE_EVENT0("drm", "~ClientNativePixmapDmaBuf");
  size_t size = stride_ * size_.height();
  int ret = munmap(data_, size);
  DCHECK(!ret);
}

void* ClientNativePixmapDmaBuf::Map() {
  TRACE_EVENT0("drm", "DmaBuf:Map");
  PrimeSyncStart(dmabuf_fd_.get());
  return data_;
}

void ClientNativePixmapDmaBuf::Unmap() {
  TRACE_EVENT0("drm", "DmaBuf:Unmap");
  PrimeSyncEnd(dmabuf_fd_.get());
}

void ClientNativePixmapDmaBuf::GetStride(int* stride) const {
  *stride = stride_;
}

}  // namespace ui
