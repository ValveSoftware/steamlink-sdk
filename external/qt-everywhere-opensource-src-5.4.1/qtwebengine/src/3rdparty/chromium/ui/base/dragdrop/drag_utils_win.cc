// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drag_utils.h"

#include <objidl.h>
#include <shlobj.h>
#include <shobjidl.h>

#include "base/win/scoped_comptr.h"
#include "base/win/scoped_hdc.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/gdi_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skbitmap_operations.h"

namespace drag_utils {

static void SetDragImageOnDataObject(HBITMAP hbitmap,
                                     const gfx::Size& size_in_pixels,
                                     const gfx::Vector2d& cursor_offset,
                                     IDataObject* data_object) {
  base::win::ScopedComPtr<IDragSourceHelper> helper;
  HRESULT rv = CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER,
                                IID_IDragSourceHelper, helper.ReceiveVoid());
  if (SUCCEEDED(rv)) {
    SHDRAGIMAGE sdi;
    sdi.sizeDragImage = size_in_pixels.ToSIZE();
    sdi.crColorKey = 0xFFFFFFFF;
    sdi.hbmpDragImage = hbitmap;
    sdi.ptOffset = gfx::PointAtOffsetFromOrigin(cursor_offset).ToPOINT();
    helper->InitializeFromBitmap(&sdi, data_object);
  }
}

// Blit the contents of the canvas to a new HBITMAP. It is the caller's
// responsibility to release the |bits| buffer.
static HBITMAP CreateHBITMAPFromSkBitmap(const SkBitmap& sk_bitmap) {
  base::win::ScopedGetDC screen_dc(NULL);
  BITMAPINFOHEADER header;
  gfx::CreateBitmapHeader(sk_bitmap.width(), sk_bitmap.height(), &header);
  void* bits;
  HBITMAP bitmap =
      CreateDIBSection(screen_dc, reinterpret_cast<BITMAPINFO*>(&header),
                       DIB_RGB_COLORS, &bits, NULL, 0);
  if (!bitmap || !bits)
    return NULL;
  DCHECK_EQ(sk_bitmap.rowBytes(), static_cast<size_t>(sk_bitmap.width() * 4));
  SkAutoLockPixels lock(sk_bitmap);
  memcpy(
      bits, sk_bitmap.getPixels(), sk_bitmap.height() * sk_bitmap.rowBytes());
  return bitmap;
}

void SetDragImageOnDataObject(const gfx::ImageSkia& image_skia,
                              const gfx::Vector2d& cursor_offset,
                              ui::OSExchangeData* data_object) {
  DCHECK(data_object && !image_skia.size().IsEmpty());
  // InitializeFromBitmap() doesn't expect an alpha channel and is confused
  // by premultiplied colors, so unpremultiply the bitmap.
  // SetDragImageOnDataObject(HBITMAP) takes ownership of the bitmap.
  HBITMAP bitmap = CreateHBITMAPFromSkBitmap(
      SkBitmapOperations::UnPreMultiply(*image_skia.bitmap()));
  if (bitmap) {
    // Attach 'bitmap' to the data_object.
    SetDragImageOnDataObject(
        bitmap,
        gfx::Size(image_skia.bitmap()->width(), image_skia.bitmap()->height()),
        cursor_offset,
        ui::OSExchangeDataProviderWin::GetIDataObject(*data_object));
  }

  // TODO: the above code is used in non-Ash, while below is used in Ash. If we
  // could figure this context out then we wouldn't do unnecessary work. However
  // as it stands getting this information in ui/base would be a layering
  // violation.
  data_object->provider().SetDragImage(image_skia, cursor_offset);
}

}  // namespace drag_utils
