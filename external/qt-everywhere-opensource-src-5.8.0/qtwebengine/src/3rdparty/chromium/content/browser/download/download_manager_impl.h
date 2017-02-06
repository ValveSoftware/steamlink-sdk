// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/lock.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/url_downloader.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/download_url_parameters.h"

namespace net {
class BoundNetLog;
}

namespace content {
class DownloadFileFactory;
class DownloadItemFactory;
class DownloadItemImpl;
class DownloadRequestHandleInterface;

class CONTENT_EXPORT DownloadManagerImpl : public DownloadManager,
                                           private DownloadItemImplDelegate {
 public:
  using DownloadItemImplCreated = base::Callback<void(DownloadItemImpl*)>;

  // Caller guarantees that |net_log| will remain valid
  // for the lifetime of DownloadManagerImpl (until Shutdown() is called).
  DownloadManagerImpl(net::NetLog* net_log, BrowserContext* browser_context);
  ~DownloadManagerImpl() override;

  // Implementation functions (not part of the DownloadManager interface).

  // Creates a download item for the SavePackage system.
  // Must be called on the UI thread.  Note that the DownloadManager
  // retains ownership.
  virtual void CreateSavePackageDownloadItem(
      const base::FilePath& main_file_path,
      const GURL& page_url,
      const std::string& mime_type,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle,
      const DownloadItemImplCreated& item_created);

  // Notifies DownloadManager about a successful completion of |download_item|.
  void OnSavePackageSuccessfullyFinished(DownloadItem* download_item);

  // DownloadManager functions.
  void SetDelegate(DownloadManagerDelegate* delegate) override;
  DownloadManagerDelegate* GetDelegate() const override;
  void Shutdown() override;
  void GetAllDownloads(DownloadVector* result) override;
  void StartDownload(
      std::unique_ptr<DownloadCreateInfo> info,
      std::unique_ptr<ByteStreamReader> stream,
      const DownloadUrlParameters::OnStartedCallback& on_started) override;

  int RemoveDownloadsByURLAndTime(
      const base::Callback<bool(const GURL&)>& url_filter,
      base::Time remove_begin,
      base::Time remove_end) override;
  int RemoveAllDownloads() override;
  void DownloadUrl(std::unique_ptr<DownloadUrlParameters> params) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  content::DownloadItem* CreateDownloadItem(
      const std::string& guid,
      uint32_t id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_refererr_url,
      const std::string& mime_type,
      const std::string& original_mime_type,
      const base::Time& start_time,
      const base::Time& end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64_t received_bytes,
      int64_t total_bytes,
      const std::string& hash,
      content::DownloadItem::DownloadState state,
      DownloadDangerType danger_type,
      DownloadInterruptReason interrupt_reason,
      bool opened) override;
  int InProgressCount() const override;
  int NonMaliciousInProgressCount() const override;
  BrowserContext* GetBrowserContext() const override;
  void CheckForHistoryFilesRemoval() override;
  DownloadItem* GetDownload(uint32_t id) override;
  DownloadItem* GetDownloadByGuid(const std::string& guid) override;

  // For testing; specifically, accessed from TestFileErrorInjector.
  void SetDownloadItemFactoryForTesting(
      std::unique_ptr<DownloadItemFactory> item_factory);
  void SetDownloadFileFactoryForTesting(
      std::unique_ptr<DownloadFileFactory> file_factory);
  virtual DownloadFileFactory* GetDownloadFileFactoryForTesting();

  void RemoveUrlDownloader(UrlDownloader* downloader);

 private:
  using DownloadSet = std::set<DownloadItem*>;
  using DownloadMap = std::unordered_map<uint32_t, DownloadItemImpl*>;
  using DownloadGuidMap = std::unordered_map<std::string, DownloadItemImpl*>;
  using DownloadItemImplVector = std::vector<DownloadItemImpl*>;
  using DownloadRemover = base::Callback<bool(const DownloadItemImpl*)>;

  // For testing.
  friend class DownloadManagerTest;
  friend class DownloadTest;

  void StartDownloadWithId(
      std::unique_ptr<DownloadCreateInfo> info,
      std::unique_ptr<ByteStreamReader> stream,
      const DownloadUrlParameters::OnStartedCallback& on_started,
      bool new_download,
      uint32_t id);

  void CreateSavePackageDownloadItemWithId(
      const base::FilePath& main_file_path,
      const GURL& page_url,
      const std::string& mime_type,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle,
      const DownloadItemImplCreated& on_started,
      uint32_t id);

  // Create a new active item based on the info.  Separate from
  // StartDownload() for testing.
  DownloadItemImpl* CreateActiveItem(uint32_t id,
                                     const DownloadCreateInfo& info);

  // Get next download id. |callback| is called on the UI thread and may
  // be called synchronously.
  void GetNextId(const DownloadIdCallback& callback);

  // Called with the result of DownloadManagerDelegate::CheckForFileExistence.
  // Updates the state of the file and then notifies this update to the file's
  // observer.
  void OnFileExistenceChecked(uint32_t download_id, bool result);

  // Remove all downloads for which |remover| returns true.
  int RemoveDownloads(const DownloadRemover& remover);

  // Overridden from DownloadItemImplDelegate
  // (Note that |GetBrowserContext| are present in both interfaces.)
  void DetermineDownloadTarget(DownloadItemImpl* item,
                               const DownloadTargetCallback& callback) override;
  bool ShouldCompleteDownload(DownloadItemImpl* item,
                              const base::Closure& complete_callback) override;
  bool ShouldOpenFileBasedOnExtension(const base::FilePath& path) override;
  bool ShouldOpenDownload(DownloadItemImpl* item,
                          const ShouldOpenDownloadCallback& callback) override;
  void CheckForFileRemoval(DownloadItemImpl* download_item) override;
  void ResumeInterruptedDownload(
      std::unique_ptr<content::DownloadUrlParameters> params,
      uint32_t id) override;
  void OpenDownload(DownloadItemImpl* download) override;
  void ShowDownloadInShell(DownloadItemImpl* download) override;
  void DownloadRemoved(DownloadItemImpl* download) override;

  void AddUrlDownloader(
      std::unique_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread>
          downloader);

  // Factory for creation of downloads items.
  std::unique_ptr<DownloadItemFactory> item_factory_;

  // Factory for the creation of download files.
  std::unique_ptr<DownloadFileFactory> file_factory_;

  // |downloads_| is the owning set for all downloads known to the
  // DownloadManager.  This includes downloads started by the user in
  // this session, downloads initialized from the history system, and
  // "save page as" downloads.
  // TODO(asanka): Remove this container in favor of downloads_by_guid_ as a
  // part of http://crbug.com/593020.
  DownloadMap downloads_;

  // Same as the above, but maps from GUID to download item. Note that the
  // container is case sensitive. Hence the key needs to be normalized to
  // upper-case when inserting new elements here. Fortunately for us,
  // DownloadItemImpl already normalizes the string GUID.
  DownloadGuidMap downloads_by_guid_;

  // True if the download manager has been initialized and requires a shutdown.
  bool shutdown_needed_;

  // Observers that want to be notified of changes to the set of downloads.
  base::ObserverList<Observer> observers_;

  // The current active browser context.
  BrowserContext* browser_context_;

  // Allows an embedder to control behavior. Guaranteed to outlive this object.
  DownloadManagerDelegate* delegate_;

  net::NetLog* net_log_;

  std::vector<std::unique_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread>>
      url_downloaders_;

  base::WeakPtrFactory<DownloadManagerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_MANAGER_IMPL_H_
