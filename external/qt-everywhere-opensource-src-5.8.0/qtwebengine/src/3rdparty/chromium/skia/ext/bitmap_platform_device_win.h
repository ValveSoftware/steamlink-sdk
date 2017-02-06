// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_BITMAP_PLATFORM_DEVICE_WIN_H_
#define SKIA_EXT_BITMAP_PLATFORM_DEVICE_WIN_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "skia/ext/platform_device.h"

namespace skia {

class ScopedPlatformPaint;

// A device is basically a wrapper around SkBitmap that provides a surface for
// SkCanvas to draw into. Our device provides a surface Windows can also write
// to. BitmapPlatformDevice creates a bitmap using CreateDIBSection() in a
// format that Skia supports and can then use this to draw ClearType into, etc.
// This pixel data is provided to the bitmap that the device contains so that it
// can be shared.
class SK_API BitmapPlatformDevice : public SkBitmapDevice, public PlatformDevice {
 public:
  // Factory function. is_opaque should be set if the caller knows the bitmap
  // will be completely opaque and allows some optimizations.
  //
  // The |shared_section| parameter is optional (pass NULL for default
  // behavior). If |shared_section| is non-null, then it must be a handle to a
  // file-mapping object returned by CreateFileMapping.  See CreateDIBSection
  // for details. If |shared_section| is null, the bitmap backing store is not
  // initialized.
  static BitmapPlatformDevice* Create(int width, int height,
                                      bool is_opaque, HANDLE shared_section,
                                      bool do_clear = false);

  // Create a BitmapPlatformDevice with no shared section. The bitmap is not
  // initialized to 0.
  static BitmapPlatformDevice* Create(int width, int height, bool is_opaque);

  ~BitmapPlatformDevice() override;

  void DrawToHDC(HDC source_dc, HDC destination_dc, int x, int y,
                 const RECT* src_rect, const SkMatrix& transform) override;

 protected:
  // Flushes the Windows device context so that the pixel data can be accessed
  // directly by Skia. Overridden from SkBaseDevice, this is called when Skia
  // starts accessing pixel data.
  const SkBitmap& onAccessBitmap() override;

  SkBaseDevice* onCreateDevice(const CreateInfo&, const SkPaint*) override;

 private:
  // PlatformDevice override
  // Retrieves the bitmap DC, which is the memory DC for our bitmap data. The
  // bitmap DC may be lazily created.
  PlatformSurface BeginPlatformPaint(const SkMatrix& transform,
                                     const SkIRect& clip_bounds) override;

  // Private constructor.
  BitmapPlatformDevice(HBITMAP hbitmap, const SkBitmap& bitmap);

  // Bitmap into which the drawing will be done. This bitmap not owned by this
  // class, but by the BitmapPlatformPixelRef inside the device's SkBitmap.
  // It's only stored here in order to lazy-create the DC (below).
  HBITMAP hbitmap_;

  // Previous bitmap held by the DC. This will be selected back before the
  // DC is destroyed.
  HBITMAP old_hbitmap_;

  // Lazily-created DC used to draw into the bitmap; see GetBitmapDC().
  HDC hdc_;

  // Create/destroy hdc_, which is the memory DC for our bitmap data.
  HDC GetBitmapDC(const SkMatrix& transform, const SkIRect& clip_bounds);
  void ReleaseBitmapDC();
  bool IsBitmapDCCreated() const;

  // Loads the current transform and clip into the context. Can be called even
  // when |hbitmap_| is NULL (will be a NOP).
  void LoadConfig(const SkMatrix& transform, const SkIRect& clip_bounds);

  DISALLOW_COPY_AND_ASSIGN(BitmapPlatformDevice);
  friend class ScopedPlatformPaint;
};

}  // namespace skia

#endif  // SKIA_EXT_BITMAP_PLATFORM_DEVICE_WIN_H_
