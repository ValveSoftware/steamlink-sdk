// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_FAVICON_TYPES_H_
#define COMPONENTS_FAVICON_BASE_FAVICON_TYPES_H_

#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace favicon_base {

struct FallbackIconStyle;

typedef int64_t FaviconID;

// Defines the icon types. They are also stored in icon_type field of favicons
// table.
// The values of the IconTypes are used to select the priority in which favicon
// data is returned in HistoryBackend and ThumbnailDatabase. Data for the
// largest IconType takes priority if data for multiple IconTypes is available.
enum IconType {
  INVALID_ICON = 0x0,
  FAVICON = 1 << 0,
  TOUCH_ICON = 1 << 1,
  TOUCH_PRECOMPOSED_ICON = 1 << 2
};

// Defines a gfx::Image of size desired_size_in_dip composed of image
// representations for each of the desired scale factors.
struct FaviconImageResult {
  FaviconImageResult();
  ~FaviconImageResult();

  // The resulting image.
  gfx::Image image;

  // The URL of the favicon which contains all of the image representations of
  // |image|.
  // TODO(pkotwicz): Return multiple |icon_urls| to allow |image| to have
  // representations from several favicons once content::FaviconStatus supports
  // multiple URLs.
  GURL icon_url;
};

// Defines a favicon bitmap which best matches the desired DIP size and one of
// the desired scale factors.
struct FaviconRawBitmapResult {
  FaviconRawBitmapResult();
  FaviconRawBitmapResult(const FaviconRawBitmapResult& other);
  ~FaviconRawBitmapResult();

  // Returns true if |bitmap_data| contains a valid bitmap.
  bool is_valid() const { return bitmap_data.get() && bitmap_data->size(); }

  // Indicates whether |bitmap_data| is expired.
  bool expired;

  // The bits of the bitmap.
  scoped_refptr<base::RefCountedMemory> bitmap_data;

  // The pixel dimensions of |bitmap_data|.
  gfx::Size pixel_size;

  // The URL of the containing favicon.
  GURL icon_url;

  // The icon type of the containing favicon.
  IconType icon_type;
};

// Define type with same structure as FaviconRawBitmapResult for passing data to
// HistoryBackend::SetFavicons().
typedef FaviconRawBitmapResult FaviconRawBitmapData;

// Result returned by LargeIconService::GetLargeIconOrFallbackStyle(). Contains
// either the bitmap data if the favicon database has a sufficiently large
// favicon bitmap and the style of the fallback icon otherwise.
struct LargeIconResult {
  explicit LargeIconResult(const FaviconRawBitmapResult& bitmap_in);

  // Takes ownership of |fallback_icon_style_in|.
  explicit LargeIconResult(FallbackIconStyle* fallback_icon_style_in);

  ~LargeIconResult();

  // The bitmap from the favicon database if the database has a sufficiently
  // large one.
  FaviconRawBitmapResult bitmap;

  // The fallback icon style if a sufficiently large icon isn't available. This
  // uses the dominant color of a smaller icon as the background if available.
  std::unique_ptr<FallbackIconStyle> fallback_icon_style;
};

}  // namespace favicon_base

#endif  // COMPONENTS_FAVICON_BASE_FAVICON_TYPES_H_
