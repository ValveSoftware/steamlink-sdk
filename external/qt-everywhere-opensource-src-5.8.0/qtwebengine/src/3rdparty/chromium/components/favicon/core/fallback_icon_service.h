// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FALLBACK_ICON_SERVICE_H_
#define COMPONENTS_FAVICON_CORE_FALLBACK_ICON_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace gfx {
class Canvas;
}

namespace favicon_base {
struct FallbackIconStyle;
}

namespace favicon {

class FallbackIconClient;

// A service to provide methods to render fallback favicons.
class FallbackIconService : public KeyedService {
 public:
  explicit FallbackIconService(FallbackIconClient* fallback_icon_client);
  ~FallbackIconService() override;

  // Renders a fallback icon synchronously and returns the bitmap. Returns an
  // empty std::vector on failure. |size| is icon width and height in pixels.
  std::vector<unsigned char> RenderFallbackIconBitmap(
      const GURL& icon_url,
      int size,
      const favicon_base::FallbackIconStyle& style);

 private:
  // Renders a fallback icon on |canvas| at position (|x|, |y|). |size| is icon
  // width and height in pixels.
  void DrawFallbackIcon(const GURL& icon_url,
                        int size,
                        const favicon_base::FallbackIconStyle& style,
                        gfx::Canvas* canvas);

  FallbackIconClient* fallback_icon_client_;

  DISALLOW_COPY_AND_ASSIGN(FallbackIconService);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FALLBACK_ICON_SERVICE_H_
