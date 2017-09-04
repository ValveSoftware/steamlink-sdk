// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/core/favicon_handler.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

namespace favicon {
namespace {

// Size (along each axis) of a touch icon. This currently corresponds to
// the apple touch icon for iPad.
const int kTouchIconSize = 144;

bool DoUrlAndIconMatch(const FaviconURL& favicon_url,
                       const GURL& url,
                       favicon_base::IconType icon_type) {
  return favicon_url.icon_url == url && favicon_url.icon_type == icon_type;
}

// Returns true if all of the icon URLs and icon types in |bitmap_results| are
// identical and if they match the icon URL and icon type in |favicon_url|.
// Returns false if |bitmap_results| is empty.
bool DoUrlsAndIconsMatch(
    const FaviconURL& favicon_url,
    const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_results) {
  if (bitmap_results.empty())
    return false;

  const favicon_base::IconType icon_type = favicon_url.icon_type;

  for (const auto& bitmap_result : bitmap_results) {
    if (favicon_url.icon_url != bitmap_result.icon_url ||
        icon_type != bitmap_result.icon_type) {
      return false;
    }
  }
  return true;
}

// Return true if |bitmap_result| is expired.
bool IsExpired(const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  return bitmap_result.expired;
}

// Return true if |bitmap_result| is valid.
bool IsValid(const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  return bitmap_result.is_valid();
}

// Returns true if |bitmap_results| is non-empty and:
// - At least one of the bitmaps in |bitmap_results| is expired
// OR
// - |bitmap_results| is missing favicons for |desired_size_in_dip| and one of
//   the scale factors in favicon_base::GetFaviconScales().
bool HasExpiredOrIncompleteResult(
    int desired_size_in_dip,
    const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_results) {
  if (bitmap_results.empty())
    return false;

  // Check if at least one of the bitmaps is expired.
  std::vector<favicon_base::FaviconRawBitmapResult>::const_iterator it =
      std::find_if(bitmap_results.begin(), bitmap_results.end(), IsExpired);
  if (it != bitmap_results.end())
    return true;

  // Any favicon size is good if the desired size is 0.
  if (desired_size_in_dip == 0)
    return false;

  // Check if the favicon for at least one of the scale factors is missing.
  // |bitmap_results| should always be complete for data inserted by
  // FaviconHandler as the FaviconHandler stores favicons resized to all
  // of favicon_base::GetFaviconScales() into the history backend.
  // Examples of when |bitmap_results| can be incomplete:
  // - Favicons inserted into the history backend by sync.
  // - Favicons for imported bookmarks.
  std::vector<gfx::Size> favicon_sizes;
  for (const auto& bitmap_result : bitmap_results)
    favicon_sizes.push_back(bitmap_result.pixel_size);

  std::vector<float> favicon_scales = favicon_base::GetFaviconScales();
  for (float favicon_scale : favicon_scales) {
    int edge_size_in_pixel = std::ceil(desired_size_in_dip * favicon_scale);
    auto it = std::find(favicon_sizes.begin(), favicon_sizes.end(),
                        gfx::Size(edge_size_in_pixel, edge_size_in_pixel));
    if (it == favicon_sizes.end())
      return true;
  }
  return false;
}

// Returns true if at least one of |bitmap_results| is valid.
bool HasValidResult(
    const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_results) {
  return std::find_if(bitmap_results.begin(), bitmap_results.end(), IsValid) !=
      bitmap_results.end();
}

// Returns the index of the entry with the largest area.
int GetLargestSizeIndex(const std::vector<gfx::Size>& sizes) {
  DCHECK(!sizes.empty());
  size_t ret = 0;
  for (size_t i = 1; i < sizes.size(); ++i) {
    if (sizes[ret].GetArea() < sizes[i].GetArea())
      ret = i;
  }
  return static_cast<int>(ret);
}

// Return the index of a size which is same as the given |size|, -1 returned if
// there is no such bitmap.
int GetIndexBySize(const std::vector<gfx::Size>& sizes,
                   const gfx::Size& size) {
  DCHECK(!sizes.empty());
  std::vector<gfx::Size>::const_iterator i =
      std::find(sizes.begin(), sizes.end(), size);
  if (i == sizes.end())
    return -1;

  return static_cast<int>(i - sizes.begin());
}

// Compare function used for std::stable_sort to sort as descend.
bool CompareIconSize(const FaviconURL& b1, const FaviconURL& b2) {
  int area1 = 0;
  if (!b1.icon_sizes.empty())
    area1 = b1.icon_sizes.front().GetArea();

  int area2 = 0;
  if (!b2.icon_sizes.empty())
    area2 = b2.icon_sizes.front().GetArea();

  return area1 > area2;
}

// Sorts the entries in |image_urls| by icon size in descending order.
// Discards all but the largest size for each FaviconURL.
void SortAndPruneImageUrls(std::vector<FaviconURL>* image_urls) {
  // Not using const-reference since the loop mutates FaviconURL::icon_sizes.
  for (FaviconURL& image_url : *image_urls) {
    if (image_url.icon_sizes.empty())
      continue;

    gfx::Size largest =
        image_url.icon_sizes[GetLargestSizeIndex(image_url.icon_sizes)];
    image_url.icon_sizes.clear();
    image_url.icon_sizes.push_back(largest);
  }
  std::stable_sort(image_urls->begin(), image_urls->end(), CompareIconSize);
}

// Checks whether two FaviconURLs are equal ignoring the icon sizes.
bool FaviconURLsEqualIgnoringSizes(const FaviconURL& u1, const FaviconURL& u2) {
  return u1.icon_type == u2.icon_type && u1.icon_url == u2.icon_url;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

FaviconHandler::DownloadRequest::DownloadRequest()
    : icon_type(favicon_base::INVALID_ICON) {
}

FaviconHandler::DownloadRequest::~DownloadRequest() {
}

FaviconHandler::DownloadRequest::DownloadRequest(
    const GURL& image_url,
    favicon_base::IconType icon_type)
    : image_url(image_url), icon_type(icon_type) {
}

////////////////////////////////////////////////////////////////////////////////

FaviconHandler::FaviconCandidate::FaviconCandidate()
    : score(0), icon_type(favicon_base::INVALID_ICON) {
}

FaviconHandler::FaviconCandidate::~FaviconCandidate() {
}

FaviconHandler::FaviconCandidate::FaviconCandidate(
    const GURL& image_url,
    const gfx::Image& image,
    float score,
    favicon_base::IconType icon_type)
    : image_url(image_url),
      image(image),
      score(score),
      icon_type(icon_type) {}

////////////////////////////////////////////////////////////////////////////////

FaviconHandler::FaviconHandler(
    FaviconService* service,
    FaviconDriver* driver,
    FaviconDriverObserver::NotificationIconType handler_type)
    : handler_type_(handler_type),
      got_favicon_from_history_(false),
      initial_history_result_expired_or_incomplete_(false),
      redownload_icons_(false),
      icon_types_(FaviconHandler::GetIconTypesFromHandlerType(handler_type)),
      download_largest_icon_(
          handler_type == FaviconDriverObserver::NON_TOUCH_LARGEST ||
          handler_type == FaviconDriverObserver::TOUCH_LARGEST),
      notification_icon_type_(favicon_base::INVALID_ICON),
      service_(service),
      driver_(driver),
      current_candidate_index_(0u) {
  DCHECK(driver_);
}

FaviconHandler::~FaviconHandler() {
}

// static
int FaviconHandler::GetIconTypesFromHandlerType(
    FaviconDriverObserver::NotificationIconType handler_type) {
  switch (handler_type) {
    case FaviconDriverObserver::NON_TOUCH_16_DIP:
    case FaviconDriverObserver::NON_TOUCH_LARGEST:
      return favicon_base::FAVICON;
    case FaviconDriverObserver::TOUCH_LARGEST:
      return favicon_base::TOUCH_ICON | favicon_base::TOUCH_PRECOMPOSED_ICON;
  }
  return 0;
}

void FaviconHandler::FetchFavicon(const GURL& url) {
  cancelable_task_tracker_.TryCancelAll();

  url_ = url;

  initial_history_result_expired_or_incomplete_ = false;
  redownload_icons_ = false;
  got_favicon_from_history_ = false;
  download_requests_.clear();
  image_urls_.clear();
  notification_icon_url_ = GURL();
  notification_icon_type_ = favicon_base::INVALID_ICON;
  current_candidate_index_ = 0u;
  best_favicon_candidate_ = FaviconCandidate();

  // Request the favicon from the history service. In parallel to this the
  // renderer is going to notify us (well WebContents) when the favicon url is
  // available.
  GetFaviconForURLFromFaviconService(
      url_, icon_types_,
      base::Bind(&FaviconHandler::OnFaviconDataForInitialURLFromFaviconService,
                 base::Unretained(this)),
      &cancelable_task_tracker_);
}

bool FaviconHandler::UpdateFaviconCandidate(const GURL& image_url,
                                            const gfx::Image& image,
                                            float score,
                                            favicon_base::IconType icon_type) {
  bool replace_best_favicon_candidate = false;
  bool exact_match = false;
  if (download_largest_icon_) {
    replace_best_favicon_candidate =
        image.Size().GetArea() >
        best_favicon_candidate_.image.Size().GetArea();

    gfx::Size largest = best_favicon_candidate_.image.Size();
    if (replace_best_favicon_candidate)
      largest = image.Size();

    // The size of the downloaded icon may not match the declared size. Stop
    // downloading if:
    // - current candidate is only candidate.
    // - next candidate doesn't have sizes attributes, in this case, the rest
    //   candidates don't have sizes attribute either, stop downloading now,
    //   otherwise, all favicon without sizes attribute are downloaded.
    // - next candidate has sizes attribute and it is not larger than largest,
    // - current candidate is maximal one we want.
    const int maximal_size = GetMaximalIconSize(icon_type);
    if (current_candidate_index_ + 1 >= image_urls_.size()) {
      exact_match = true;
    } else {
      FaviconURL next_image_url = image_urls_[current_candidate_index_ + 1];
      exact_match = next_image_url.icon_sizes.empty() ||
          next_image_url.icon_sizes[0].GetArea() <= largest.GetArea() ||
          (image.Size().width() == maximal_size &&
           image.Size().height() == maximal_size);
    }
  } else {
    exact_match = score == 1 || preferred_icon_size() == 0;
    replace_best_favicon_candidate =
        exact_match ||
        best_favicon_candidate_.icon_type == favicon_base::INVALID_ICON ||
        score > best_favicon_candidate_.score;
  }
  if (replace_best_favicon_candidate) {
    best_favicon_candidate_ =
        FaviconCandidate(image_url, image, score, icon_type);
  }
  return exact_match;
}

void FaviconHandler::SetFavicon(const GURL& icon_url,
                                const gfx::Image& image,
                                favicon_base::IconType icon_type) {
  if (ShouldSaveFavicon())
    SetHistoryFavicons(url_, icon_url, icon_type, image);

  NotifyFaviconUpdated(icon_url, icon_type, image);
}

void FaviconHandler::NotifyFaviconUpdated(
    const std::vector<favicon_base::FaviconRawBitmapResult>&
        favicon_bitmap_results) {
  if (favicon_bitmap_results.empty())
    return;

  gfx::Image resized_image = favicon_base::SelectFaviconFramesFromPNGs(
      favicon_bitmap_results,
      favicon_base::GetFaviconScales(),
      preferred_icon_size());
  // The history service sends back results for a single icon URL and icon
  // type, so it does not matter which result we get |icon_url| and |icon_type|
  // from.
  const GURL icon_url = favicon_bitmap_results[0].icon_url;
  favicon_base::IconType icon_type = favicon_bitmap_results[0].icon_type;
  NotifyFaviconUpdated(icon_url, icon_type, resized_image);
}

void FaviconHandler::NotifyFaviconUpdated(const GURL& icon_url,
                                          favicon_base::IconType icon_type,
                                          const gfx::Image& image) {
  if (image.IsEmpty())
    return;

  gfx::Image image_with_adjusted_colorspace = image;
  favicon_base::SetFaviconColorSpace(&image_with_adjusted_colorspace);

  driver_->OnFaviconUpdated(url_, handler_type_, icon_url,
                            icon_url != notification_icon_url_,
                            image_with_adjusted_colorspace);

  notification_icon_url_ = icon_url;
  notification_icon_type_ = icon_type;
}

void FaviconHandler::OnUpdateFaviconURL(
    const GURL& page_url,
    const std::vector<FaviconURL>& candidates) {
  if (page_url != url_)
    return;

  std::vector<FaviconURL> pruned_candidates;
  for (const FaviconURL& candidate : candidates) {
    if (!candidate.icon_url.is_empty() && (candidate.icon_type & icon_types_))
      pruned_candidates.push_back(candidate);
  }

  if (download_largest_icon_)
    SortAndPruneImageUrls(&pruned_candidates);

  // Ignore FaviconURL::icon_sizes because FaviconURL::icon_sizes is not stored
  // in the history database.
  if (image_urls_.size() == pruned_candidates.size() &&
      std::equal(pruned_candidates.begin(), pruned_candidates.end(),
                 image_urls_.begin(), FaviconURLsEqualIgnoringSizes)) {
    return;
  }

  download_requests_.clear();
  image_urls_ = pruned_candidates;
  current_candidate_index_ = 0u;
  best_favicon_candidate_ = FaviconCandidate();

  // TODO(davemoore) Should clear on empty url. Currently we ignore it.
  // This appears to be what FF does as well.
  if (current_candidate() && got_favicon_from_history_)
    OnGotInitialHistoryDataAndIconURLCandidates();
}

void FaviconHandler::OnGotInitialHistoryDataAndIconURLCandidates() {
  if (!initial_history_result_expired_or_incomplete_ &&
      DoUrlAndIconMatch(*current_candidate(), notification_icon_url_,
                        notification_icon_type_)) {
    // - The data from history is valid and not expired.
    // - The icon URL of the history data matches one of the page's icon URLs.
    // - The icon URL of the history data matches the icon URL of the last
    //   OnFaviconAvailable() notification.
    // We are done. No additional downloads or history requests are needed.
    // TODO: Store all of the icon URLs associated with a page in history so
    // that we can check whether the page's icon URLs match the page's icon URLs
    // at the time that the favicon data was stored to the history database.
    return;
  }

  DownloadCurrentCandidateOrAskFaviconService();
}

void FaviconHandler::OnDidDownloadFavicon(
    int id,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  DownloadRequests::iterator i = download_requests_.find(id);
  if (i == download_requests_.end()) {
    // Currently WebContents notifies us of ANY downloads so that it is
    // possible to get here.
    return;
  }

  DownloadRequest download_request = i->second;
  download_requests_.erase(i);

  if (!current_candidate() ||
      !DoUrlAndIconMatch(*current_candidate(),
                         image_url,
                         download_request.icon_type)) {
    return;
  }

  bool request_next_icon = true;
  if (!bitmaps.empty()) {
    float score = 0.0f;
    gfx::ImageSkia image_skia;
    if (download_largest_icon_) {
      int index = -1;
      // Use the largest bitmap if FaviconURL doesn't have sizes attribute.
      if (current_candidate()->icon_sizes.empty()) {
        index = GetLargestSizeIndex(original_bitmap_sizes);
      } else {
        index = GetIndexBySize(original_bitmap_sizes,
                               current_candidate()->icon_sizes[0]);
        // Find largest bitmap if there is no one exactly matched.
        if (index == -1)
          index = GetLargestSizeIndex(original_bitmap_sizes);
      }
      image_skia = gfx::ImageSkia(gfx::ImageSkiaRep(bitmaps[index], 1));
    } else {
      image_skia = CreateFaviconImageSkia(bitmaps,
                                          original_bitmap_sizes,
                                          preferred_icon_size(),
                                          &score);
    }

    if (!image_skia.isNull()) {
      gfx::Image image(image_skia);
      // The downloaded icon is still valid when there is no FaviconURL update
      // during the downloading.
      request_next_icon = !UpdateFaviconCandidate(image_url, image, score,
                                                  download_request.icon_type);
    }
  }

  if (request_next_icon && current_candidate_index_ + 1 < image_urls_.size()) {
    // Process the next candidate.
    ++current_candidate_index_;
    DownloadCurrentCandidateOrAskFaviconService();
  } else {
    // We have either found the ideal candidate or run out of candidates.
    if (best_favicon_candidate_.icon_type != favicon_base::INVALID_ICON) {
      // No more icons to request, set the favicon from the candidate.
      SetFavicon(best_favicon_candidate_.image_url,
                 best_favicon_candidate_.image,
                 best_favicon_candidate_.icon_type);
    }
    // Clear download related state.
    download_requests_.clear();
    current_candidate_index_ = image_urls_.size();
    best_favicon_candidate_ = FaviconCandidate();
  }
}

bool FaviconHandler::HasPendingTasksForTest() {
  return !download_requests_.empty() ||
         cancelable_task_tracker_.HasTrackedTasks();
}

int FaviconHandler::DownloadFavicon(const GURL& image_url,
                                    int max_bitmap_size) {
  if (!image_url.is_valid()) {
    NOTREACHED();
    return 0;
  }
  return driver_->StartDownload(image_url, max_bitmap_size);
}

void FaviconHandler::UpdateFaviconMappingAndFetch(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  // TODO(pkotwicz): pass in all of |image_urls_| to
  // UpdateFaviconMappingsAndFetch().
  if (service_) {
    std::vector<GURL> icon_urls;
    icon_urls.push_back(icon_url);
    service_->UpdateFaviconMappingsAndFetch(page_url, icon_urls, icon_type,
                                            preferred_icon_size(), callback,
                                            tracker);
  }
}

void FaviconHandler::GetFaviconFromFaviconService(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  if (service_) {
    service_->GetFavicon(icon_url, icon_type, preferred_icon_size(), callback,
                         tracker);
  }
}

void FaviconHandler::GetFaviconForURLFromFaviconService(
    const GURL& page_url,
    int icon_types,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  if (service_) {
    service_->GetFaviconForPageURL(page_url, icon_types, preferred_icon_size(),
                                   callback, tracker);
  }
}

void FaviconHandler::SetHistoryFavicons(const GURL& page_url,
                                        const GURL& icon_url,
                                        favicon_base::IconType icon_type,
                                        const gfx::Image& image) {
  if (service_) {
    service_->SetFavicons(page_url, icon_url, icon_type, image);
  }
}

bool FaviconHandler::ShouldSaveFavicon() {
  if (!driver_->IsOffTheRecord())
    return true;

  // Always save favicon if the page is bookmarked.
  return driver_->IsBookmarked(url_);
}

int FaviconHandler::GetMaximalIconSize(favicon_base::IconType icon_type) {
  switch (icon_type) {
    case favicon_base::FAVICON:
#if defined(OS_ANDROID)
      return 192;
#else
      return gfx::ImageSkia::GetMaxSupportedScale() * gfx::kFaviconSize;
#endif
    case favicon_base::TOUCH_ICON:
    case favicon_base::TOUCH_PRECOMPOSED_ICON:
      return kTouchIconSize;
    case favicon_base::INVALID_ICON:
      return 0;
  }
  NOTREACHED();
  return 0;
}

void FaviconHandler::OnFaviconDataForInitialURLFromFaviconService(
    const std::vector<favicon_base::FaviconRawBitmapResult>&
        favicon_bitmap_results) {
  got_favicon_from_history_ = true;
  bool has_valid_result = HasValidResult(favicon_bitmap_results);
  initial_history_result_expired_or_incomplete_ =
      !has_valid_result ||
      HasExpiredOrIncompleteResult(preferred_icon_size(),
                                   favicon_bitmap_results);
  redownload_icons_ = initial_history_result_expired_or_incomplete_ &&
                      !favicon_bitmap_results.empty();

  if (has_valid_result &&
      (!current_candidate() ||
       DoUrlsAndIconsMatch(*current_candidate(), favicon_bitmap_results))) {
    // The db knows the favicon (although it may be out of date) and the entry
    // doesn't have an icon. Set the favicon now, and if the favicon turns out
    // to be expired (or the wrong url) we'll fetch later on. This way the
    // user doesn't see a flash of the default favicon.
    NotifyFaviconUpdated(favicon_bitmap_results);
  }

  if (current_candidate())
    OnGotInitialHistoryDataAndIconURLCandidates();
}

void FaviconHandler::DownloadCurrentCandidateOrAskFaviconService() {
  GURL icon_url = current_candidate()->icon_url;
  favicon_base::IconType icon_type = current_candidate()->icon_type;

  if (redownload_icons_) {
    // We have the mapping, but the favicon is out of date. Download it now.
    ScheduleDownload(icon_url, icon_type);
  } else {
    // We don't know the favicon, but we may have previously downloaded the
    // favicon for another page that shares the same favicon. Ask for the
    // favicon given the favicon URL.
    if (driver_->IsOffTheRecord()) {
      GetFaviconFromFaviconService(
          icon_url, icon_type,
          base::Bind(&FaviconHandler::OnFaviconData, base::Unretained(this)),
          &cancelable_task_tracker_);
    } else {
      // Ask the history service for the icon. This does two things:
      // 1. Attempts to fetch the favicon data from the database.
      // 2. If the favicon exists in the database, this updates the database to
      //    include the mapping between the page url and the favicon url.
      // This is asynchronous. The history service will call back when done.
      UpdateFaviconMappingAndFetch(
          url_, icon_url, icon_type,
          base::Bind(&FaviconHandler::OnFaviconData, base::Unretained(this)),
          &cancelable_task_tracker_);
    }
  }
}

void FaviconHandler::OnFaviconData(const std::vector<
    favicon_base::FaviconRawBitmapResult>& favicon_bitmap_results) {
  bool has_results = !favicon_bitmap_results.empty();
  bool has_valid_result = HasValidResult(favicon_bitmap_results);
  bool has_expired_or_incomplete_result =
      !has_valid_result || HasExpiredOrIncompleteResult(preferred_icon_size(),
                                                        favicon_bitmap_results);

  if (has_valid_result) {
    // There is a valid favicon. Notify any observers. It is useful to notify
    // the observers even if the favicon is expired or incomplete (incorrect
    // size) because temporarily showing the user an expired favicon or
    // streched favicon is preferable to showing the user the default favicon.
    NotifyFaviconUpdated(favicon_bitmap_results);
  }

  if (!current_candidate() ||
      (has_results &&
       !DoUrlsAndIconsMatch(*current_candidate(), favicon_bitmap_results))) {
    // The icon URLs have been updated since the favicon data was requested.
    return;
  }

  if (has_expired_or_incomplete_result) {
    ScheduleDownload(current_candidate()->icon_url,
                     current_candidate()->icon_type);
  }
}

void FaviconHandler::ScheduleDownload(const GURL& image_url,
                                      favicon_base::IconType icon_type) {
  // A max bitmap size is specified to avoid receiving huge bitmaps in
  // OnDidDownloadFavicon(). See FaviconDriver::StartDownload()
  // for more details about the max bitmap size.
  const int download_id = DownloadFavicon(image_url,
                                          GetMaximalIconSize(icon_type));

  // Download ids should be unique.
  DCHECK(download_requests_.find(download_id) == download_requests_.end());
  download_requests_[download_id] = DownloadRequest(image_url, icon_type);

  if (download_id == 0) {
    // If DownloadFavicon() did not start a download, it returns a download id
    // of 0. We still need to call OnDidDownloadFavicon() because the method is
    // responsible for initiating the data request for the next candidate.
    OnDidDownloadFavicon(download_id, image_url, std::vector<SkBitmap>(),
                         std::vector<gfx::Size>());

  }
}

}  // namespace favicon
