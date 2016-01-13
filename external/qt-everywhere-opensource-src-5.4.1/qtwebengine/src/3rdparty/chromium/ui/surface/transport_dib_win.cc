// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/surface/transport_dib.h"

#include <windows.h>

#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_info.h"
#include "skia/ext/platform_canvas.h"

TransportDIB::TransportDIB()
    : size_(0) {
}

TransportDIB::~TransportDIB() {
}

TransportDIB::TransportDIB(HANDLE handle)
    : shared_memory_(handle, false /* read write */),
      size_(0) {
}

// static
TransportDIB* TransportDIB::Create(size_t size, uint32 sequence_num) {
  TransportDIB* dib = new TransportDIB;

  if (!dib->shared_memory_.CreateAnonymous(size)) {
    delete dib;
    return NULL;
  }

  dib->size_ = size;
  dib->sequence_num_ = sequence_num;

  return dib;
}

// static
TransportDIB* TransportDIB::Map(Handle handle) {
  scoped_ptr<TransportDIB> dib(CreateWithHandle(handle));
  if (!dib->Map())
    return NULL;
  return dib.release();
}

// static
TransportDIB* TransportDIB::CreateWithHandle(Handle handle) {
  return new TransportDIB(handle);
}

// static
bool TransportDIB::is_valid_handle(Handle dib) {
  return dib != NULL;
}

// static
bool TransportDIB::is_valid_id(TransportDIB::Id id) {
  return is_valid_handle(id.handle);
}

skia::PlatformCanvas* TransportDIB::GetPlatformCanvas(int w, int h) {
  // This DIB already mapped the file into this process, but PlatformCanvas
  // will map it again.
  DCHECK(!memory()) << "Mapped file twice in the same process.";

  // We can't check the canvas size before mapping, but it's safe because
  // Windows will fail to map the section if the dimensions of the canvas
  // are too large.
  skia::PlatformCanvas* canvas =
      skia::CreatePlatformCanvas(w, h, true, handle(),
                                 skia::RETURN_NULL_ON_FAILURE);

  // Calculate the size for the memory region backing the canvas.
  if (canvas)
    size_ = skia::PlatformCanvasStrideForWidth(w) * h;

  return canvas;
}

bool TransportDIB::Map() {
  if (!is_valid_handle(handle()))
    return false;
  if (memory())
    return true;

  if (!shared_memory_.Map(0 /* map whole shared memory segment */)) {
    LOG(ERROR) << "Failed to map transport DIB"
               << " handle:" << shared_memory_.handle()
               << " error:" << ::GetLastError();
    return false;
  }

  size_ = shared_memory_.mapped_size();
  return true;
}

void* TransportDIB::memory() const {
  return shared_memory_.memory();
}

TransportDIB::Handle TransportDIB::handle() const {
  return shared_memory_.handle();
}

TransportDIB::Id TransportDIB::id() const {
  return Id(handle(), sequence_num_);
}
