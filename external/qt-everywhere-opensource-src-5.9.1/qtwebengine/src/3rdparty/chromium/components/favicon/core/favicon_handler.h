// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_HANDLER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_HANDLER_H_

#include <stddef.h>

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/favicon/core/favicon_url.h"
#include "components/favicon_base/favicon_callback.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class SkBitmap;

namespace base {
class RefCountedMemory;
}

namespace favicon {

class FaviconDriver;
class FaviconService;
class TestFaviconHandler;

// FaviconHandler works with FaviconDriver to fetch the specific type of
// favicon.
//
// FetchFavicon requests the favicon from the favicon service which in turn
// requests the favicon from the history database. At this point
// we only know the URL of the page, and not necessarily the url of the
// favicon. To ensure we handle reloading stale favicons as well as
// reloading a favicon on page reload we always request the favicon from
// history regardless of whether the active favicon is valid.
//
// After the navigation two types of events are delivered (which is
// first depends upon who is faster): notification from the history
// db on our request for the favicon
// (OnFaviconDataForInitialURLFromFaviconService), or a message from the
// renderer giving us the URL of the favicon for the page (SetFaviconURL).
// . If the history db has a valid up to date favicon for the page, we update
//   the current page and use the favicon.
// . When we receive the favicon url if it matches that of the current page
//   and the current page's favicon is set, we do nothing (everything is
//   ok).
// . On the other hand if the database does not know the favicon for url, or
//   the favicon is out date, or the URL from the renderer does not match that
//   of the current page we proceed to DownloadFaviconOrAskHistory. Before we
//   invoke DownloadFaviconOrAskHistory we wait until we've received both
//   the favicon url and the callback from history. We wait to ensure we
//   truly know both the favicon url and the state of the database.
//
// DownloadFaviconOrAskHistory does the following:
// . If we have a valid favicon, but it is expired we ask the renderer to
//   download the favicon.
// . Otherwise we ask the history database to update the mapping from
//   page url to favicon url and call us back with the favicon. Remember, it is
//   possible for the db to already have the favicon, just not the mapping
//   between page to favicon url. The callback for this is OnFaviconData.
//
// OnFaviconData either updates the favicon of the current page (if the
// db knew about the favicon), or requests the renderer to download the
// favicon.
//
// When the renderer downloads favicons, it considers the entire list of
// favicon candidates, if |download_largest_favicon_| is true, the largest
// favicon will be used, otherwise the one that best matches the preferred size
// is chosen (or the first one if there is no preferred  size). Once the
// matching favicon has been determined, SetFavicon is called which updates
// the page's favicon and notifies the database to save the favicon.

class FaviconHandler {
 public:
  FaviconHandler(FaviconService* service,
                 FaviconDriver* driver,
                 FaviconDriverObserver::NotificationIconType handler_type);
  virtual ~FaviconHandler();

  // Returns the bit mask of favicon_base::IconType based on the handler's type.
  static int GetIconTypesFromHandlerType(
      FaviconDriverObserver::NotificationIconType handler_type);

  // Initiates loading the favicon for the specified url.
  void FetchFavicon(const GURL& url);

  // Message Handler.  Must be public, because also called from
  // PrerenderContents. Collects the |image_urls| list.
  void OnUpdateFaviconURL(const GURL& page_url,
                          const std::vector<favicon::FaviconURL>& candidates);

  // Called when the history request for favicon data mapped to |url_| has
  // completed and the renderer has told us the icon URLs used by |url_|
  void OnGotInitialHistoryDataAndIconURLCandidates();

  // Message handler for ImageHostMsg_DidDownloadImage. Called when the image
  // at |image_url| has been downloaded.
  // |bitmaps| is a list of all the frames of the image at |image_url|.
  // |original_bitmap_sizes| are the sizes of |bitmaps| before they were resized
  // to the maximum bitmap size passed to DownloadFavicon().
  void OnDidDownloadFavicon(
      int id,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

  // For testing.
  const std::vector<favicon::FaviconURL>& image_urls() const {
    return image_urls_;
  }

  // Returns whether the handler is waiting for a download to complete or for
  // data from the FaviconService. Reserved for testing.
  bool HasPendingTasksForTest();

 protected:
  // These virtual methods make FaviconHandler testable and are overridden by
  // TestFaviconHandler.

  // Asks the render to download favicon, returns the request id.
  virtual int DownloadFavicon(const GURL& image_url, int max_bitmap_size);

  // Ask the favicon from history
  virtual void UpdateFaviconMappingAndFetch(
      const GURL& page_url,
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const favicon_base::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker);

  virtual void GetFaviconFromFaviconService(
      const GURL& icon_url,
      favicon_base::IconType icon_type,
      const favicon_base::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker);

  virtual void GetFaviconForURLFromFaviconService(
      const GURL& page_url,
      int icon_types,
      const favicon_base::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker);

  virtual void SetHistoryFavicons(const GURL& page_url,
                                  const GURL& icon_url,
                                  favicon_base::IconType icon_type,
                                  const gfx::Image& image);

  // Returns true if the favicon should be saved.
  virtual bool ShouldSaveFavicon();

 private:
  // For testing:
  friend class TestFaviconHandler;

  // Represents an in progress download of an image from the renderer.
  struct DownloadRequest {
    DownloadRequest();
    ~DownloadRequest();

    DownloadRequest(const GURL& image_url, favicon_base::IconType icon_type);

    GURL image_url;
    favicon_base::IconType icon_type;
  };

  // Used to track a candidate for the favicon.
  struct FaviconCandidate {
    FaviconCandidate();
    ~FaviconCandidate();

    FaviconCandidate(const GURL& image_url,
                     const gfx::Image& image,
                     float score,
                     favicon_base::IconType icon_type);

    GURL image_url;
    gfx::Image image;
    float score;
    favicon_base::IconType icon_type;
  };

  // Get the maximal icon size in pixels for a icon of type |icon_type| for the
  // current platform.
  static int GetMaximalIconSize(favicon_base::IconType icon_type);

  // See description above class for details.
  void OnFaviconDataForInitialURLFromFaviconService(const std::vector<
      favicon_base::FaviconRawBitmapResult>& favicon_bitmap_results);

  // If the favicon currently mapped to |url_| has expired, downloads the
  // current candidate favicon from the renderer. Otherwise requests data for
  // the current favicon from history. If data is requested from history,
  // OnFaviconData() is called with the history data once it has been retrieved.
  void DownloadCurrentCandidateOrAskFaviconService();

  // See description above class for details.
  void OnFaviconData(const std::vector<favicon_base::FaviconRawBitmapResult>&
                         favicon_bitmap_results);

  // Schedules a download for the specified entry. This adds the request to
  // download_requests_.
  void ScheduleDownload(const GURL& image_url,
                        favicon_base::IconType icon_type);

  // Updates |favicon_candidate_| and returns true if it is an exact match.
  bool UpdateFaviconCandidate(const GURL& image_url,
                              const gfx::Image& image,
                              float score,
                              favicon_base::IconType icon_type);

  // Sets the image data for the favicon.
  void SetFavicon(const GURL& icon_url,
                  const gfx::Image& image,
                  favicon_base::IconType icon_type);

  // Notifies |driver_| that FaviconHandler found an icon which matches the
  // |handler_type_| criteria. NotifyFaviconUpdated() can be called multiple
  // times for the same page if:
  // - a better match is found for |handler_type_| (e.g. newer bitmap data)
  // - Javascript changes the page's icon URLs.
  void NotifyFaviconUpdated(
      const std::vector<favicon_base::FaviconRawBitmapResult>&
          favicon_bitmap_results);
  void NotifyFaviconUpdated(const GURL& icon_url,
                            favicon_base::IconType icon_type,
                            const gfx::Image& image);

  // Return the current candidate if any.
  favicon::FaviconURL* current_candidate() {
    return current_candidate_index_ < image_urls_.size()
               ? &image_urls_[current_candidate_index_]
               : nullptr;
  }

  // Returns the preferred size of the image. 0 means no preference (any size
  // will do).
  int preferred_icon_size() const {
    return download_largest_icon_ ? 0 : gfx::kFaviconSize;
  }

  // Used for FaviconService requests.
  base::CancelableTaskTracker cancelable_task_tracker_;

  FaviconDriverObserver::NotificationIconType handler_type_;

  // URL of the page we're requesting the favicon for.
  GURL url_;

  // Whether we got data back for the initial request to the FaviconService.
  bool got_favicon_from_history_;

  // Whether the history data returned in
  // OnFaviconDataForInitialURLFromFaviconService() is out of date or is known
  // to be incomplete. If true, it means there is a favicon mapped to |url_| in
  // history, but we need to redownload the icon URLs because the icon in the
  // database has expired or the data in the database is incomplete.
  bool initial_history_result_expired_or_incomplete_;

  // Whether FaviconHandler should ignore history state and determine the
  // optimal icon URL out of |image_urls_| for |url_| by downloading
  // |image_urls_| one by one.
  bool redownload_icons_;

  // Requests to the renderer to download favicons.
  typedef std::map<int, DownloadRequest> DownloadRequests;
  DownloadRequests download_requests_;

  // The combination of the supported icon types.
  const int icon_types_;

  // Whether the largest icon should be downloaded.
  const bool download_largest_icon_;

  // The prioritized favicon candidates from the page back from the renderer.
  std::vector<favicon::FaviconURL> image_urls_;

  // The icon URL and the icon type of the favicon in the most recent
  // FaviconDriver::OnFaviconAvailable() notification.
  GURL notification_icon_url_;
  favicon_base::IconType notification_icon_type_;

  // The FaviconService which implements favicon operations. May be null during
  // testing.
  FaviconService* service_;

  // This handler's driver, owns this object.
  FaviconDriver* driver_;

  // The index of the favicon URL in |image_urls_| which is currently being
  // requested from history or downloaded.
  size_t current_candidate_index_;

  // Best image we've seen so far.  As images are downloaded from the page they
  // are stored here. When there is an exact match, or no more images are
  // available the favicon service and the current page are updated (assuming
  // the image is for a favicon).
  FaviconCandidate best_favicon_candidate_;

  DISALLOW_COPY_AND_ASSIGN(FaviconHandler);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_HANDLER_H_
