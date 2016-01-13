// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_FAVICON_URL_
#define CONTENT_PUBLIC_COMMON_FAVICON_URL_

#include <vector>

#include "content/common/content_export.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace content {

// The favicon url from the render.
struct CONTENT_EXPORT FaviconURL {
  // The icon type in a page. The definition must be same as
  // favicon_base::IconType.
  enum IconType {
    INVALID_ICON = 0x0,
    FAVICON = 1 << 0,
    TOUCH_ICON = 1 << 1,
    TOUCH_PRECOMPOSED_ICON = 1 << 2
  };

  FaviconURL();
  FaviconURL(const GURL& url,
             IconType type,
             const std::vector<gfx::Size>& sizes);
  ~FaviconURL();

  // The url of the icon.
  GURL icon_url;

  // The type of the icon
  IconType icon_type;

  // Icon's bitmaps' size
  std::vector<gfx::Size> icon_sizes;
};

} // namespace content

#endif  // CONTENT_PUBLIC_COMMON_FAVICON_URL_
