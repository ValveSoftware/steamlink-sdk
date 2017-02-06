// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_IOS_WEB_FAVICON_DRIVER_H_
#define COMPONENTS_FAVICON_IOS_WEB_FAVICON_DRIVER_H_

#include "base/macros.h"
#include "components/favicon/core/favicon_driver_impl.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "ios/web/public/web_state/web_state_user_data.h"

namespace web {
struct FaviconStatus;
class WebState;
}

namespace favicon {

// WebFaviconDriver is an implementation of FaviconDriver that listen to
// WebState events to start download of favicons and to get informed when the
// favicon download has completed.
class WebFaviconDriver : public web::WebStateObserver,
                         public web::WebStateUserData<WebFaviconDriver>,
                         public FaviconDriverImpl {
 public:
  static void CreateForWebState(web::WebState* web_state,
                                FaviconService* favicon_service,
                                history::HistoryService* history_service,
                                bookmarks::BookmarkModel* bookmark_model);

  // FaviconDriver implementation.
  void FetchFavicon(const GURL& url) override;
  gfx::Image GetFavicon() const override;
  bool FaviconIsValid() const override;
  int StartDownload(const GURL& url, int max_bitmap_size) override;
  bool IsOffTheRecord() override;
  GURL GetActiveURL() override;
  void OnFaviconUpdated(
      const GURL& page_url,
      FaviconDriverObserver::NotificationIconType notification_icon_type,
      const GURL& icon_url,
      bool icon_url_changed,
      const gfx::Image& image) override;

 private:
  friend class web::WebStateUserData<WebFaviconDriver>;

  WebFaviconDriver(web::WebState* web_state,
                   FaviconService* favicon_service,
                   history::HistoryService* history_service,
                   bookmarks::BookmarkModel* bookmark_model);
  ~WebFaviconDriver() override;

  // web::WebStateObserver implementation.
  void FaviconUrlUpdated(
      const std::vector<web::FaviconURL>& candidates) override;

  // The URL passed to FetchFavicon().
  GURL fetch_favicon_url_;

  DISALLOW_COPY_AND_ASSIGN(WebFaviconDriver);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_IOS_WEB_FAVICON_DRIVER_H_
