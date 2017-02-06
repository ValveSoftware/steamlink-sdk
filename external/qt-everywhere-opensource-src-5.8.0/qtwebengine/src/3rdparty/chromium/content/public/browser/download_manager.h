// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadManager object manages the process of downloading, including
// updates to the history system and providing the information for displaying
// the downloads view in the Destinations tab. There is one DownloadManager per
// active browser context in Chrome.
//
// Download observers:
// Objects that are interested in notifications about new downloads, or progress
// updates for a given download must implement one of the download observer
// interfaces:
//   DownloadManager::Observer:
//     - allows observers, primarily views, to be notified when changes to the
//       set of all downloads (such as new downloads, or deletes) occur
// Use AddObserver() / RemoveObserver() on the appropriate download object to
// receive state updates.
//
// Download state persistence:
// The DownloadManager uses the history service for storing persistent
// information about the state of all downloads. The history system maintains a
// separate table for this called 'downloads'. At the point that the
// DownloadManager is constructed, we query the history service for the state of
// all persisted downloads.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/time/time.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_url_parameters.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"

class GURL;

namespace url {
class Origin;
}

namespace content {

class BrowserContext;
class ByteStreamReader;
class DownloadManagerDelegate;
class DownloadQuery;
class DownloadRequestHandle;
struct DownloadCreateInfo;

// Browser's download manager: manages all downloads and destination view.
class CONTENT_EXPORT DownloadManager : public base::SupportsUserData::Data {
 public:
  ~DownloadManager() override {}

  // Sets/Gets the delegate for this DownloadManager. The delegate has to live
  // past its Shutdown method being called (by the DownloadManager).
  virtual void SetDelegate(DownloadManagerDelegate* delegate) = 0;
  virtual DownloadManagerDelegate* GetDelegate() const = 0;

  // Shutdown the download manager. Content calls this when BrowserContext is
  // being destructed. If the embedder needs this to be called earlier, it can
  // call it. In that case, the delegate's Shutdown() method will only be called
  // once.
  virtual void Shutdown() = 0;

  // Interface to implement for observers that wish to be informed of changes
  // to the DownloadManager's collection of downloads.
  class CONTENT_EXPORT Observer {
   public:
    // A DownloadItem was created. This item may be visible before the filename
    // is determined; in this case the return value of GetTargetFileName() will
    // be null.  This method may be called an arbitrary number of times, e.g.
    // when loading history on startup.  As a result, consumers should avoid
    // doing large amounts of work in OnDownloadCreated().  TODO(<whoever>):
    // When we've fully specified the possible states of the DownloadItem in
    // download_item.h, we should remove the caveat above.
    virtual void OnDownloadCreated(
        DownloadManager* manager, DownloadItem* item) {}

    // A SavePackage has successfully finished.
    virtual void OnSavePackageSuccessfullyFinished(
        DownloadManager* manager, DownloadItem* item) {}

    // Called when the DownloadManager is being destroyed to prevent Observers
    // from calling back to a stale pointer.
    virtual void ManagerGoingDown(DownloadManager* manager) {}

   protected:
    virtual ~Observer() {}
  };

  typedef std::vector<DownloadItem*> DownloadVector;

  // Add all download items to |downloads|, no matter the type or state, without
  // clearing |downloads| first.
  virtual void GetAllDownloads(DownloadVector* downloads) = 0;

  // Called by a download source (Currently DownloadResourceHandler)
  // to initiate the non-source portions of a download.
  // Returns the id assigned to the download.  If the DownloadCreateInfo
  // specifies an id, that id will be used.
  virtual void StartDownload(
      std::unique_ptr<DownloadCreateInfo> info,
      std::unique_ptr<ByteStreamReader> stream,
      const DownloadUrlParameters::OnStartedCallback& on_started) = 0;

  // Remove downloads whose URLs match the |url_filter| and are within
  // the given time constraints - after remove_begin (inclusive) and before
  // remove_end (exclusive). You may pass in null Time values to do an unbounded
  // delete in either direction.
  virtual int RemoveDownloadsByURLAndTime(
      const base::Callback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end) = 0;

  // Remove all downloads will delete all downloads. The number of downloads
  // deleted is returned back to the caller.
  virtual int RemoveAllDownloads() = 0;

  // See DownloadUrlParameters for details about controlling the download.
  virtual void DownloadUrl(
      std::unique_ptr<DownloadUrlParameters> parameters) = 0;

  // Allow objects to observe the download creation process.
  virtual void AddObserver(Observer* observer) = 0;

  // Remove a download observer from ourself.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Called by the embedder, after creating the download manager, to let it know
  // about downloads from previous runs of the browser.
  virtual DownloadItem* CreateDownloadItem(
      const std::string& guid,
      uint32_t id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_referrer_url,
      const std::string& mime_type,
      const std::string& original_mime_type,
      const base::Time& start_time,
      const base::Time& end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64_t received_bytes,
      int64_t total_bytes,
      const std::string& hash,
      DownloadItem::DownloadState state,
      DownloadDangerType danger_type,
      DownloadInterruptReason interrupt_reason,
      bool opened) = 0;

  // The number of in progress (including paused) downloads.
  // Performance note: this loops over all items. If profiling finds that this
  // is too slow, use an AllDownloadItemNotifier to count in-progress items.
  virtual int InProgressCount() const = 0;

  // The number of in progress (including paused) downloads.
  // Performance note: this loops over all items. If profiling finds that this
  // is too slow, use an AllDownloadItemNotifier to count in-progress items.
  // This excludes downloads that are marked as malicious.
  virtual int NonMaliciousInProgressCount() const = 0;

  virtual BrowserContext* GetBrowserContext() const = 0;

  // Checks whether downloaded files still exist. Updates state of downloads
  // that refer to removed files. The check runs in the background and may
  // finish asynchronously after this method returns.
  virtual void CheckForHistoryFilesRemoval() = 0;

  // Get the download item for |id| if present, no matter what type of download
  // it is or state it's in.
  // DEPRECATED: Don't add new callers for GetDownload(uint32_t). Instead keep
  // track of the GUID and use GetDownloadByGuid(), or observe the DownloadItem
  // if you need to keep track of a specific download. (http://crbug.com/593020)
  virtual DownloadItem* GetDownload(uint32_t id) = 0;

  // Get the download item for |guid|.
  virtual DownloadItem* GetDownloadByGuid(const std::string& guid) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_MANAGER_H_
