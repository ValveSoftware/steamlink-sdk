// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_image_data_impl.h"

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/view_messages.h"
#include "content/renderer/pepper/common.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/thunk/thunk.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "ui/surface/transport_dib.h"

using ppapi::thunk::PPB_ImageData_API;

namespace content {

namespace {
// Returns true if the ImageData shared memory should be allocated in the
// browser process for the current platform.
bool IsBrowserAllocated() {
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  // On the Mac, shared memory has to be created in the browser in order to
  // work in the sandbox.
  return true;
#endif
  return false;
}
}  // namespace

PPB_ImageData_Impl::PPB_ImageData_Impl(PP_Instance instance,
                                       PPB_ImageData_Shared::ImageDataType type)
    : Resource(ppapi::OBJECT_IS_IMPL, instance),
      format_(PP_IMAGEDATAFORMAT_BGRA_PREMUL),
      width_(0),
      height_(0) {
  switch (type) {
    case PPB_ImageData_Shared::PLATFORM:
      backend_.reset(new ImageDataPlatformBackend(IsBrowserAllocated()));
      return;
    case PPB_ImageData_Shared::SIMPLE:
      backend_.reset(new ImageDataSimpleBackend);
      return;
      // No default: so that we get a compiler warning if any types are added.
  }
  NOTREACHED();
}

PPB_ImageData_Impl::PPB_ImageData_Impl(PP_Instance instance, ForTest)
    : Resource(ppapi::OBJECT_IS_IMPL, instance),
      format_(PP_IMAGEDATAFORMAT_BGRA_PREMUL),
      width_(0),
      height_(0) {
  backend_.reset(new ImageDataPlatformBackend(false));
}

PPB_ImageData_Impl::~PPB_ImageData_Impl() {}

bool PPB_ImageData_Impl::Init(PP_ImageDataFormat format,
                              int width,
                              int height,
                              bool init_to_zero) {
  // TODO(brettw) this should be called only on the main thread!
  if (!IsImageDataFormatSupported(format))
    return false;  // Only support this one format for now.
  if (width <= 0 || height <= 0)
    return false;
  if (static_cast<int64>(width) * static_cast<int64>(height) >=
      std::numeric_limits<int32>::max() / 4)
    return false;  // Prevent overflow of signed 32-bit ints.

  format_ = format;
  width_ = width;
  height_ = height;
  return backend_->Init(this, format, width, height, init_to_zero);
}

// static
PP_Resource PPB_ImageData_Impl::Create(PP_Instance instance,
                                       PPB_ImageData_Shared::ImageDataType type,
                                       PP_ImageDataFormat format,
                                       const PP_Size& size,
                                       PP_Bool init_to_zero) {
  scoped_refptr<PPB_ImageData_Impl> data(
      new PPB_ImageData_Impl(instance, type));
  if (!data->Init(format, size.width, size.height, !!init_to_zero))
    return 0;
  return data->GetReference();
}

PPB_ImageData_API* PPB_ImageData_Impl::AsPPB_ImageData_API() { return this; }

bool PPB_ImageData_Impl::IsMapped() const { return backend_->IsMapped(); }

TransportDIB* PPB_ImageData_Impl::GetTransportDIB() const {
  return backend_->GetTransportDIB();
}

PP_Bool PPB_ImageData_Impl::Describe(PP_ImageDataDesc* desc) {
  desc->format = format_;
  desc->size.width = width_;
  desc->size.height = height_;
  desc->stride = width_ * 4;
  return PP_TRUE;
}

void* PPB_ImageData_Impl::Map() { return backend_->Map(); }

void PPB_ImageData_Impl::Unmap() { backend_->Unmap(); }

int32_t PPB_ImageData_Impl::GetSharedMemory(int* handle, uint32_t* byte_count) {
  return backend_->GetSharedMemory(handle, byte_count);
}

skia::PlatformCanvas* PPB_ImageData_Impl::GetPlatformCanvas() {
  return backend_->GetPlatformCanvas();
}

SkCanvas* PPB_ImageData_Impl::GetCanvas() { return backend_->GetCanvas(); }

void PPB_ImageData_Impl::SetIsCandidateForReuse() {
  // Nothing to do since we don't support image data re-use in-process.
}

const SkBitmap* PPB_ImageData_Impl::GetMappedBitmap() const {
  return backend_->GetMappedBitmap();
}

// ImageDataPlatformBackend ----------------------------------------------------

ImageDataPlatformBackend::ImageDataPlatformBackend(bool is_browser_allocated)
    : width_(0), height_(0), is_browser_allocated_(is_browser_allocated) {}

// On POSIX, we have to tell the browser to free the transport DIB.
ImageDataPlatformBackend::~ImageDataPlatformBackend() {
  if (is_browser_allocated_) {
#if defined(OS_POSIX)
    if (dib_) {
      RenderThreadImpl::current()->Send(
          new ViewHostMsg_FreeTransportDIB(dib_->id()));
    }
#endif
  }
}

bool ImageDataPlatformBackend::Init(PPB_ImageData_Impl* impl,
                                    PP_ImageDataFormat format,
                                    int width,
                                    int height,
                                    bool init_to_zero) {
  // TODO(brettw) use init_to_zero when we implement caching.
  width_ = width;
  height_ = height;
  uint32 buffer_size = width_ * height_ * 4;

  // Allocate the transport DIB and the PlatformCanvas pointing to it.
  TransportDIB* dib = NULL;
  if (is_browser_allocated_) {
#if defined(OS_POSIX)
    // Allocate the image data by sending a message to the browser requesting a
    // TransportDIB (see also chrome/renderer/webplugin_delegate_proxy.cc,
    // method WebPluginDelegateProxy::CreateBitmap() for similar code). The
    // TransportDIB is cached in the browser, and is freed (in typical cases) by
    // the TransportDIB's destructor.
    TransportDIB::Handle dib_handle;
    IPC::Message* msg =
        new ViewHostMsg_AllocTransportDIB(buffer_size, true, &dib_handle);
    if (!RenderThreadImpl::current()->Send(msg))
      return false;
    if (!TransportDIB::is_valid_handle(dib_handle))
      return false;

    dib = TransportDIB::CreateWithHandle(dib_handle);
#endif
  } else {
    static int next_dib_id = 0;
    dib = TransportDIB::Create(buffer_size, next_dib_id++);
    if (!dib)
      return false;
  }
  DCHECK(dib);
  dib_.reset(dib);
  return true;
}

bool ImageDataPlatformBackend::IsMapped() const {
  return !!mapped_canvas_.get();
}

TransportDIB* ImageDataPlatformBackend::GetTransportDIB() const {
  return dib_.get();
}

void* ImageDataPlatformBackend::Map() {
  if (!mapped_canvas_) {
    mapped_canvas_.reset(dib_->GetPlatformCanvas(width_, height_));
    if (!mapped_canvas_)
      return NULL;
  }
  const SkBitmap& bitmap =
      skia::GetTopDevice(*mapped_canvas_)->accessBitmap(true);

  // Our platform bitmaps are set to opaque by default, which we don't want.
  const_cast<SkBitmap&>(bitmap).setAlphaType(kPremul_SkAlphaType);

  bitmap.lockPixels();
  return bitmap.getAddr32(0, 0);
}

void ImageDataPlatformBackend::Unmap() {
  // This is currently unimplemented, which is OK. The data will just always
  // be around once it's mapped. Chrome's TransportDIB isn't currently
  // unmappable without freeing it, but this may be something we want to support
  // in the future to save some memory.
}

int32_t ImageDataPlatformBackend::GetSharedMemory(int* handle,
                                                  uint32_t* byte_count) {
  *byte_count = dib_->size();
#if defined(OS_WIN)
  *handle = reinterpret_cast<intptr_t>(dib_->handle());
#else
  *handle = static_cast<intptr_t>(dib_->handle().fd);
#endif

  return PP_OK;
}

skia::PlatformCanvas* ImageDataPlatformBackend::GetPlatformCanvas() {
  return mapped_canvas_.get();
}

SkCanvas* ImageDataPlatformBackend::GetCanvas() { return mapped_canvas_.get(); }

const SkBitmap* ImageDataPlatformBackend::GetMappedBitmap() const {
  if (!mapped_canvas_)
    return NULL;
  return &skia::GetTopDevice(*mapped_canvas_)->accessBitmap(false);
}

// ImageDataSimpleBackend ------------------------------------------------------

ImageDataSimpleBackend::ImageDataSimpleBackend() : map_count_(0) {}

ImageDataSimpleBackend::~ImageDataSimpleBackend() {}

bool ImageDataSimpleBackend::Init(PPB_ImageData_Impl* impl,
                                  PP_ImageDataFormat format,
                                  int width,
                                  int height,
                                  bool init_to_zero) {
  skia_bitmap_.setConfig(
      SkBitmap::kARGB_8888_Config, impl->width(), impl->height());
  shared_memory_.reset(
      RenderThread::Get()
          ->HostAllocateSharedMemoryBuffer(skia_bitmap_.getSize())
          .release());
  return !!shared_memory_.get();
}

bool ImageDataSimpleBackend::IsMapped() const { return map_count_ > 0; }

TransportDIB* ImageDataSimpleBackend::GetTransportDIB() const { return NULL; }

void* ImageDataSimpleBackend::Map() {
  DCHECK(shared_memory_.get());
  if (map_count_++ == 0) {
    shared_memory_->Map(skia_bitmap_.getSize());
    skia_bitmap_.setPixels(shared_memory_->memory());
    // Our platform bitmaps are set to opaque by default, which we don't want.
    skia_bitmap_.setAlphaType(kPremul_SkAlphaType);
    skia_canvas_.reset(new SkCanvas(skia_bitmap_));
    return skia_bitmap_.getAddr32(0, 0);
  }
  return shared_memory_->memory();
}

void ImageDataSimpleBackend::Unmap() {
  if (--map_count_ == 0)
    shared_memory_->Unmap();
}

int32_t ImageDataSimpleBackend::GetSharedMemory(int* handle,
                                                uint32_t* byte_count) {
  *byte_count = skia_bitmap_.getSize();
#if defined(OS_POSIX)
  *handle = shared_memory_->handle().fd;
#elif defined(OS_WIN)
  *handle = reinterpret_cast<int>(shared_memory_->handle());
#else
#error "Platform not supported."
#endif
  return PP_OK;
}

skia::PlatformCanvas* ImageDataSimpleBackend::GetPlatformCanvas() {
  return NULL;
}

SkCanvas* ImageDataSimpleBackend::GetCanvas() {
  if (!IsMapped())
    return NULL;
  return skia_canvas_.get();
}

const SkBitmap* ImageDataSimpleBackend::GetMappedBitmap() const {
  if (!IsMapped())
    return NULL;
  return &skia_bitmap_;
}

}  // namespace content
