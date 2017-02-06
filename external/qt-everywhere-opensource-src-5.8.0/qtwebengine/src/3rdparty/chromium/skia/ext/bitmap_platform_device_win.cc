// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <psapi.h>
#include <stddef.h>

#include "base/debug/gdi_debug_util_win.h"
#include "base/logging.h"
#include "base/win/win_util.h"
#include "skia/ext/bitmap_platform_device_win.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkRect.h"

namespace {

HBITMAP CreateHBitmap(int width, int height, bool is_opaque,
                             HANDLE shared_section, void** data) {
  // CreateDIBSection appears to get unhappy if we create an empty bitmap, so
  // just create a minimal bitmap
  if ((width == 0) || (height == 0)) {
    width = 1;
    height = 1;
  }

  BITMAPINFOHEADER hdr = {0};
  hdr.biSize = sizeof(BITMAPINFOHEADER);
  hdr.biWidth = width;
  hdr.biHeight = -height;  // minus means top-down bitmap
  hdr.biPlanes = 1;
  hdr.biBitCount = 32;
  hdr.biCompression = BI_RGB;  // no compression
  hdr.biSizeImage = 0;
  hdr.biXPelsPerMeter = 1;
  hdr.biYPelsPerMeter = 1;
  hdr.biClrUsed = 0;
  hdr.biClrImportant = 0;

  HBITMAP hbitmap = CreateDIBSection(NULL, reinterpret_cast<BITMAPINFO*>(&hdr),
                                     0, data, shared_section, 0);

#if !defined(_WIN64)
  // If this call fails, we're gonna crash hard. Try to get some useful
  // information out before we crash for post-mortem analysis.
  if (!hbitmap)
    base::debug::GDIBitmapAllocFailure(&hdr, shared_section);
#endif

  return hbitmap;
}

void LoadTransformToDC(HDC dc, const SkMatrix& matrix) {
  XFORM xf;
  xf.eM11 = matrix[SkMatrix::kMScaleX];
  xf.eM21 = matrix[SkMatrix::kMSkewX];
  xf.eDx = matrix[SkMatrix::kMTransX];
  xf.eM12 = matrix[SkMatrix::kMSkewY];
  xf.eM22 = matrix[SkMatrix::kMScaleY];
  xf.eDy = matrix[SkMatrix::kMTransY];
  SetWorldTransform(dc, &xf);
}

void LoadClippingRegionToDC(HDC context,
                            const SkIRect& clip_bounds,
                            const SkMatrix& transformation) {
  HRGN hrgn = CreateRectRgnIndirect(&skia::SkIRectToRECT(clip_bounds));
  int result = SelectClipRgn(context, hrgn);
  SkASSERT(result != ERROR);
  result = DeleteObject(hrgn);
  SkASSERT(result != 0);
}

}  // namespace

namespace skia {

void DrawToNativeContext(SkCanvas* canvas, HDC destination_hdc, int x, int y,
                         const RECT* src_rect) {
  ScopedPlatformPaint p(canvas);
  PlatformDevice* platform_device = GetPlatformDevice(GetTopDevice(*canvas));
  if (platform_device)
    platform_device->DrawToHDC(p.GetPlatformSurface(), destination_hdc, x, y,
                               src_rect, canvas->getTotalMatrix());

}

void PlatformDevice::DrawToHDC(HDC, HDC, int x, int y, const RECT* src_rect,
                               const SkMatrix& transform) {}

HDC BitmapPlatformDevice::GetBitmapDC(const SkMatrix& transform,
                                      const SkIRect& clip_bounds) {
  if (!hdc_) {
    hdc_ = CreateCompatibleDC(NULL);
    InitializeDC(hdc_);
    old_hbitmap_ = static_cast<HBITMAP>(SelectObject(hdc_, hbitmap_));
  }

  LoadConfig(transform, clip_bounds);
  return hdc_;
}

void BitmapPlatformDevice::ReleaseBitmapDC() {
  SkASSERT(hdc_);
  SelectObject(hdc_, old_hbitmap_);
  DeleteDC(hdc_);
  hdc_ = NULL;
  old_hbitmap_ = NULL;
}

bool BitmapPlatformDevice::IsBitmapDCCreated()
    const {
  return hdc_ != NULL;
}

void BitmapPlatformDevice::LoadConfig(const SkMatrix& transform,
                                      const SkIRect& clip_bounds) {
  if (!hdc_)
    return;  // Nothing to do.

  // Transform.
  LoadTransformToDC(hdc_, transform);
  LoadClippingRegionToDC(hdc_, clip_bounds, transform);
}

static void DeleteHBitmapCallback(void* addr, void* context) {
  // If context is not NULL then it's a valid HBITMAP to delete.
  // Otherwise we just unmap the pixel memory.
  if (context)
    DeleteObject(static_cast<HBITMAP>(context));
  else
    UnmapViewOfFile(addr);
}

static bool InstallHBitmapPixels(SkBitmap* bitmap, int width, int height,
                                 bool is_opaque, void* data, HBITMAP hbitmap) {
  const SkAlphaType at = is_opaque ? kOpaque_SkAlphaType : kPremul_SkAlphaType;
  const SkImageInfo info = SkImageInfo::MakeN32(width, height, at);
  const size_t rowBytes = info.minRowBytes();
  SkColorTable* color_table = NULL;
  return bitmap->installPixels(info, data, rowBytes, color_table,
                               DeleteHBitmapCallback, hbitmap);
}

// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDevice* BitmapPlatformDevice::Create(
    int width,
    int height,
    bool is_opaque,
    HANDLE shared_section,
    bool do_clear) {

  void* data;
  HBITMAP hbitmap = NULL;

  // This function contains an implementation of a Skia platform bitmap for
  // drawing and compositing graphics. The original implementation uses Windows
  // GDI to create the backing bitmap memory, however it's possible for a
  // process to not have access to GDI which will cause this code to fail. It's
  // possible to detect when GDI is unavailable and instead directly map the
  // shared memory as the bitmap.
  if (base::win::IsUser32AndGdi32Available()) {
    hbitmap = CreateHBitmap(width, height, is_opaque, shared_section, &data);
    if (!hbitmap) {
      LOG(ERROR) << "CreateHBitmap failed";
      return NULL;
    }
  } else {
    DCHECK(shared_section != NULL);
    data = MapViewOfFile(shared_section, FILE_MAP_WRITE, 0, 0,
                         PlatformCanvasStrideForWidth(width) * height);
    if (!data) {
      LOG(ERROR) << "MapViewOfFile failed";
      return NULL;
    }
  }

  SkBitmap bitmap;
  if (!InstallHBitmapPixels(&bitmap, width, height, is_opaque, data, hbitmap)) {
    LOG(ERROR) << "InstallHBitmapPixels failed";
    return NULL;
  }

  if (do_clear)
    bitmap.eraseColor(0);

#ifndef NDEBUG
  // If we were given data, then don't clobber it!
  if (!shared_section && is_opaque)
    // To aid in finding bugs, we set the background color to something
    // obviously wrong so it will be noticable when it is not cleared
    bitmap.eraseARGB(255, 0, 255, 128);  // bright bluish green
#endif

  // The device object will take ownership of the HBITMAP. The initial refcount
  // of the data object will be 1, which is what the constructor expects.
  return new BitmapPlatformDevice(hbitmap, bitmap);
}

// static
BitmapPlatformDevice* BitmapPlatformDevice::Create(int width, int height,
                                                   bool is_opaque) {
  const HANDLE shared_section = NULL;
  const bool do_clear = false;
  return Create(width, height, is_opaque, shared_section, do_clear);
}

// The device will own the HBITMAP, which corresponds to also owning the pixel
// data. Therefore, we do not transfer ownership to the SkBitmapDevice's bitmap.
BitmapPlatformDevice::BitmapPlatformDevice(
    HBITMAP hbitmap,
    const SkBitmap& bitmap)
    : SkBitmapDevice(bitmap),
      hbitmap_(hbitmap),
      old_hbitmap_(NULL),
      hdc_(NULL) {
  // The data object is already ref'ed for us by create().
  if (hbitmap) {
    SetPlatformDevice(this, this);
    BITMAP bitmap_data;
    GetObject(hbitmap_, sizeof(BITMAP), &bitmap_data);
  }
}

BitmapPlatformDevice::~BitmapPlatformDevice() {
  if (hdc_)
    ReleaseBitmapDC();
}

HDC BitmapPlatformDevice::BeginPlatformPaint(const SkMatrix& transform,
                                             const SkIRect& clip_bounds) {
  return GetBitmapDC(transform, clip_bounds);
}

void BitmapPlatformDevice::DrawToHDC(HDC source_dc, HDC destination_dc,
                                     int x, int y,
                                     const RECT* src_rect,
                                     const SkMatrix& transform) {
  bool created_dc = !IsBitmapDCCreated();

  RECT temp_rect;
  if (!src_rect) {
    temp_rect.left = 0;
    temp_rect.right = width();
    temp_rect.top = 0;
    temp_rect.bottom = height();
    src_rect = &temp_rect;
  }

  int copy_width = src_rect->right - src_rect->left;
  int copy_height = src_rect->bottom - src_rect->top;

  // We need to reset the translation for our bitmap or (0,0) won't be in the
  // upper left anymore
  SkMatrix identity;
  identity.reset();

  LoadTransformToDC(source_dc, identity);
  if (isOpaque()) {
    BitBlt(destination_dc,
           x,
           y,
           copy_width,
           copy_height,
           source_dc,
           src_rect->left,
           src_rect->top,
           SRCCOPY);
  } else {
    SkASSERT(copy_width != 0 && copy_height != 0);
    BLENDFUNCTION blend_function = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    GdiAlphaBlend(destination_dc,
                  x,
                  y,
                  copy_width,
                  copy_height,
                  source_dc,
                  src_rect->left,
                  src_rect->top,
                  copy_width,
                  copy_height,
                  blend_function);
  }
  LoadTransformToDC(source_dc, transform);

  if (created_dc)
    ReleaseBitmapDC();
}

const SkBitmap& BitmapPlatformDevice::onAccessBitmap() {
  // FIXME(brettw) OPTIMIZATION: We should only flush if we know a GDI
  // operation has occurred on our DC.
  if (IsBitmapDCCreated())
    GdiFlush();
  return SkBitmapDevice::onAccessBitmap();
}

SkBaseDevice* BitmapPlatformDevice::onCreateDevice(const CreateInfo& cinfo,
                                                   const SkPaint*) {
  const SkImageInfo& info = cinfo.fInfo;
  const bool do_clear = !info.isOpaque();
  SkASSERT(info.colorType() == kN32_SkColorType);
  return Create(info.width(), info.height(), info.isOpaque(), NULL, do_clear);
}

// PlatformCanvas impl

SkCanvas* CreatePlatformCanvas(int width,
                               int height,
                               bool is_opaque,
                               HANDLE shared_section,
                               OnFailureType failureType) {
  sk_sp<SkBaseDevice> dev(
      BitmapPlatformDevice::Create(width, height, is_opaque, shared_section));
  return CreateCanvas(dev, failureType);
}

}  // namespace skia
