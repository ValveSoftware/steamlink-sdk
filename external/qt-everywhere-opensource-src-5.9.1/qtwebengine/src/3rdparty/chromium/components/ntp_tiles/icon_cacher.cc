// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/icon_cacher.h"

#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/favicon_util.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_util.h"
#include "components/image_fetcher/image_fetcher.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace ntp_tiles {

namespace {

favicon_base::IconType IconType(const PopularSites::Site& site) {
  return site.large_icon_url.is_valid() ? favicon_base::TOUCH_ICON
                                        : favicon_base::FAVICON;
}

const GURL& IconURL(const PopularSites::Site& site) {
  return site.large_icon_url.is_valid() ? site.large_icon_url
                                        : site.favicon_url;
}

}  // namespace

IconCacher::IconCacher(
    favicon::FaviconService* favicon_service,
    std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher)
    : favicon_service_(favicon_service),
      image_fetcher_(std::move(image_fetcher)) {
  image_fetcher_->SetDataUseServiceName(
      data_use_measurement::DataUseUserData::NTP_TILES);
}

IconCacher::~IconCacher() = default;

void IconCacher::StartFetch(PopularSites::Site site,
                            const base::Callback<void(bool)>& done) {
  favicon::GetFaviconImageForPageURL(
      favicon_service_, site.url, IconType(site),
      base::Bind(&IconCacher::OnGetFaviconImageForPageURLFinished,
                 base::Unretained(this), std::move(site), done),
      &tracker_);
}

void IconCacher::OnGetFaviconImageForPageURLFinished(
    PopularSites::Site site,
    const base::Callback<void(bool)>& done,
    const favicon_base::FaviconImageResult& result) {
  if (!result.image.IsEmpty()) {
    done.Run(false);
    return;
  }

  image_fetcher_->StartOrQueueNetworkRequest(
      std::string(), IconURL(site),
      base::Bind(&IconCacher::OnFaviconDownloaded, base::Unretained(this), site,
                 done));
}

void IconCacher::OnFaviconDownloaded(PopularSites::Site site,
                                     const base::Callback<void(bool)>& done,
                                     const std::string& id,
                                     const gfx::Image& fetched_image) {
  if (fetched_image.IsEmpty()) {
    done.Run(false);
    return;
  }

  gfx::Image image = fetched_image;
  favicon_base::SetFaviconColorSpace(&image);
  favicon_service_->SetFavicons(site.url, IconURL(site), IconType(site), image);
  done.Run(true);
}

}  // namespace ntp_tiles
