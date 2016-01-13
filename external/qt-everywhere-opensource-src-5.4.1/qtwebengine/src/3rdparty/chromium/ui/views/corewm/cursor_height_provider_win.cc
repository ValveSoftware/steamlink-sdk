// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/cursor_height_provider_win.h"

#include <windows.h>
#include <algorithm>
#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_hdc.h"

namespace {
typedef scoped_ptr<uint32_t> PixelData;
typedef std::map<HCURSOR, int> HeightStorage;

const uint32_t kBitsPeruint32 = sizeof(uint32_t) * 8;
// All bits are 1 for transparent portion of monochromatic mask.
const uint32_t kTransparentMask = 0xffffffff;
// This is height of default pointer arrow in Windows 7.
const int kDefaultHeight = 20;
// Masks are monochromatic.
const size_t kNumberOfColors = 2;
const size_t KHeaderAndPalette =
      sizeof(BITMAPINFOHEADER) + kNumberOfColors * sizeof(RGBQUAD);

static HeightStorage* cached_heights = NULL;

// Extracts the pixel data of provided bitmap
PixelData GetBitmapData(HBITMAP handle, const BITMAPINFO& info, HDC hdc) {
  PixelData data;
  // Masks are monochromatic.
  DCHECK_EQ(info.bmiHeader.biBitCount, 1);
  if (info.bmiHeader.biBitCount != 1)
    return data.Pass();

  // When getting pixel data palette is appended to memory pointed by
  // BITMAPINFO passed so allocate additional memory to store additional data.
  scoped_ptr<BITMAPINFO> header(
      reinterpret_cast<BITMAPINFO*>(new char[KHeaderAndPalette]));
  memcpy(header.get(), &(info.bmiHeader), sizeof(info.bmiHeader));

  data.reset(new uint32_t[info.bmiHeader.biSizeImage / sizeof(uint32_t)]);

  int result = GetDIBits(hdc,
                         handle,
                         0,
                         info.bmiHeader.biHeight,
                         data.get(),
                         header.get(),
                         DIB_RGB_COLORS);

  if (result == 0)
    data.reset();

  return data.Pass();
}

// Checks if the specifed row is transparent in provided bitmap.
bool IsRowTransparent(const PixelData& data,
                      const uint32_t row_size,
                      const uint32_t last_byte_mask,
                      const uint32_t y) {
  // Set the padding bits to 1 to make mask matching easier.
  *(data.get() + (y + 1) * row_size - 1) |= last_byte_mask;
  for (uint32_t i = y * row_size; i < (y + 1) * row_size; ++i) {
    if (*(data.get() + i) != kTransparentMask)
      return false;
  }
  return true;
}

// Gets the vertical offset between specified cursor's hotpoint and it's bottom.
//
// Gets the cursor image data and extract cursor's visible height.
// Based on that get's what should be the vertical offset between cursor's
// hot point and the tooltip.
int CalculateCursorHeight(HCURSOR cursor_handle) {
  base::win::ScopedGetDC hdc(NULL);

  ICONINFO icon = {0};
  GetIconInfo(cursor_handle, &icon);

  BITMAPINFO bitmap_info = {0};
  bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
  if (GetDIBits(hdc, icon.hbmMask, 0, 0, NULL, &bitmap_info, DIB_RGB_COLORS) ==
      0)
    return kDefaultHeight;

  // Rows are padded to full DWORDs. OR with this mask will set them to 1
  // to simplify matching with |transparent_mask|.
  uint32_t last_byte_mask = ~0;
  const unsigned char bits_to_shift = sizeof(last_byte_mask) * 8 -
      (bitmap_info.bmiHeader.biWidth % kBitsPeruint32);
  if (bits_to_shift != kBitsPeruint32)
    last_byte_mask = (last_byte_mask << bits_to_shift);
  else
    last_byte_mask = 0;

  const uint32_t row_size =
      (bitmap_info.bmiHeader.biWidth + kBitsPeruint32 - 1) / kBitsPeruint32;
  PixelData data(GetBitmapData(icon.hbmMask, bitmap_info, hdc));
  if (data == NULL)
    return kDefaultHeight;

  const int cursor_height = GetSystemMetrics(SM_CYCURSOR);
  // Crash data seems to indicate cursor_height may be bigger than the bitmap.
  int i = std::max(0, static_cast<int>(bitmap_info.bmiHeader.biHeight) -
                   cursor_height);
  for (; i < bitmap_info.bmiHeader.biHeight; ++i) {
    if (!IsRowTransparent(data, row_size, last_byte_mask, i)) {
      i--;
      break;
    }
  }
  return bitmap_info.bmiHeader.biHeight - i - icon.yHotspot;
}

}  // namespace

namespace views {
namespace corewm {

int GetCurrentCursorVisibleHeight() {
  CURSORINFO cursor = {0};
  cursor.cbSize = sizeof(cursor);
  GetCursorInfo(&cursor);

  if (cached_heights == NULL)
    cached_heights = new HeightStorage;

  HeightStorage::const_iterator cached_height =
      cached_heights->find(cursor.hCursor);
  if (cached_height != cached_heights->end())
    return cached_height->second;

  const int height = CalculateCursorHeight(cursor.hCursor);
  (*cached_heights)[cursor.hCursor] = height;

  return height;
}

}  // namespace corewm
}  // namespace views
