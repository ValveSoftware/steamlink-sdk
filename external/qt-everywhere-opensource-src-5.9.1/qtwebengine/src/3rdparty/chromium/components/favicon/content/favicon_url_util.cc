// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/favicon_url_util.h"

#include <algorithm>
#include <iterator>

#include "components/favicon/core/favicon_url.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/common/favicon_url.h"

namespace favicon {
namespace {

favicon_base::IconType IconTypeFromContentIconType(
    content::FaviconURL::IconType icon_type) {
  switch (icon_type) {
    case content::FaviconURL::FAVICON:
      return favicon_base::FAVICON;
    case content::FaviconURL::TOUCH_ICON:
      return favicon_base::TOUCH_ICON;
    case content::FaviconURL::TOUCH_PRECOMPOSED_ICON:
      return favicon_base::TOUCH_PRECOMPOSED_ICON;
    case content::FaviconURL::INVALID_ICON:
      return favicon_base::INVALID_ICON;
  }
  NOTREACHED();
  return favicon_base::INVALID_ICON;
}

}  // namespace

FaviconURL FaviconURLFromContentFaviconURL(
    const content::FaviconURL& favicon_url) {
  return FaviconURL(favicon_url.icon_url,
                    IconTypeFromContentIconType(favicon_url.icon_type),
                    favicon_url.icon_sizes);
}

std::vector<FaviconURL> FaviconURLsFromContentFaviconURLs(
    const std::vector<content::FaviconURL>& favicon_urls) {
  std::vector<FaviconURL> result;
  result.reserve(favicon_urls.size());
  std::transform(favicon_urls.begin(), favicon_urls.end(),
                 std::back_inserter(result), FaviconURLFromContentFaviconURL);
  return result;
}

}  // namespace favicon
