// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_manager_impl.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/alias.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_factory.h"
#include "content/browser/download/download_item_factory.h"
#include "content/browser/download/download_item_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_request_context.h"
#include "storage/browser/blob/blob_url_request_job_factory.h"
#include "url/origin.h"

namespace content {
namespace {

std::unique_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread> BeginDownload(
    std::unique_ptr<DownloadUrlParameters> params,
    content::ResourceContext* resource_context,
    uint32_t download_id,
    base::WeakPtr<DownloadManagerImpl> download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::unique_ptr<net::URLRequest> url_request =
      DownloadRequestCore::CreateRequestOnIOThread(download_id, params.get());
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
      params->GetBlobDataHandle();
  if (blob_data_handle) {
    storage::BlobProtocolHandler::SetRequestedBlobDataHandle(
        url_request.get(), std::move(blob_data_handle));
  }

  // If there's a valid renderer process associated with the request, then the
  // request should be driven by the ResourceLoader. Pass it over to the
  // ResourceDispatcherHostImpl which will in turn pass it along to the
  // ResourceLoader.
  if (params->render_process_host_id() >= 0) {
    DownloadInterruptReason reason =
        ResourceDispatcherHostImpl::Get()->BeginDownload(
            std::move(url_request), params->referrer(),
            params->content_initiated(), resource_context,
            params->render_process_host_id(),
            params->render_view_host_routing_id(),
            params->render_frame_host_routing_id(),
            params->do_not_prompt_for_login());

    // If the download was accepted, the DownloadResourceHandler is now
    // responsible for driving the request to completion.
    if (reason == DOWNLOAD_INTERRUPT_REASON_NONE)
      return nullptr;

    // Otherwise, create an interrupted download.
    std::unique_ptr<DownloadCreateInfo> failed_created_info(
        new DownloadCreateInfo(base::Time::Now(), net::BoundNetLog(),
                               base::WrapUnique(new DownloadSaveInfo)));
    failed_created_info->url_chain.push_back(params->url());
    failed_created_info->result = reason;
    std::unique_ptr<ByteStreamReader> empty_byte_stream;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DownloadManager::StartDownload, download_manager,
                   base::Passed(&failed_created_info),
                   base::Passed(&empty_byte_stream), params->callback()));
    return nullptr;
  }

  return std::unique_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread>(
      UrlDownloader::BeginDownload(download_manager, std::move(url_request),
                                   params->referrer())
          .release());
}

class DownloadItemFactoryImpl : public DownloadItemFactory {
 public:
  DownloadItemFactoryImpl() {}
  ~DownloadItemFactoryImpl() override {}

  DownloadItemImpl* CreatePersistedItem(
      DownloadItemImplDelegate* delegate,
      const std::string& guid,
      uint32_t download_id,
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
      DownloadItem::DownloadState state,
      DownloadDangerType danger_type,
      DownloadInterruptReason interrupt_reason,
      bool opened,
      const net::BoundNetLog& bound_net_log) override {
    return new DownloadItemImpl(
        delegate, guid, download_id, current_path, target_path, url_chain,
        referrer_url, site_url, tab_url, tab_refererr_url, mime_type,
        original_mime_type, start_time, end_time, etag, last_modified,
        received_bytes, total_bytes, hash, state, danger_type, interrupt_reason,
        opened, bound_net_log);
  }

  DownloadItemImpl* CreateActiveItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const DownloadCreateInfo& info,
      const net::BoundNetLog& bound_net_log) override {
    return new DownloadItemImpl(delegate, download_id, info, bound_net_log);
  }

  DownloadItemImpl* CreateSavePageItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const base::FilePath& path,
      const GURL& url,
      const std::string& mime_type,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle,
      const net::BoundNetLog& bound_net_log) override {
    return new DownloadItemImpl(delegate, download_id, path, url, mime_type,
                                std::move(request_handle), bound_net_log);
  }
};

}  // namespace

DownloadManagerImpl::DownloadManagerImpl(
    net::NetLog* net_log,
    BrowserContext* browser_context)
    : item_factory_(new DownloadItemFactoryImpl()),
      file_factory_(new DownloadFileFactory()),
      shutdown_needed_(true),
      browser_context_(browser_context),
      delegate_(NULL),
      net_log_(net_log),
      weak_factory_(this) {
  DCHECK(browser_context);
}

DownloadManagerImpl::~DownloadManagerImpl() {
  DCHECK(!shutdown_needed_);
}

DownloadItemImpl* DownloadManagerImpl::CreateActiveItem(
    uint32_t id,
    const DownloadCreateInfo& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!ContainsKey(downloads_, id));
  net::BoundNetLog bound_net_log =
      net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD);
  DownloadItemImpl* download =
      item_factory_->CreateActiveItem(this, id, info, bound_net_log);
  downloads_[id] = download;
  downloads_by_guid_[download->GetGuid()] = download;
  return download;
}

void DownloadManagerImpl::GetNextId(const DownloadIdCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (delegate_) {
    delegate_->GetNextId(callback);
    return;
  }
  static uint32_t next_id = content::DownloadItem::kInvalidId + 1;
  callback.Run(next_id++);
}

void DownloadManagerImpl::DetermineDownloadTarget(
    DownloadItemImpl* item, const DownloadTargetCallback& callback) {
  // Note that this next call relies on
  // DownloadItemImplDelegate::DownloadTargetCallback and
  // DownloadManagerDelegate::DownloadTargetCallback having the same
  // type.  If the types ever diverge, gasket code will need to
  // be written here.
  if (!delegate_ || !delegate_->DetermineDownloadTarget(item, callback)) {
    base::FilePath target_path = item->GetForcedFilePath();
    // TODO(asanka): Determine a useful path if |target_path| is empty.
    callback.Run(target_path,
                 DownloadItem::TARGET_DISPOSITION_OVERWRITE,
                 DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                 target_path);
  }
}

bool DownloadManagerImpl::ShouldCompleteDownload(
    DownloadItemImpl* item, const base::Closure& complete_callback) {
  if (!delegate_ ||
      delegate_->ShouldCompleteDownload(item, complete_callback)) {
    return true;
  }
  // Otherwise, the delegate has accepted responsibility to run the
  // callback when the download is ready for completion.
  return false;
}

bool DownloadManagerImpl::ShouldOpenFileBasedOnExtension(
    const base::FilePath& path) {
  if (!delegate_)
    return false;

  return delegate_->ShouldOpenFileBasedOnExtension(path);
}

bool DownloadManagerImpl::ShouldOpenDownload(
    DownloadItemImpl* item, const ShouldOpenDownloadCallback& callback) {
  if (!delegate_)
    return true;

  // Relies on DownloadItemImplDelegate::ShouldOpenDownloadCallback and
  // DownloadManagerDelegate::DownloadOpenDelayedCallback "just happening"
  // to have the same type :-}.
  return delegate_->ShouldOpenDownload(item, callback);
}

void DownloadManagerImpl::SetDelegate(DownloadManagerDelegate* delegate) {
  delegate_ = delegate;
}

DownloadManagerDelegate* DownloadManagerImpl::GetDelegate() const {
  return delegate_;
}

void DownloadManagerImpl::Shutdown() {
  DVLOG(20) << __FUNCTION__ << "()"
            << " shutdown_needed_ = " << shutdown_needed_;
  if (!shutdown_needed_)
    return;
  shutdown_needed_ = false;

  FOR_EACH_OBSERVER(Observer, observers_, ManagerGoingDown(this));
  // TODO(benjhayden): Consider clearing observers_.

  // If there are in-progress downloads, cancel them. This also goes for
  // dangerous downloads which will remain in history if they aren't explicitly
  // accepted or discarded. Canceling will remove the intermediate download
  // file.
  for (const auto& it : downloads_) {
    DownloadItemImpl* download = it.second;
    if (download->GetState() == DownloadItem::IN_PROGRESS)
      download->Cancel(false);
  }
  STLDeleteValues(&downloads_);
  downloads_by_guid_.clear();
  url_downloaders_.clear();

  // We'll have nothing more to report to the observers after this point.
  observers_.Clear();

  if (delegate_)
    delegate_->Shutdown();
  delegate_ = NULL;
}

void DownloadManagerImpl::StartDownload(
    std::unique_ptr<DownloadCreateInfo> info,
    std::unique_ptr<ByteStreamReader> stream,
    const DownloadUrlParameters::OnStartedCallback& on_started) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(info);
  // |stream| is only non-nil if the download request was successful.
  DCHECK((info->result == DOWNLOAD_INTERRUPT_REASON_NONE && stream.get()) ||
         (info->result != DOWNLOAD_INTERRUPT_REASON_NONE && !stream.get()));
  DVLOG(20) << __FUNCTION__ << "()"
            << " result=" << DownloadInterruptReasonToString(info->result);
  uint32_t download_id = info->download_id;
  const bool new_download = (download_id == content::DownloadItem::kInvalidId);
  base::Callback<void(uint32_t)> got_id(base::Bind(
      &DownloadManagerImpl::StartDownloadWithId, weak_factory_.GetWeakPtr(),
      base::Passed(&info), base::Passed(&stream), on_started, new_download));
  if (new_download) {
    GetNextId(got_id);
  } else {
    got_id.Run(download_id);
  }
}

void DownloadManagerImpl::StartDownloadWithId(
    std::unique_ptr<DownloadCreateInfo> info,
    std::unique_ptr<ByteStreamReader> stream,
    const DownloadUrlParameters::OnStartedCallback& on_started,
    bool new_download,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(content::DownloadItem::kInvalidId, id);
  DownloadItemImpl* download = NULL;
  if (new_download) {
    download = CreateActiveItem(id, *info);
  } else {
    DownloadMap::iterator item_iterator = downloads_.find(id);
    // Trying to resume an interrupted download.
    if (item_iterator == downloads_.end() ||
        (item_iterator->second->GetState() == DownloadItem::CANCELLED)) {
      // If the download is no longer known to the DownloadManager, then it was
      // removed after it was resumed. Ignore. If the download is cancelled
      // while resuming, then also ignore the request.
      info->request_handle->CancelRequest();
      if (!on_started.is_null())
        on_started.Run(NULL, DOWNLOAD_INTERRUPT_REASON_USER_CANCELED);
      // The ByteStreamReader lives and dies on the FILE thread.
      if (info->result == DOWNLOAD_INTERRUPT_REASON_NONE)
        BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE,
                                  stream.release());
      return;
    }
    download = item_iterator->second;
  }

  base::FilePath default_download_directory;
  if (delegate_) {
    base::FilePath website_save_directory;  // Unused
    bool skip_dir_check = false;            // Unused
    delegate_->GetSaveDir(GetBrowserContext(), &website_save_directory,
                          &default_download_directory, &skip_dir_check);
  }

  std::unique_ptr<DownloadFile> download_file;

  if (info->result == DOWNLOAD_INTERRUPT_REASON_NONE) {
    DCHECK(stream.get());
    download_file.reset(
        file_factory_->CreateFile(std::move(info->save_info),
                                  default_download_directory,
                                  std::move(stream),
                                  download->GetBoundNetLog(),
                                  download->DestinationObserverAsWeakPtr()));
  }
  // It is important to leave info->save_info intact in the case of an interrupt
  // so that the DownloadItem can salvage what it can out of a failed resumption
  // attempt.

  download->Start(std::move(download_file), std::move(info->request_handle),
                  *info);

  // For interrupted downloads, Start() will transition the state to
  // IN_PROGRESS and consumers will be notified via OnDownloadUpdated().
  // For new downloads, we notify here, rather than earlier, so that
  // the download_file is bound to download and all the usual
  // setters (e.g. Cancel) work.
  if (new_download)
    FOR_EACH_OBSERVER(Observer, observers_, OnDownloadCreated(this, download));

  if (!on_started.is_null())
    on_started.Run(download, DOWNLOAD_INTERRUPT_REASON_NONE);
}

void DownloadManagerImpl::CheckForHistoryFilesRemoval() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const auto& it : downloads_) {
    DownloadItemImpl* item = it.second;
    CheckForFileRemoval(item);
  }
}

void DownloadManagerImpl::CheckForFileRemoval(DownloadItemImpl* download_item) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if ((download_item->GetState() == DownloadItem::COMPLETE) &&
      !download_item->GetFileExternallyRemoved() &&
      delegate_) {
    delegate_->CheckForFileExistence(
        download_item,
        base::Bind(&DownloadManagerImpl::OnFileExistenceChecked,
                   weak_factory_.GetWeakPtr(), download_item->GetId()));
  }
}

void DownloadManagerImpl::OnFileExistenceChecked(uint32_t download_id,
                                                 bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!result) {  // File does not exist.
    if (ContainsKey(downloads_, download_id))
      downloads_[download_id]->OnDownloadedFileRemoved();
  }
}

BrowserContext* DownloadManagerImpl::GetBrowserContext() const {
  return browser_context_;
}

void DownloadManagerImpl::CreateSavePackageDownloadItem(
    const base::FilePath& main_file_path,
    const GURL& page_url,
    const std::string& mime_type,
    std::unique_ptr<DownloadRequestHandleInterface> request_handle,
    const DownloadItemImplCreated& item_created) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetNextId(base::Bind(
      &DownloadManagerImpl::CreateSavePackageDownloadItemWithId,
      weak_factory_.GetWeakPtr(), main_file_path, page_url, mime_type,
      base::Passed(std::move(request_handle)), item_created));
}

void DownloadManagerImpl::CreateSavePackageDownloadItemWithId(
    const base::FilePath& main_file_path,
    const GURL& page_url,
    const std::string& mime_type,
    std::unique_ptr<DownloadRequestHandleInterface> request_handle,
    const DownloadItemImplCreated& item_created,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(content::DownloadItem::kInvalidId, id);
  DCHECK(!ContainsKey(downloads_, id));
  net::BoundNetLog bound_net_log =
      net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD);
  DownloadItemImpl* download_item = item_factory_->CreateSavePageItem(
      this, id, main_file_path, page_url, mime_type, std::move(request_handle),
      bound_net_log);
  downloads_[download_item->GetId()] = download_item;
  DCHECK(!ContainsKey(downloads_by_guid_, download_item->GetGuid()));
  downloads_by_guid_[download_item->GetGuid()] = download_item;
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadCreated(
      this, download_item));
  if (!item_created.is_null())
    item_created.Run(download_item);
}

void DownloadManagerImpl::OnSavePackageSuccessfullyFinished(
    DownloadItem* download_item) {
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnSavePackageSuccessfullyFinished(this, download_item));
}

// Resume a download of a specific URL. We send the request to the
// ResourceDispatcherHost, and let it send us responses like a regular
// download.
void DownloadManagerImpl::ResumeInterruptedDownload(
    std::unique_ptr<content::DownloadUrlParameters> params,
    uint32_t id) {
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BeginDownload, base::Passed(&params),
                 browser_context_->GetResourceContext(), id,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DownloadManagerImpl::AddUrlDownloader,
                 weak_factory_.GetWeakPtr()));
}

void DownloadManagerImpl::SetDownloadItemFactoryForTesting(
    std::unique_ptr<DownloadItemFactory> item_factory) {
  item_factory_ = std::move(item_factory);
}

void DownloadManagerImpl::SetDownloadFileFactoryForTesting(
    std::unique_ptr<DownloadFileFactory> file_factory) {
  file_factory_ = std::move(file_factory);
}

DownloadFileFactory* DownloadManagerImpl::GetDownloadFileFactoryForTesting() {
  return file_factory_.get();
}

void DownloadManagerImpl::DownloadRemoved(DownloadItemImpl* download) {
  if (!download)
    return;

  downloads_by_guid_.erase(download->GetGuid());

  uint32_t download_id = download->GetId();
  if (downloads_.erase(download_id) == 0)
    return;
  delete download;
}

void DownloadManagerImpl::AddUrlDownloader(
    std::unique_ptr<UrlDownloader, BrowserThread::DeleteOnIOThread>
        downloader) {
  if (downloader)
    url_downloaders_.push_back(std::move(downloader));
}

void DownloadManagerImpl::RemoveUrlDownloader(UrlDownloader* downloader) {
  for (auto ptr = url_downloaders_.begin(); ptr != url_downloaders_.end();
       ++ptr) {
    if (ptr->get() == downloader) {
      url_downloaders_.erase(ptr);
      return;
    }
  }
}

namespace {

bool EmptyFilter(const GURL& url) {
  return true;
}

bool RemoveDownloadByURLAndTime(
    const base::Callback<bool(const GURL&)>& url_filter,
          base::Time remove_begin,
          base::Time remove_end,
          const DownloadItemImpl* download_item) {
  return url_filter.Run(download_item->GetURL()) &&
         download_item->GetStartTime() >= remove_begin &&
         (remove_end.is_null() || download_item->GetStartTime() < remove_end);
}

}  // namespace

int DownloadManagerImpl::RemoveDownloads(const DownloadRemover& remover) {
  int count = 0;
  DownloadMap::const_iterator it = downloads_.begin();
  while (it != downloads_.end()) {
    DownloadItemImpl* download = it->second;

    // Increment done here to protect against invalidation below.
    ++it;

    if (download->GetState() != DownloadItem::IN_PROGRESS &&
        remover.Run(download)) {
      download->Remove();
      count++;
    }
  }
  return count;
}

int DownloadManagerImpl::RemoveDownloadsByURLAndTime(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time remove_begin,
    base::Time remove_end) {
  return RemoveDownloads(base::Bind(&RemoveDownloadByURLAndTime,
                                    url_filter,
                                    remove_begin, remove_end));
}

int DownloadManagerImpl::RemoveAllDownloads() {
  const base::Callback<bool(const GURL&)> empty_filter =
      base::Bind(&EmptyFilter);
  // The null times make the date range unbounded.
  int num_deleted = RemoveDownloadsByURLAndTime(
      empty_filter, base::Time(), base::Time());
  RecordClearAllSize(num_deleted);
  return num_deleted;
}

void DownloadManagerImpl::DownloadUrl(
    std::unique_ptr<DownloadUrlParameters> params) {
  if (params->post_id() >= 0) {
    // Check this here so that the traceback is more useful.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
  }
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&BeginDownload, base::Passed(&params),
                 browser_context_->GetResourceContext(),
                 content::DownloadItem::kInvalidId, weak_factory_.GetWeakPtr()),
      base::Bind(&DownloadManagerImpl::AddUrlDownloader,
                 weak_factory_.GetWeakPtr()));
}

void DownloadManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DownloadManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

DownloadItem* DownloadManagerImpl::CreateDownloadItem(
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
    DownloadItem::DownloadState state,
    DownloadDangerType danger_type,
    DownloadInterruptReason interrupt_reason,
    bool opened) {
  if (ContainsKey(downloads_, id)) {
    NOTREACHED();
    return NULL;
  }
  DCHECK(!ContainsKey(downloads_by_guid_, guid));
  DownloadItemImpl* item = item_factory_->CreatePersistedItem(
      this, guid, id, current_path, target_path, url_chain, referrer_url,
      site_url, tab_url, tab_refererr_url, mime_type, original_mime_type,
      start_time, end_time, etag, last_modified, received_bytes, total_bytes,
      hash, state, danger_type, interrupt_reason, opened,
      net::BoundNetLog::Make(net_log_, net::NetLog::SOURCE_DOWNLOAD));
  downloads_[id] = item;
  downloads_by_guid_[guid] = item;
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadCreated(this, item));
  DVLOG(20) << __FUNCTION__ << "() download = " << item->DebugString(true);
  return item;
}

int DownloadManagerImpl::InProgressCount() const {
  int count = 0;
  for (const auto& it : downloads_) {
    if (it.second->GetState() == DownloadItem::IN_PROGRESS)
      ++count;
  }
  return count;
}

int DownloadManagerImpl::NonMaliciousInProgressCount() const {
  int count = 0;
  for (const auto& it : downloads_) {
    if (it.second->GetState() == DownloadItem::IN_PROGRESS &&
        it.second->GetDangerType() != DOWNLOAD_DANGER_TYPE_DANGEROUS_URL &&
        it.second->GetDangerType() != DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT &&
        it.second->GetDangerType() != DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST &&
        it.second->GetDangerType() !=
            DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED) {
      ++count;
    }
  }
  return count;
}

DownloadItem* DownloadManagerImpl::GetDownload(uint32_t download_id) {
  return ContainsKey(downloads_, download_id) ? downloads_[download_id]
                                              : nullptr;
}

DownloadItem* DownloadManagerImpl::GetDownloadByGuid(const std::string& guid) {
  DCHECK(guid == base::ToUpperASCII(guid));
  return ContainsKey(downloads_by_guid_, guid) ? downloads_by_guid_[guid]
                                               : nullptr;
}

void DownloadManagerImpl::GetAllDownloads(DownloadVector* downloads) {
  for (const auto& it : downloads_) {
    downloads->push_back(it.second);
  }
}

void DownloadManagerImpl::OpenDownload(DownloadItemImpl* download) {
  int num_unopened = 0;
  for (const auto& it : downloads_) {
    DownloadItemImpl* item = it.second;
    if ((item->GetState() == DownloadItem::COMPLETE) &&
        !item->GetOpened())
      ++num_unopened;
  }
  RecordOpensOutstanding(num_unopened);

  if (delegate_)
    delegate_->OpenDownload(download);
}

void DownloadManagerImpl::ShowDownloadInShell(DownloadItemImpl* download) {
  if (delegate_)
    delegate_->ShowDownloadInShell(download);
}

}  // namespace content
