// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CONTENT_CONTENT_FAVICON_DRIVER_H_
#define COMPONENTS_FAVICON_CONTENT_CONTENT_FAVICON_DRIVER_H_

#include "base/macros.h"
#include "components/favicon/core/favicon_driver_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace content {
struct FaviconStatus;
struct FaviconURL;
class WebContents;
}

namespace favicon {

// ContentFaviconDriver is an implementation of FaviconDriver that listens to
// WebContents events to start download of favicons and to get informed when the
// favicon download has completed.
class ContentFaviconDriver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ContentFaviconDriver>,
      public FaviconDriverImpl {
 public:
  static void CreateForWebContents(content::WebContents* web_contents,
                                   FaviconService* favicon_service,
                                   history::HistoryService* history_service,
                                   bookmarks::BookmarkModel* bookmark_model);

  // Returns the current tab's favicon URLs. If this is empty,
  // DidUpdateFaviconURL has not yet been called for the current navigation.
  const std::vector<content::FaviconURL>& favicon_urls() const {
    return favicon_urls_;
  }

  // Saves the favicon for the last committed navigation entry to the thumbnail
  // database.
  void SaveFavicon();

  // FaviconDriver implementation.
  gfx::Image GetFavicon() const override;
  bool FaviconIsValid() const override;
  int StartDownload(const GURL& url, int max_bitmap_size) override;
  bool IsOffTheRecord() override;
  GURL GetActiveURL() override;

 protected:
  ContentFaviconDriver(content::WebContents* web_contents,
                       FaviconService* favicon_service,
                       history::HistoryService* history_service,
                       bookmarks::BookmarkModel* bookmark_model);
  ~ContentFaviconDriver() override;

 private:
  friend class content::WebContentsUserData<ContentFaviconDriver>;

  // FaviconDriver implementation.
  void OnFaviconUpdated(
      const GURL& page_url,
      FaviconDriverObserver::NotificationIconType icon_type,
      const GURL& icon_url,
      bool icon_url_changed,
      const gfx::Image& image) override;

  // content::WebContentsObserver implementation.
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) override;
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

  GURL bypass_cache_page_url_;
  std::vector<content::FaviconURL> favicon_urls_;

  DISALLOW_COPY_AND_ASSIGN(ContentFaviconDriver);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CONTENT_CONTENT_FAVICON_DRIVER_H_
