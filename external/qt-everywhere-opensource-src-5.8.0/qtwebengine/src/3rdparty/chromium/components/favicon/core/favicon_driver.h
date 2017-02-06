// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_
#define COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "components/favicon/core/favicon_driver_observer.h"

class GURL;

namespace gfx {
class Image;
}

namespace favicon {

// Interface that allows favicon core code to obtain information about the
// current page. This is partially implemented by FaviconDriverImpl, and
// concrete implementation should be based on that class instead of directly
// subclassing FaviconDriver.
class FaviconDriver {
 public:
  // Adds/Removes an observer.
  void AddObserver(FaviconDriverObserver* observer);
  void RemoveObserver(FaviconDriverObserver* observer);

  // Initiates loading the favicon for the specified url.
  virtual void FetchFavicon(const GURL& url) = 0;

  // Returns the favicon for this tab, or IDR_DEFAULT_FAVICON if the tab does
  // not have a favicon. The default implementation uses the current navigation
  // entry. Returns an empty bitmap if there are no navigation entries, which
  // should rarely happen.
  virtual gfx::Image GetFavicon() const = 0;

  // Returns true if we have the favicon for the page.
  virtual bool FaviconIsValid() const = 0;

  // Starts the download for the given favicon. When finished, the driver
  // will call OnDidDownloadFavicon() with the results.
  // Returns the unique id of the download request. The id will be passed
  // in OnDidDownloadFavicon().
  // Bitmaps with pixel sizes larger than |max_bitmap_size| are filtered out
  // from the bitmap results. If there are no bitmap results <=
  // |max_bitmap_size|, the smallest bitmap is resized to |max_bitmap_size| and
  // is the only result. A |max_bitmap_size| of 0 means unlimited.
  virtual int StartDownload(const GURL& url, int max_bitmap_size) = 0;

  // Returns whether the user is operating in an off-the-record context.
  virtual bool IsOffTheRecord() = 0;

  // Returns whether |url| is bookmarked.
  virtual bool IsBookmarked(const GURL& url) = 0;

  // Returns the URL of the current page, if any. Returns an invalid URL
  // otherwise.
  virtual GURL GetActiveURL() = 0;

  // Notifies the driver that the favicon image has been updated.
  // See comment for FaviconDriverObserver::OnFaviconUpdated() for more details.
  virtual void OnFaviconUpdated(
      const GURL& page_url,
      FaviconDriverObserver::NotificationIconType notification_icon_type,
      const GURL& icon_url,
      bool icon_url_changed,
      const gfx::Image& image) = 0;

  // Returns whether the driver is waiting for a download to complete or for
  // data from the FaviconService. Reserved for testing.
  virtual bool HasPendingTasksForTest() = 0;

 protected:
  FaviconDriver();
  virtual ~FaviconDriver();

  // Notifies FaviconDriverObservers that the favicon image has been updated.
  void NotifyFaviconUpdatedObservers(
    FaviconDriverObserver::NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image);


 private:
  base::ObserverList<FaviconDriverObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(FaviconDriver);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_FAVICON_DRIVER_H_
