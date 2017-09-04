// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_ICON_CACHER_H_
#define COMPONENTS_NTP_TILES_ICON_CACHER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/ntp_tiles/popular_sites.h"

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace favicon_base {
struct FaviconImageResult;
}  // namespace favicon_base

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageFetcher;
}  // namespace image_fetcher

namespace ntp_tiles {

// Ensures that a Popular Sites icon is cached, downloading and saving it if
// not.
//
// Does not provide any way to get a fetched favicon; use the FaviconService for
// that. All this class does is guarantee that FaviconService will be able to
// get you an icon (if it exists).
class IconCacher {
 public:
  IconCacher(favicon::FaviconService* favicon_service,
             std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher);
  ~IconCacher();

  // Fetches the icon if necessary, then invokes |done| with true if it was
  // newly fetched (false if it was already cached or could not be fetched).
  void StartFetch(PopularSites::Site site,
                  const base::Callback<void(bool)>& done);

 private:
  void OnGetFaviconImageForPageURLFinished(
      PopularSites::Site site,
      const base::Callback<void(bool)>& done,
      const favicon_base::FaviconImageResult& result);

  void OnFaviconDownloaded(PopularSites::Site site,
                           const base::Callback<void(bool)>& done,
                           const std::string& id,
                           const gfx::Image& fetched_image);

  base::CancelableTaskTracker tracker_;
  favicon::FaviconService* const favicon_service_;
  std::unique_ptr<image_fetcher::ImageFetcher> const image_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(IconCacher);
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_ICON_CACHER_H_
