// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_job.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context.h"
#include "url/origin.h"

namespace content {

namespace {

const int kBufferSize = 32768;
const size_t kMaxConcurrentUrlFetches = 2;
const int kMax503Retries = 3;

std::string FormatUrlErrorMessage(
      const char* format, const GURL& url,
      AppCacheUpdateJob::ResultType error,
      int response_code) {
    // Show the net response code if we have one.
    int code = response_code;
    if (error != AppCacheUpdateJob::SERVER_ERROR)
      code = static_cast<int>(error);
    return base::StringPrintf(format, code, url.spec().c_str());
}

bool IsEvictableError(AppCacheUpdateJob::ResultType result,
                      const AppCacheErrorDetails& details) {
  switch (result) {
    case AppCacheUpdateJob::DB_ERROR:
    case AppCacheUpdateJob::DISKCACHE_ERROR:
    case AppCacheUpdateJob::QUOTA_ERROR:
    case AppCacheUpdateJob::NETWORK_ERROR:
    case AppCacheUpdateJob::CANCELLED_ERROR:
      return false;

    case AppCacheUpdateJob::REDIRECT_ERROR:
    case AppCacheUpdateJob::SERVER_ERROR:
    case AppCacheUpdateJob::SECURITY_ERROR:
      return true;

    case AppCacheUpdateJob::MANIFEST_ERROR:
      return details.reason == APPCACHE_SIGNATURE_ERROR;

    default:
      NOTREACHED();
      return true;
  }
}

void EmptyCompletionCallback(int result) {}

}  // namespace

// Helper class for collecting hosts per frontend when sending notifications
// so that only one notification is sent for all hosts using the same frontend.
class HostNotifier {
 public:
  typedef std::vector<int> HostIds;
  typedef std::map<AppCacheFrontend*, HostIds> NotifyHostMap;

  // Caller is responsible for ensuring there will be no duplicate hosts.
  void AddHost(AppCacheHost* host) {
    std::pair<NotifyHostMap::iterator , bool> ret = hosts_to_notify.insert(
        NotifyHostMap::value_type(host->frontend(), HostIds()));
    ret.first->second.push_back(host->host_id());
  }

  void AddHosts(const std::set<AppCacheHost*>& hosts) {
    for (std::set<AppCacheHost*>::const_iterator it = hosts.begin();
         it != hosts.end(); ++it) {
      AddHost(*it);
    }
  }

  void SendNotifications(AppCacheEventID event_id) {
    for (NotifyHostMap::iterator it = hosts_to_notify.begin();
         it != hosts_to_notify.end(); ++it) {
      AppCacheFrontend* frontend = it->first;
      frontend->OnEventRaised(it->second, event_id);
    }
  }

  void SendProgressNotifications(
      const GURL& url, int num_total, int num_complete) {
    for (NotifyHostMap::iterator it = hosts_to_notify.begin();
         it != hosts_to_notify.end(); ++it) {
      AppCacheFrontend* frontend = it->first;
      frontend->OnProgressEventRaised(it->second, url,
                                      num_total, num_complete);
    }
  }

  void SendErrorNotifications(const AppCacheErrorDetails& details) {
    DCHECK(!details.message.empty());
    for (NotifyHostMap::iterator it = hosts_to_notify.begin();
         it != hosts_to_notify.end(); ++it) {
      AppCacheFrontend* frontend = it->first;
      frontend->OnErrorEventRaised(it->second, details);
    }
  }

  void SendLogMessage(const std::string& message) {
    for (NotifyHostMap::iterator it = hosts_to_notify.begin();
         it != hosts_to_notify.end(); ++it) {
      AppCacheFrontend* frontend = it->first;
      for (HostIds::iterator id = it->second.begin();
           id != it->second.end(); ++id) {
        frontend->OnLogMessage(*id, APPCACHE_LOG_WARNING, message);
      }
    }
  }

 private:
  NotifyHostMap hosts_to_notify;
};

AppCacheUpdateJob::UrlToFetch::UrlToFetch(const GURL& url,
                                          bool checked,
                                          AppCacheResponseInfo* info)
    : url(url),
      storage_checked(checked),
      existing_response_info(info) {
}

AppCacheUpdateJob::UrlToFetch::UrlToFetch(const UrlToFetch& other) = default;

AppCacheUpdateJob::UrlToFetch::~UrlToFetch() {
}

// Helper class to fetch resources. Depending on the fetch type,
// can either fetch to an in-memory string or write the response
// data out to the disk cache.
AppCacheUpdateJob::URLFetcher::URLFetcher(const GURL& url,
                                          FetchType fetch_type,
                                          AppCacheUpdateJob* job)
    : url_(url),
      job_(job),
      fetch_type_(fetch_type),
      retry_503_attempts_(0),
      buffer_(new net::IOBuffer(kBufferSize)),
      request_(job->service_->request_context()
                   ->CreateRequest(url, net::DEFAULT_PRIORITY, this)),
      result_(UPDATE_OK),
      redirect_response_code_(-1) {}

AppCacheUpdateJob::URLFetcher::~URLFetcher() {
}

void AppCacheUpdateJob::URLFetcher::Start() {
  request_->set_first_party_for_cookies(job_->manifest_url_);
  request_->set_initiator(url::Origin(job_->manifest_url_));
  if (fetch_type_ == MANIFEST_FETCH && job_->doing_full_update_check_)
    request_->SetLoadFlags(request_->load_flags() | net::LOAD_BYPASS_CACHE);
  else if (existing_response_headers_.get())
    AddConditionalHeaders(existing_response_headers_.get());
  request_->Start();
}

void AppCacheUpdateJob::URLFetcher::OnReceivedRedirect(
    net::URLRequest* request,
    const net::RedirectInfo& redirect_info,
    bool* defer_redirect) {
  DCHECK_EQ(request_.get(), request);
  // Redirect is not allowed by the update process.
  job_->MadeProgress();
  redirect_response_code_ = request->GetResponseCode();
  request->Cancel();
  result_ = REDIRECT_ERROR;
  OnResponseCompleted();
}

void AppCacheUpdateJob::URLFetcher::OnResponseStarted(
    net::URLRequest *request) {
  DCHECK_EQ(request_.get(), request);
  int response_code = -1;
  if (request->status().is_success()) {
    response_code = request->GetResponseCode();
    job_->MadeProgress();
  }

  if ((response_code / 100) != 2) {
    if (response_code > 0)
      result_ = SERVER_ERROR;
    else
      result_ = NETWORK_ERROR;
    OnResponseCompleted();
    return;
  }

  if (url_.SchemeIsCryptographic()) {
    // Do not cache content with cert errors.
    // Also, we willfully violate the HTML5 spec at this point in order
    // to support the appcaching of cross-origin HTTPS resources.
    // We've opted for a milder constraint and allow caching unless
    // the resource has a "no-store" header. A spec change has been
    // requested on the whatwg list.
    // See http://code.google.com/p/chromium/issues/detail?id=69594
    // TODO(michaeln): Consider doing this for cross-origin HTTP too.
    const net::HttpNetworkSession::Params* session_params =
        request->context()->GetNetworkSessionParams();
    bool ignore_cert_errors = session_params &&
                              session_params->ignore_certificate_errors;
    if ((net::IsCertStatusError(request->ssl_info().cert_status) &&
            !ignore_cert_errors) ||
        (url_.GetOrigin() != job_->manifest_url_.GetOrigin() &&
            request->response_headers()->
                HasHeaderValue("cache-control", "no-store"))) {
      DCHECK_EQ(-1, redirect_response_code_);
      request->Cancel();
      result_ = SECURITY_ERROR;
      OnResponseCompleted();
      return;
    }
  }

  // Write response info to storage for URL fetches. Wait for async write
  // completion before reading any response data.
  if (fetch_type_ == URL_FETCH || fetch_type_ == MASTER_ENTRY_FETCH) {
    response_writer_.reset(job_->CreateResponseWriter());
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer(
        new HttpResponseInfoIOBuffer(
            new net::HttpResponseInfo(request->response_info())));
    response_writer_->WriteInfo(
        io_buffer.get(),
        base::Bind(&URLFetcher::OnWriteComplete, base::Unretained(this)));
  } else {
    ReadResponseData();
  }
}

void AppCacheUpdateJob::URLFetcher::OnReadCompleted(
    net::URLRequest* request, int bytes_read) {
  DCHECK_EQ(request_.get(), request);
  bool data_consumed = true;
  if (request->status().is_success() && bytes_read > 0) {
    job_->MadeProgress();
    data_consumed = ConsumeResponseData(bytes_read);
    if (data_consumed) {
      bytes_read = 0;
      while (request->Read(buffer_.get(), kBufferSize, &bytes_read)) {
        if (bytes_read > 0) {
          data_consumed = ConsumeResponseData(bytes_read);
          if (!data_consumed)
            break;  // wait for async data processing, then read more
        } else {
          break;
        }
      }
    }
  }
  if (data_consumed && !request->status().is_io_pending()) {
    DCHECK_EQ(UPDATE_OK, result_);
    OnResponseCompleted();
  }
}

void AppCacheUpdateJob::URLFetcher::AddConditionalHeaders(
    const net::HttpResponseHeaders* headers) {
  DCHECK(request_);
  DCHECK(headers);
  net::HttpRequestHeaders extra_headers;

  // Add If-Modified-Since header if response info has Last-Modified header.
  const std::string last_modified = "Last-Modified";
  std::string last_modified_value;
  headers->EnumerateHeader(nullptr, last_modified, &last_modified_value);
  if (!last_modified_value.empty()) {
    extra_headers.SetHeader(net::HttpRequestHeaders::kIfModifiedSince,
                            last_modified_value);
  }

  // Add If-None-Match header if response info has ETag header.
  const std::string etag = "ETag";
  std::string etag_value;
  headers->EnumerateHeader(nullptr, etag, &etag_value);
  if (!etag_value.empty()) {
    extra_headers.SetHeader(net::HttpRequestHeaders::kIfNoneMatch,
                            etag_value);
  }
  if (!extra_headers.IsEmpty())
    request_->SetExtraRequestHeaders(extra_headers);
}

void  AppCacheUpdateJob::URLFetcher::OnWriteComplete(int result) {
  if (result < 0) {
    request_->Cancel();
    result_ = DISKCACHE_ERROR;
    OnResponseCompleted();
    return;
  }
  ReadResponseData();
}

void AppCacheUpdateJob::URLFetcher::ReadResponseData() {
  InternalUpdateState state = job_->internal_state_;
  if (state == CACHE_FAILURE || state == CANCELLED || state == COMPLETED)
    return;
  int bytes_read = 0;
  request_->Read(buffer_.get(), kBufferSize, &bytes_read);
  OnReadCompleted(request_.get(), bytes_read);
}

// Returns false if response data is processed asynchronously, in which
// case ReadResponseData will be invoked when it is safe to continue
// reading more response data from the request.
bool AppCacheUpdateJob::URLFetcher::ConsumeResponseData(int bytes_read) {
  DCHECK_GT(bytes_read, 0);
  switch (fetch_type_) {
    case MANIFEST_FETCH:
    case MANIFEST_REFETCH:
      manifest_data_.append(buffer_->data(), bytes_read);
      break;
    case URL_FETCH:
    case MASTER_ENTRY_FETCH:
      DCHECK(response_writer_.get());
      response_writer_->WriteData(
          buffer_.get(),
          bytes_read,
          base::Bind(&URLFetcher::OnWriteComplete, base::Unretained(this)));
      return false;  // wait for async write completion to continue reading
    default:
      NOTREACHED();
  }
  return true;
}

void AppCacheUpdateJob::URLFetcher::OnResponseCompleted() {
  if (request_->status().is_success())
    job_->MadeProgress();

  // Retry for 503s where retry-after is 0.
  if (request_->status().is_success() &&
      request_->GetResponseCode() == 503 &&
      MaybeRetryRequest()) {
    return;
  }

  switch (fetch_type_) {
    case MANIFEST_FETCH:
      job_->HandleManifestFetchCompleted(this);
      break;
    case URL_FETCH:
      job_->HandleUrlFetchCompleted(this);
      break;
    case MASTER_ENTRY_FETCH:
      job_->HandleMasterEntryFetchCompleted(this);
      break;
    case MANIFEST_REFETCH:
      job_->HandleManifestRefetchCompleted(this);
      break;
    default:
      NOTREACHED();
  }

  delete this;
}

bool AppCacheUpdateJob::URLFetcher::MaybeRetryRequest() {
  if (retry_503_attempts_ >= kMax503Retries ||
      !request_->response_headers()->HasHeaderValue("retry-after", "0")) {
    return false;
  }
  ++retry_503_attempts_;
  result_ = UPDATE_OK;
  request_ = job_->service_->request_context()->CreateRequest(
      url_, net::DEFAULT_PRIORITY, this);
  Start();
  return true;
}

AppCacheUpdateJob::AppCacheUpdateJob(AppCacheServiceImpl* service,
                                     AppCacheGroup* group)
    : service_(service),
      manifest_url_(group->manifest_url()),
      group_(group),
      update_type_(UNKNOWN_TYPE),
      internal_state_(FETCH_MANIFEST),
      doing_full_update_check_(false),
      master_entries_completed_(0),
      url_fetches_completed_(0),
      manifest_fetcher_(NULL),
      manifest_has_valid_mime_type_(false),
      stored_state_(UNSTORED),
      storage_(service->storage()),
      weak_factory_(this) {
    service_->AddObserver(this);
}

AppCacheUpdateJob::~AppCacheUpdateJob() {
  if (service_)
    service_->RemoveObserver(this);
  if (internal_state_ != COMPLETED)
    Cancel();

  DCHECK(!inprogress_cache_.get());
  DCHECK(pending_master_entries_.empty());

  // The job must not outlive any of its fetchers.
  CHECK(!manifest_fetcher_);
  CHECK(pending_url_fetches_.empty());
  CHECK(master_entry_fetches_.empty());

  if (group_)
    group_->SetUpdateAppCacheStatus(AppCacheGroup::IDLE);
}

void AppCacheUpdateJob::StartUpdate(AppCacheHost* host,
                                    const GURL& new_master_resource) {
  DCHECK(group_->update_job() == this);
  DCHECK(!group_->is_obsolete());

  bool is_new_pending_master_entry = false;
  if (!new_master_resource.is_empty()) {
    DCHECK(new_master_resource == host->pending_master_entry_url());
    DCHECK(!new_master_resource.has_ref());
    DCHECK(new_master_resource.GetOrigin() == manifest_url_.GetOrigin());

    if (ContainsKey(failed_master_entries_, new_master_resource))
      return;

    // Cannot add more to this update if already terminating.
    if (IsTerminating()) {
      group_->QueueUpdate(host, new_master_resource);
      return;
    }

    std::pair<PendingMasters::iterator, bool> ret =
        pending_master_entries_.insert(
            PendingMasters::value_type(new_master_resource, PendingHosts()));
    is_new_pending_master_entry = ret.second;
    ret.first->second.push_back(host);
    host->AddObserver(this);
  }

  // Notify host (if any) if already checking or downloading.
  AppCacheGroup::UpdateAppCacheStatus update_status = group_->update_status();
  if (update_status == AppCacheGroup::CHECKING ||
      update_status == AppCacheGroup::DOWNLOADING) {
    if (host) {
      NotifySingleHost(host, APPCACHE_CHECKING_EVENT);
      if (update_status == AppCacheGroup::DOWNLOADING)
        NotifySingleHost(host, APPCACHE_DOWNLOADING_EVENT);

      // Add to fetch list or an existing entry if already fetched.
      if (!new_master_resource.is_empty()) {
        AddMasterEntryToFetchList(host, new_master_resource,
                                  is_new_pending_master_entry);
      }
    }
    return;
  }

  // Begin update process for the group.
  MadeProgress();
  group_->SetUpdateAppCacheStatus(AppCacheGroup::CHECKING);
  if (group_->HasCache()) {
    base::TimeDelta kFullUpdateInterval = base::TimeDelta::FromHours(24);
    update_type_ = UPGRADE_ATTEMPT;
    base::TimeDelta time_since_last_check =
        base::Time::Now() - group_->last_full_update_check_time();
    doing_full_update_check_ = time_since_last_check > kFullUpdateInterval;
    NotifyAllAssociatedHosts(APPCACHE_CHECKING_EVENT);
  } else {
    update_type_ = CACHE_ATTEMPT;
    doing_full_update_check_ = true;
    DCHECK(host);
    NotifySingleHost(host, APPCACHE_CHECKING_EVENT);
  }

  if (!new_master_resource.is_empty()) {
    AddMasterEntryToFetchList(host, new_master_resource,
                              is_new_pending_master_entry);
  }

  BrowserThread::PostAfterStartupTask(
      FROM_HERE, base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&AppCacheUpdateJob::FetchManifest, weak_factory_.GetWeakPtr(),
                 true));
}

AppCacheResponseWriter* AppCacheUpdateJob::CreateResponseWriter() {
  AppCacheResponseWriter* writer =
      storage_->CreateResponseWriter(manifest_url_);
  stored_response_ids_.push_back(writer->response_id());
  return writer;
}

void AppCacheUpdateJob::HandleCacheFailure(
    const AppCacheErrorDetails& error_details,
    ResultType result,
    const GURL& failed_resource_url) {
  // 6.9.4 cache failure steps 2-8.
  DCHECK(internal_state_ != CACHE_FAILURE);
  DCHECK(!error_details.message.empty());
  DCHECK(result != UPDATE_OK);
  internal_state_ = CACHE_FAILURE;
  LogHistogramStats(result, failed_resource_url);
  CancelAllUrlFetches();
  CancelAllMasterEntryFetches(error_details);
  NotifyAllError(error_details);
  DiscardInprogressCache();
  internal_state_ = COMPLETED;

  if (update_type_ == CACHE_ATTEMPT ||
      !IsEvictableError(result, error_details) ||
      service_->storage() != storage_) {
    DeleteSoon();
    return;
  }

  if (group_->first_evictable_error_time().is_null()) {
    group_->set_first_evictable_error_time(base::Time::Now());
    storage_->StoreEvictionTimes(group_);
    DeleteSoon();
    return;
  }

  base::TimeDelta kMaxEvictableErrorDuration = base::TimeDelta::FromDays(14);
  base::TimeDelta error_duration =
      base::Time::Now() - group_->first_evictable_error_time();
  if (error_duration > kMaxEvictableErrorDuration) {
    // Break the connection with the group prior to calling
    // DeleteAppCacheGroup, otherwise that method would delete |this|
    // and we need the stack to unwind prior to deletion.
    group_->SetUpdateAppCacheStatus(AppCacheGroup::IDLE);
    group_ = NULL;
    service_->DeleteAppCacheGroup(manifest_url_,
                                  base::Bind(EmptyCompletionCallback));
  }

  DeleteSoon();  // To unwind the stack prior to deletion.
}

void AppCacheUpdateJob::FetchManifest(bool is_first_fetch) {
  DCHECK(!manifest_fetcher_);
  manifest_fetcher_ = new URLFetcher(
     manifest_url_,
     is_first_fetch ? URLFetcher::MANIFEST_FETCH :
                      URLFetcher::MANIFEST_REFETCH,
     this);

  if (is_first_fetch) {
    // Maybe load the cached headers to make a condiditional request.
    AppCacheEntry* entry = (update_type_ == UPGRADE_ATTEMPT) ?
        group_->newest_complete_cache()->GetEntry(manifest_url_) : NULL;
    if (entry && !doing_full_update_check_) {
      // Asynchronously load response info for manifest from newest cache.
      storage_->LoadResponseInfo(manifest_url_, entry->response_id(), this);
      return;
    }
    manifest_fetcher_->Start();
    return;
  }

  DCHECK(internal_state_ == REFETCH_MANIFEST);
  DCHECK(manifest_response_info_.get());
  manifest_fetcher_->set_existing_response_headers(
      manifest_response_info_->headers.get());
  manifest_fetcher_->Start();
}


void AppCacheUpdateJob::HandleManifestFetchCompleted(
    URLFetcher* fetcher) {
  DCHECK_EQ(internal_state_, FETCH_MANIFEST);
  DCHECK_EQ(manifest_fetcher_, fetcher);
  manifest_fetcher_ = NULL;

  net::URLRequest* request = fetcher->request();
  int response_code = -1;
  bool is_valid_response_code = false;
  if (request->status().is_success()) {
    response_code = request->GetResponseCode();
    is_valid_response_code = (response_code / 100 == 2);

    std::string mime_type;
    request->GetMimeType(&mime_type);
    manifest_has_valid_mime_type_ = (mime_type == "text/cache-manifest");
  }

  if (is_valid_response_code) {
    manifest_data_ = fetcher->manifest_data();
    manifest_response_info_.reset(
        new net::HttpResponseInfo(request->response_info()));
    if (update_type_ == UPGRADE_ATTEMPT)
      CheckIfManifestChanged();  // continues asynchronously
    else
      ContinueHandleManifestFetchCompleted(true);
  } else if (response_code == 304 && update_type_ == UPGRADE_ATTEMPT) {
    ContinueHandleManifestFetchCompleted(false);
  } else if ((response_code == 404 || response_code == 410) &&
             update_type_ == UPGRADE_ATTEMPT) {
    storage_->MakeGroupObsolete(group_, this, response_code);  // async
  } else {
    const char kFormatString[] = "Manifest fetch failed (%d) %s";
    std::string message = FormatUrlErrorMessage(
        kFormatString, manifest_url_, fetcher->result(), response_code);
    HandleCacheFailure(AppCacheErrorDetails(message,
                                    APPCACHE_MANIFEST_ERROR,
                                    manifest_url_,
                                    response_code,
                                    false /*is_cross_origin*/),
                       fetcher->result(),
                       GURL());
  }
}

void AppCacheUpdateJob::OnGroupMadeObsolete(AppCacheGroup* group,
                                            bool success,
                                            int response_code) {
  DCHECK(master_entry_fetches_.empty());
  CancelAllMasterEntryFetches(AppCacheErrorDetails(
      "The cache has been made obsolete, "
      "the manifest file returned 404 or 410",
      APPCACHE_MANIFEST_ERROR,
      GURL(),
      response_code,
      false /*is_cross_origin*/));
  if (success) {
    DCHECK(group->is_obsolete());
    NotifyAllAssociatedHosts(APPCACHE_OBSOLETE_EVENT);
    internal_state_ = COMPLETED;
    MaybeCompleteUpdate();
  } else {
    // Treat failure to mark group obsolete as a cache failure.
    HandleCacheFailure(AppCacheErrorDetails(
                           "Failed to mark the cache as obsolete",
                           APPCACHE_UNKNOWN_ERROR, GURL(),  0,
                           false /*is_cross_origin*/),
                       DB_ERROR,
                       GURL());
  }
}

void AppCacheUpdateJob::ContinueHandleManifestFetchCompleted(bool changed) {
  DCHECK(internal_state_ == FETCH_MANIFEST);

  if (!changed) {
    DCHECK(update_type_ == UPGRADE_ATTEMPT);
    internal_state_ = NO_UPDATE;

    // Wait for pending master entries to download.
    FetchMasterEntries();
    MaybeCompleteUpdate();  // if not done, run async 6.9.4 step 7 substeps
    return;
  }

  AppCacheManifest manifest;
  if (!ParseManifest(manifest_url_, manifest_data_.data(),
                     manifest_data_.length(),
                     manifest_has_valid_mime_type_ ?
                        PARSE_MANIFEST_ALLOWING_INTERCEPTS :
                        PARSE_MANIFEST_PER_STANDARD,
                     manifest)) {
    const char kFormatString[] = "Failed to parse manifest %s";
    const std::string message = base::StringPrintf(kFormatString,
        manifest_url_.spec().c_str());
    HandleCacheFailure(
        AppCacheErrorDetails(
            message, APPCACHE_SIGNATURE_ERROR, GURL(), 0,
            false /*is_cross_origin*/),
        MANIFEST_ERROR,
        GURL());
    VLOG(1) << message;
    return;
  }

  // Proceed with update process. Section 6.9.4 steps 8-20.
  internal_state_ = DOWNLOADING;
  inprogress_cache_ = new AppCache(storage_, storage_->NewCacheId());
  BuildUrlFileList(manifest);
  inprogress_cache_->InitializeWithManifest(&manifest);

  // Associate all pending master hosts with the newly created cache.
  for (PendingMasters::iterator it = pending_master_entries_.begin();
       it != pending_master_entries_.end(); ++it) {
    PendingHosts& hosts = it->second;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      (*host_it)
          ->AssociateIncompleteCache(inprogress_cache_.get(), manifest_url_);
    }
  }

  if (manifest.did_ignore_intercept_namespaces) {
    // Must be done after associating all pending master hosts.
    std::string message(
        "Ignoring the INTERCEPT section of the application cache manifest "
        "because the content type is not text/cache-manifest");
    LogConsoleMessageToAll(message);
  }

  group_->SetUpdateAppCacheStatus(AppCacheGroup::DOWNLOADING);
  NotifyAllAssociatedHosts(APPCACHE_DOWNLOADING_EVENT);
  FetchUrls();
  FetchMasterEntries();
  MaybeCompleteUpdate();  // if not done, continues when async fetches complete
}

void AppCacheUpdateJob::HandleUrlFetchCompleted(URLFetcher* fetcher) {
  DCHECK(internal_state_ == DOWNLOADING);

  net::URLRequest* request = fetcher->request();
  const GURL& url = request->original_url();
  pending_url_fetches_.erase(url);
  NotifyAllProgress(url);
  ++url_fetches_completed_;

  int response_code = request->status().is_success()
                          ? request->GetResponseCode()
                          : fetcher->redirect_response_code();

  AppCacheEntry& entry = url_file_list_.find(url)->second;

  if (response_code / 100 == 2) {
    // Associate storage with the new entry.
    DCHECK(fetcher->response_writer());
    entry.set_response_id(fetcher->response_writer()->response_id());
    entry.set_response_size(fetcher->response_writer()->amount_written());
    if (!inprogress_cache_->AddOrModifyEntry(url, entry))
      duplicate_response_ids_.push_back(entry.response_id());

    // TODO(michaeln): Check for <html manifest=xxx>
    // See http://code.google.com/p/chromium/issues/detail?id=97930
    // if (entry.IsMaster() && !(entry.IsExplicit() || fallback || intercept))
    //   if (!manifestAttribute) skip it

    // Foreign entries will be detected during cache selection.
    // Note: 6.9.4, step 17.9 possible optimization: if resource is HTML or XML
    // file whose root element is an html element with a manifest attribute
    // whose value doesn't match the manifest url of the application cache
    // being processed, mark the entry as being foreign.
  } else {
    VLOG(1) << "Request status: " << request->status().status()
            << " error: " << request->status().error()
            << " response code: " << response_code;
    if (entry.IsExplicit() || entry.IsFallback() || entry.IsIntercept()) {
      if (response_code == 304 && fetcher->existing_entry().has_response_id()) {
        // Keep the existing response.
        entry.set_response_id(fetcher->existing_entry().response_id());
        entry.set_response_size(fetcher->existing_entry().response_size());
        inprogress_cache_->AddOrModifyEntry(url, entry);
      } else {
        const char kFormatString[] = "Resource fetch failed (%d) %s";
        std::string message = FormatUrlErrorMessage(
            kFormatString, url, fetcher->result(), response_code);
        ResultType result = fetcher->result();
        bool is_cross_origin = url.GetOrigin() != manifest_url_.GetOrigin();
        switch (result) {
          case DISKCACHE_ERROR:
            HandleCacheFailure(
                AppCacheErrorDetails(
                    message, APPCACHE_UNKNOWN_ERROR, GURL(), 0,
                    is_cross_origin),
                result,
                url);
            break;
          case NETWORK_ERROR:
            HandleCacheFailure(
                AppCacheErrorDetails(message, APPCACHE_RESOURCE_ERROR, url, 0,
                    is_cross_origin),
                result,
                url);
            break;
          default:
            HandleCacheFailure(AppCacheErrorDetails(message,
                                            APPCACHE_RESOURCE_ERROR,
                                            url,
                                            response_code,
                                            is_cross_origin),
                               result,
                               url);
            break;
        }
        return;
      }
    } else if (response_code == 404 || response_code == 410) {
      // Entry is skipped.  They are dropped from the cache.
    } else if (update_type_ == UPGRADE_ATTEMPT &&
               fetcher->existing_entry().has_response_id()) {
      // Keep the existing response.
      // TODO(michaeln): Not sure this is a good idea. This is spec compliant
      // but the old resource may or may not be compatible with the new contents
      // of the cache. Impossible to know one way or the other.
      entry.set_response_id(fetcher->existing_entry().response_id());
      entry.set_response_size(fetcher->existing_entry().response_size());
      inprogress_cache_->AddOrModifyEntry(url, entry);
    }
  }

  // Fetch another URL now that one request has completed.
  DCHECK(internal_state_ != CACHE_FAILURE);
  FetchUrls();
  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::HandleMasterEntryFetchCompleted(
    URLFetcher* fetcher) {
  DCHECK(internal_state_ == NO_UPDATE || internal_state_ == DOWNLOADING);

  // TODO(jennb): Handle downloads completing during cache failure when update
  // no longer fetches master entries directly. For now, we cancel all pending
  // master entry fetches when entering cache failure state so this will never
  // be called in CACHE_FAILURE state.

  net::URLRequest* request = fetcher->request();
  const GURL& url = request->original_url();
  master_entry_fetches_.erase(url);
  ++master_entries_completed_;

  int response_code = request->status().is_success()
      ? request->GetResponseCode() : -1;

  PendingMasters::iterator found = pending_master_entries_.find(url);
  DCHECK(found != pending_master_entries_.end());
  PendingHosts& hosts = found->second;

  // Section 6.9.4. No update case: step 7.3, else step 22.
  if (response_code / 100 == 2) {
    // Add fetched master entry to the appropriate cache.
    AppCache* cache = inprogress_cache_.get() ? inprogress_cache_.get()
                                              : group_->newest_complete_cache();
    DCHECK(fetcher->response_writer());
    AppCacheEntry master_entry(AppCacheEntry::MASTER,
                               fetcher->response_writer()->response_id(),
                               fetcher->response_writer()->amount_written());
    if (cache->AddOrModifyEntry(url, master_entry))
      added_master_entries_.push_back(url);
    else
      duplicate_response_ids_.push_back(master_entry.response_id());

    // In no-update case, associate host with the newest cache.
    if (!inprogress_cache_.get()) {
      // TODO(michaeln): defer until the updated cache has been stored
      DCHECK(cache == group_->newest_complete_cache());
      for (PendingHosts::iterator host_it = hosts.begin();
           host_it != hosts.end(); ++host_it) {
        (*host_it)->AssociateCompleteCache(cache);
      }
    }
  } else {
    HostNotifier host_notifier;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      AppCacheHost* host = *host_it;
      host_notifier.AddHost(host);

      // In downloading case, disassociate host from inprogress cache.
      if (inprogress_cache_.get())
        host->AssociateNoCache(GURL());

      host->RemoveObserver(this);
    }
    hosts.clear();

    failed_master_entries_.insert(url);

    const char kFormatString[] = "Manifest fetch failed (%d) %s";
    std::string message = FormatUrlErrorMessage(
        kFormatString, request->url(), fetcher->result(), response_code);
    host_notifier.SendErrorNotifications(
        AppCacheErrorDetails(message,
                     APPCACHE_MANIFEST_ERROR,
                     request->url(),
                     response_code,
                     false /*is_cross_origin*/));

    // In downloading case, update result is different if all master entries
    // failed vs. only some failing.
    if (inprogress_cache_.get()) {
      // Only count successful downloads to know if all master entries failed.
      pending_master_entries_.erase(found);
      --master_entries_completed_;

      // Section 6.9.4, step 22.3.
      if (update_type_ == CACHE_ATTEMPT && pending_master_entries_.empty()) {
        HandleCacheFailure(AppCacheErrorDetails(message,
                                        APPCACHE_MANIFEST_ERROR,
                                        request->url(),
                                        response_code,
                                        false /*is_cross_origin*/),
                           fetcher->result(),
                           GURL());
        return;
      }
    }
  }

  DCHECK(internal_state_ != CACHE_FAILURE);
  FetchMasterEntries();
  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::HandleManifestRefetchCompleted(
    URLFetcher* fetcher) {
  DCHECK(internal_state_ == REFETCH_MANIFEST);
  DCHECK(manifest_fetcher_ == fetcher);
  manifest_fetcher_ = NULL;

  net::URLRequest* request = fetcher->request();
  int response_code = request->status().is_success()
      ? request->GetResponseCode() : -1;
  if (response_code == 304 || manifest_data_ == fetcher->manifest_data()) {
    // Only need to store response in storage if manifest is not already
    // an entry in the cache.
    AppCacheEntry* entry = inprogress_cache_->GetEntry(manifest_url_);
    if (entry) {
      entry->add_types(AppCacheEntry::MANIFEST);
      StoreGroupAndCache();
    } else {
      manifest_response_writer_.reset(CreateResponseWriter());
      scoped_refptr<HttpResponseInfoIOBuffer> io_buffer(
          new HttpResponseInfoIOBuffer(manifest_response_info_.release()));
      manifest_response_writer_->WriteInfo(
          io_buffer.get(),
          base::Bind(&AppCacheUpdateJob::OnManifestInfoWriteComplete,
                     base::Unretained(this)));
    }
  } else {
    VLOG(1) << "Request status: " << request->status().status()
            << " error: " << request->status().error()
            << " response code: " << response_code;
    ScheduleUpdateRetry(kRerunDelayMs);
    if (response_code == 200) {
      HandleCacheFailure(AppCacheErrorDetails("Manifest changed during update",
                                      APPCACHE_CHANGED_ERROR,
                                      GURL(),
                                      0,
                                      false /*is_cross_origin*/),
                         MANIFEST_ERROR,
                         GURL());
    } else {
      const char kFormatString[] = "Manifest re-fetch failed (%d) %s";
      std::string message = FormatUrlErrorMessage(
          kFormatString, manifest_url_, fetcher->result(), response_code);
      HandleCacheFailure(AppCacheErrorDetails(message,
                                      APPCACHE_MANIFEST_ERROR,
                                      GURL(),
                                      response_code,
                                      false /*is_cross_origin*/),
                         fetcher->result(),
                         GURL());
    }
  }
}

void AppCacheUpdateJob::OnManifestInfoWriteComplete(int result) {
  if (result > 0) {
    scoped_refptr<net::StringIOBuffer> io_buffer(
        new net::StringIOBuffer(manifest_data_));
    manifest_response_writer_->WriteData(
        io_buffer.get(),
        manifest_data_.length(),
        base::Bind(&AppCacheUpdateJob::OnManifestDataWriteComplete,
                   base::Unretained(this)));
  } else {
    HandleCacheFailure(
        AppCacheErrorDetails("Failed to write the manifest headers to storage",
                     APPCACHE_UNKNOWN_ERROR,
                     GURL(),
                     0,
                     false /*is_cross_origin*/),
        DISKCACHE_ERROR,
        GURL());
  }
}

void AppCacheUpdateJob::OnManifestDataWriteComplete(int result) {
  if (result > 0) {
    AppCacheEntry entry(AppCacheEntry::MANIFEST,
        manifest_response_writer_->response_id(),
        manifest_response_writer_->amount_written());
    if (!inprogress_cache_->AddOrModifyEntry(manifest_url_, entry))
      duplicate_response_ids_.push_back(entry.response_id());
    StoreGroupAndCache();
  } else {
    HandleCacheFailure(
        AppCacheErrorDetails("Failed to write the manifest data to storage",
                     APPCACHE_UNKNOWN_ERROR,
                     GURL(),
                     0,
                     false /*is_cross_origin*/),
        DISKCACHE_ERROR,
        GURL());
  }
}

void AppCacheUpdateJob::StoreGroupAndCache() {
  DCHECK(stored_state_ == UNSTORED);
  stored_state_ = STORING;

  scoped_refptr<AppCache> newest_cache;
  if (inprogress_cache_.get())
    newest_cache.swap(inprogress_cache_);
  else
    newest_cache = group_->newest_complete_cache();
  newest_cache->set_update_time(base::Time::Now());

  group_->set_first_evictable_error_time(base::Time());
  if (doing_full_update_check_)
    group_->set_last_full_update_check_time(base::Time::Now());

  storage_->StoreGroupAndNewestCache(group_, newest_cache.get(), this);
}

void AppCacheUpdateJob::OnGroupAndNewestCacheStored(AppCacheGroup* group,
                                                    AppCache* newest_cache,
                                                    bool success,
                                                    bool would_exceed_quota) {
  DCHECK(stored_state_ == STORING);
  if (success) {
    stored_state_ = STORED;
    MaybeCompleteUpdate();  // will definitely complete
    return;
  }

  stored_state_ = UNSTORED;

  // Restore inprogress_cache_ to get the proper events delivered
  // and the proper cleanup to occur.
  if (newest_cache != group->newest_complete_cache())
    inprogress_cache_ = newest_cache;

  ResultType result = DB_ERROR;
  AppCacheErrorReason reason = APPCACHE_UNKNOWN_ERROR;
  std::string message("Failed to commit new cache to storage");
  if (would_exceed_quota) {
    message.append(", would exceed quota");
    result = QUOTA_ERROR;
    reason = APPCACHE_QUOTA_ERROR;
  }
  HandleCacheFailure(
      AppCacheErrorDetails(message, reason, GURL(), 0,
          false /*is_cross_origin*/),
      result,
      GURL());
}

void AppCacheUpdateJob::NotifySingleHost(AppCacheHost* host,
                                         AppCacheEventID event_id) {
  std::vector<int> ids(1, host->host_id());
  host->frontend()->OnEventRaised(ids, event_id);
}

void AppCacheUpdateJob::NotifyAllAssociatedHosts(AppCacheEventID event_id) {
  HostNotifier host_notifier;
  AddAllAssociatedHostsToNotifier(&host_notifier);
  host_notifier.SendNotifications(event_id);
}

void AppCacheUpdateJob::NotifyAllProgress(const GURL& url) {
  HostNotifier host_notifier;
  AddAllAssociatedHostsToNotifier(&host_notifier);
  host_notifier.SendProgressNotifications(
      url, url_file_list_.size(), url_fetches_completed_);
}

void AppCacheUpdateJob::NotifyAllFinalProgress() {
  DCHECK(url_file_list_.size() == url_fetches_completed_);
  NotifyAllProgress(GURL());
}

void AppCacheUpdateJob::NotifyAllError(const AppCacheErrorDetails& details) {
  HostNotifier host_notifier;
  AddAllAssociatedHostsToNotifier(&host_notifier);
  host_notifier.SendErrorNotifications(details);
}

void AppCacheUpdateJob::LogConsoleMessageToAll(const std::string& message) {
  HostNotifier host_notifier;
  AddAllAssociatedHostsToNotifier(&host_notifier);
  host_notifier.SendLogMessage(message);
}

void AppCacheUpdateJob::AddAllAssociatedHostsToNotifier(
    HostNotifier* host_notifier) {
  // Collect hosts so we only send one notification per frontend.
  // A host can only be associated with a single cache so no need to worry
  // about duplicate hosts being added to the notifier.
  if (inprogress_cache_.get()) {
    DCHECK(internal_state_ == DOWNLOADING || internal_state_ == CACHE_FAILURE);
    host_notifier->AddHosts(inprogress_cache_->associated_hosts());
  }

  AppCacheGroup::Caches old_caches = group_->old_caches();
  for (AppCacheGroup::Caches::const_iterator it = old_caches.begin();
       it != old_caches.end(); ++it) {
    host_notifier->AddHosts((*it)->associated_hosts());
  }

  AppCache* newest_cache = group_->newest_complete_cache();
  if (newest_cache)
    host_notifier->AddHosts(newest_cache->associated_hosts());
}

void AppCacheUpdateJob::OnDestructionImminent(AppCacheHost* host) {
  // The host is about to be deleted; remove from our collection.
  PendingMasters::iterator found =
      pending_master_entries_.find(host->pending_master_entry_url());
  CHECK(found != pending_master_entries_.end());
  PendingHosts& hosts = found->second;
  PendingHosts::iterator it = std::find(hosts.begin(), hosts.end(), host);
  CHECK(it != hosts.end());
  hosts.erase(it);
}

void AppCacheUpdateJob::OnServiceReinitialized(
    AppCacheStorageReference* old_storage_ref) {
  // We continue to use the disabled instance, but arrange for its
  // deletion when its no longer needed.
  if (old_storage_ref->storage() == storage_)
    disabled_storage_reference_ = old_storage_ref;
}

void AppCacheUpdateJob::CheckIfManifestChanged() {
  DCHECK(update_type_ == UPGRADE_ATTEMPT);
  AppCacheEntry* entry = NULL;
  if (group_->newest_complete_cache())
    entry = group_->newest_complete_cache()->GetEntry(manifest_url_);
  if (!entry) {
    // TODO(michaeln): This is just a bandaid to avoid a crash.
    // http://code.google.com/p/chromium/issues/detail?id=95101
    if (service_->storage() == storage_) {
      // Use a local variable because service_ is reset in HandleCacheFailure.
      AppCacheServiceImpl* service = service_;
      HandleCacheFailure(
          AppCacheErrorDetails("Manifest entry not found in existing cache",
                       APPCACHE_UNKNOWN_ERROR,
                       GURL(),
                       0,
                       false /*is_cross_origin*/),
          DB_ERROR,
          GURL());
      AppCacheHistograms::AddMissingManifestEntrySample();
      service->DeleteAppCacheGroup(manifest_url_, net::CompletionCallback());
    }
    return;
  }

  // Load manifest data from storage to compare against fetched manifest.
  manifest_response_reader_.reset(
      storage_->CreateResponseReader(manifest_url_,
                                     entry->response_id()));
  read_manifest_buffer_ = new net::IOBuffer(kBufferSize);
  manifest_response_reader_->ReadData(
      read_manifest_buffer_.get(),
      kBufferSize,
      base::Bind(&AppCacheUpdateJob::OnManifestDataReadComplete,
                 base::Unretained(this)));  // async read
}

void AppCacheUpdateJob::OnManifestDataReadComplete(int result) {
  if (result > 0) {
    loaded_manifest_data_.append(read_manifest_buffer_->data(), result);
    manifest_response_reader_->ReadData(
        read_manifest_buffer_.get(),
        kBufferSize,
        base::Bind(&AppCacheUpdateJob::OnManifestDataReadComplete,
                   base::Unretained(this)));  // read more
  } else {
    read_manifest_buffer_ = NULL;
    manifest_response_reader_.reset();
    ContinueHandleManifestFetchCompleted(
        result < 0 || manifest_data_ != loaded_manifest_data_);
  }
}

void AppCacheUpdateJob::BuildUrlFileList(const AppCacheManifest& manifest) {
  for (base::hash_set<std::string>::const_iterator it =
           manifest.explicit_urls.begin();
       it != manifest.explicit_urls.end(); ++it) {
    AddUrlToFileList(GURL(*it), AppCacheEntry::EXPLICIT);
  }

  const std::vector<AppCacheNamespace>& intercepts =
      manifest.intercept_namespaces;
  for (std::vector<AppCacheNamespace>::const_iterator it = intercepts.begin();
       it != intercepts.end(); ++it) {
    int flags = AppCacheEntry::INTERCEPT;
    if (it->is_executable)
      flags |= AppCacheEntry::EXECUTABLE;
    AddUrlToFileList(it->target_url, flags);
  }

  const std::vector<AppCacheNamespace>& fallbacks =
      manifest.fallback_namespaces;
  for (std::vector<AppCacheNamespace>::const_iterator it = fallbacks.begin();
       it != fallbacks.end(); ++it) {
     AddUrlToFileList(it->target_url, AppCacheEntry::FALLBACK);
  }

  // Add all master entries from newest complete cache.
  if (update_type_ == UPGRADE_ATTEMPT) {
    const AppCache::EntryMap& entries =
        group_->newest_complete_cache()->entries();
    for (AppCache::EntryMap::const_iterator it = entries.begin();
         it != entries.end(); ++it) {
      const AppCacheEntry& entry = it->second;
      if (entry.IsMaster())
        AddUrlToFileList(it->first, AppCacheEntry::MASTER);
    }
  }
}

void AppCacheUpdateJob::AddUrlToFileList(const GURL& url, int type) {
  std::pair<AppCache::EntryMap::iterator, bool> ret = url_file_list_.insert(
      AppCache::EntryMap::value_type(url, AppCacheEntry(type)));

  if (ret.second)
    urls_to_fetch_.push_back(UrlToFetch(url, false, NULL));
  else
    ret.first->second.add_types(type);  // URL already exists. Merge types.
}

void AppCacheUpdateJob::FetchUrls() {
  DCHECK(internal_state_ == DOWNLOADING);

  // Fetch each URL in the list according to section 6.9.4 step 17.1-17.3.
  // Fetch up to the concurrent limit. Other fetches will be triggered as each
  // each fetch completes.
  while (pending_url_fetches_.size() < kMaxConcurrentUrlFetches &&
         !urls_to_fetch_.empty()) {
    UrlToFetch url_to_fetch = urls_to_fetch_.front();
    urls_to_fetch_.pop_front();

    AppCache::EntryMap::iterator it = url_file_list_.find(url_to_fetch.url);
    DCHECK(it != url_file_list_.end());
    AppCacheEntry& entry = it->second;
    if (ShouldSkipUrlFetch(entry)) {
      NotifyAllProgress(url_to_fetch.url);
      ++url_fetches_completed_;
    } else if (AlreadyFetchedEntry(url_to_fetch.url, entry.types())) {
      NotifyAllProgress(url_to_fetch.url);
      ++url_fetches_completed_;  // saved a URL request
    } else if (!url_to_fetch.storage_checked &&
               MaybeLoadFromNewestCache(url_to_fetch.url, entry)) {
      // Continues asynchronously after data is loaded from newest cache.
    } else {
      URLFetcher* fetcher = new URLFetcher(
          url_to_fetch.url, URLFetcher::URL_FETCH, this);
      if (url_to_fetch.existing_response_info.get() &&
          group_->newest_complete_cache()) {
        AppCacheEntry* existing_entry =
            group_->newest_complete_cache()->GetEntry(url_to_fetch.url);
        DCHECK(existing_entry);
        DCHECK(existing_entry->response_id() ==
               url_to_fetch.existing_response_info->response_id());
        fetcher->set_existing_response_headers(
            url_to_fetch.existing_response_info->http_response_info()->headers
                .get());
        fetcher->set_existing_entry(*existing_entry);
      }
      fetcher->Start();
      pending_url_fetches_.insert(
          PendingUrlFetches::value_type(url_to_fetch.url, fetcher));
    }
  }
}

void AppCacheUpdateJob::CancelAllUrlFetches() {
  // Cancel any pending URL requests.
  for (PendingUrlFetches::iterator it = pending_url_fetches_.begin();
       it != pending_url_fetches_.end(); ++it) {
    delete it->second;
  }

  url_fetches_completed_ +=
      pending_url_fetches_.size() + urls_to_fetch_.size();
  pending_url_fetches_.clear();
  urls_to_fetch_.clear();
}

bool AppCacheUpdateJob::ShouldSkipUrlFetch(const AppCacheEntry& entry) {
  // 6.6.4 Step 17
  // If the resource URL being processed was flagged as neither an
  // "explicit entry" nor or a "fallback entry", then the user agent
  // may skip this URL.
  if (entry.IsExplicit() || entry.IsFallback() || entry.IsIntercept())
    return false;

  // TODO(jennb): decide if entry should be skipped to expire it from cache
  return false;
}

bool AppCacheUpdateJob::AlreadyFetchedEntry(const GURL& url,
                                            int entry_type) {
  DCHECK(internal_state_ == DOWNLOADING || internal_state_ == NO_UPDATE);
  AppCacheEntry* existing =
      inprogress_cache_.get() ? inprogress_cache_->GetEntry(url)
                              : group_->newest_complete_cache()->GetEntry(url);
  if (existing) {
    existing->add_types(entry_type);
    return true;
  }
  return false;
}

void AppCacheUpdateJob::AddMasterEntryToFetchList(AppCacheHost* host,
                                                  const GURL& url,
                                                  bool is_new) {
  DCHECK(!IsTerminating());

  if (internal_state_ == DOWNLOADING || internal_state_ == NO_UPDATE) {
    AppCache* cache;
    if (inprogress_cache_.get()) {
      // always associate
      host->AssociateIncompleteCache(inprogress_cache_.get(), manifest_url_);
      cache = inprogress_cache_.get();
    } else {
      cache = group_->newest_complete_cache();
    }

    // Update existing entry if it has already been fetched.
    AppCacheEntry* entry = cache->GetEntry(url);
    if (entry) {
      entry->add_types(AppCacheEntry::MASTER);
      if (internal_state_ == NO_UPDATE && !inprogress_cache_.get()) {
        // only associate if have entry
        host->AssociateCompleteCache(cache);
      }
      if (is_new)
        ++master_entries_completed_;  // pretend fetching completed
      return;
    }
  }

  // Add to fetch list if not already fetching.
  if (master_entry_fetches_.find(url) == master_entry_fetches_.end()) {
    master_entries_to_fetch_.insert(url);
    if (internal_state_ == DOWNLOADING || internal_state_ == NO_UPDATE)
      FetchMasterEntries();
  }
}

void AppCacheUpdateJob::FetchMasterEntries() {
  DCHECK(internal_state_ == NO_UPDATE || internal_state_ == DOWNLOADING);

  // Fetch each master entry in the list, up to the concurrent limit.
  // Additional fetches will be triggered as each fetch completes.
  while (master_entry_fetches_.size() < kMaxConcurrentUrlFetches &&
         !master_entries_to_fetch_.empty()) {
    const GURL& url = *master_entries_to_fetch_.begin();

    if (AlreadyFetchedEntry(url, AppCacheEntry::MASTER)) {
      ++master_entries_completed_;  // saved a URL request

      // In no update case, associate hosts to newest cache in group
      // now that master entry has been "successfully downloaded".
      if (internal_state_ == NO_UPDATE) {
        // TODO(michaeln): defer until the updated cache has been stored.
        DCHECK(!inprogress_cache_.get());
        AppCache* cache = group_->newest_complete_cache();
        PendingMasters::iterator found = pending_master_entries_.find(url);
        DCHECK(found != pending_master_entries_.end());
        PendingHosts& hosts = found->second;
        for (PendingHosts::iterator host_it = hosts.begin();
             host_it != hosts.end(); ++host_it) {
          (*host_it)->AssociateCompleteCache(cache);
        }
      }
    } else {
      URLFetcher* fetcher = new URLFetcher(
          url, URLFetcher::MASTER_ENTRY_FETCH, this);
      fetcher->Start();
      master_entry_fetches_.insert(PendingUrlFetches::value_type(url, fetcher));
    }

    master_entries_to_fetch_.erase(master_entries_to_fetch_.begin());
  }
}

void AppCacheUpdateJob::CancelAllMasterEntryFetches(
    const AppCacheErrorDetails& error_details) {
  // For now, cancel all in-progress fetches for master entries and pretend
  // all master entries fetches have completed.
  // TODO(jennb): Delete this when update no longer fetches master entries
  // directly.

  // Cancel all in-progress fetches.
  for (PendingUrlFetches::iterator it = master_entry_fetches_.begin();
       it != master_entry_fetches_.end(); ++it) {
    delete it->second;
    master_entries_to_fetch_.insert(it->first);  // back in unfetched list
  }
  master_entry_fetches_.clear();

  master_entries_completed_ += master_entries_to_fetch_.size();

  // Cache failure steps, step 2.
  // Pretend all master entries that have not yet been fetched have completed
  // downloading. Unassociate hosts from any appcache and send ERROR event.
  HostNotifier host_notifier;
  while (!master_entries_to_fetch_.empty()) {
    const GURL& url = *master_entries_to_fetch_.begin();
    PendingMasters::iterator found = pending_master_entries_.find(url);
    DCHECK(found != pending_master_entries_.end());
    PendingHosts& hosts = found->second;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      AppCacheHost* host = *host_it;
      host->AssociateNoCache(GURL());
      host_notifier.AddHost(host);
      host->RemoveObserver(this);
    }
    hosts.clear();

    master_entries_to_fetch_.erase(master_entries_to_fetch_.begin());
  }
  host_notifier.SendErrorNotifications(error_details);
}

bool AppCacheUpdateJob::MaybeLoadFromNewestCache(const GURL& url,
                                                 AppCacheEntry& entry) {
  if (update_type_ != UPGRADE_ATTEMPT)
    return false;

  AppCache* newest = group_->newest_complete_cache();
  AppCacheEntry* copy_me = newest->GetEntry(url);
  if (!copy_me || !copy_me->has_response_id())
    return false;

  // Load HTTP headers for entry from newest cache.
  loading_responses_.insert(
      LoadingResponses::value_type(copy_me->response_id(), url));
  storage_->LoadResponseInfo(manifest_url_, copy_me->response_id(), this);
  // Async: wait for OnResponseInfoLoaded to complete.
  return true;
}

void AppCacheUpdateJob::OnResponseInfoLoaded(
    AppCacheResponseInfo* response_info,
    int64_t response_id) {
  const net::HttpResponseInfo* http_info = response_info ?
      response_info->http_response_info() : NULL;

  // Needed response info for a manifest fetch request.
  if (internal_state_ == FETCH_MANIFEST) {
    if (http_info)
      manifest_fetcher_->set_existing_response_headers(
          http_info->headers.get());
    manifest_fetcher_->Start();
    return;
  }

  LoadingResponses::iterator found = loading_responses_.find(response_id);
  DCHECK(found != loading_responses_.end());
  const GURL& url = found->second;

  if (!http_info) {
    LoadFromNewestCacheFailed(url, NULL);  // no response found
  } else {
    // Check if response can be re-used according to HTTP caching semantics.
    // Responses with a "vary" header get treated as expired.
    const std::string name = "vary";
    std::string value;
    size_t iter = 0;
    if (!http_info->headers.get() ||
        http_info->headers->RequiresValidation(http_info->request_time,
                                               http_info->response_time,
                                               base::Time::Now()) ||
        http_info->headers->EnumerateHeader(&iter, name, &value)) {
      LoadFromNewestCacheFailed(url, response_info);
    } else {
      DCHECK(group_->newest_complete_cache());
      AppCacheEntry* copy_me = group_->newest_complete_cache()->GetEntry(url);
      DCHECK(copy_me);
      DCHECK(copy_me->response_id() == response_id);

      AppCache::EntryMap::iterator it = url_file_list_.find(url);
      DCHECK(it != url_file_list_.end());
      AppCacheEntry& entry = it->second;
      entry.set_response_id(response_id);
      entry.set_response_size(copy_me->response_size());
      inprogress_cache_->AddOrModifyEntry(url, entry);
      NotifyAllProgress(url);
      ++url_fetches_completed_;
    }
  }
  loading_responses_.erase(found);

  MaybeCompleteUpdate();
}

void AppCacheUpdateJob::LoadFromNewestCacheFailed(
    const GURL& url, AppCacheResponseInfo* response_info) {
  if (internal_state_ == CACHE_FAILURE)
    return;

  // Re-insert url at front of fetch list. Indicate storage has been checked.
  urls_to_fetch_.push_front(UrlToFetch(url, true, response_info));
  FetchUrls();
}

void AppCacheUpdateJob::MaybeCompleteUpdate() {
  DCHECK(internal_state_ != CACHE_FAILURE);

  // Must wait for any pending master entries or url fetches to complete.
  if (master_entries_completed_ != pending_master_entries_.size() ||
      url_fetches_completed_ != url_file_list_.size()) {
    DCHECK(internal_state_ != COMPLETED);
    return;
  }

  switch (internal_state_) {
    case NO_UPDATE:
      if (master_entries_completed_ > 0) {
        switch (stored_state_) {
          case UNSTORED:
            StoreGroupAndCache();
            return;
          case STORING:
            return;
          case STORED:
            break;
        }
      } else {
        bool times_changed = false;
        if (!group_->first_evictable_error_time().is_null()) {
          group_->set_first_evictable_error_time(base::Time());
          times_changed = true;
        }
        if (doing_full_update_check_) {
          group_->set_last_full_update_check_time(base::Time::Now());
          times_changed = true;
        }
        if (times_changed)
          storage_->StoreEvictionTimes(group_);
      }
      // 6.9.4 steps 7.3-7.7.
      NotifyAllAssociatedHosts(APPCACHE_NO_UPDATE_EVENT);
      DiscardDuplicateResponses();
      internal_state_ = COMPLETED;
      break;
    case DOWNLOADING:
      internal_state_ = REFETCH_MANIFEST;
      FetchManifest(false);
      break;
    case REFETCH_MANIFEST:
      DCHECK(stored_state_ == STORED);
      NotifyAllFinalProgress();
      if (update_type_ == CACHE_ATTEMPT)
        NotifyAllAssociatedHosts(APPCACHE_CACHED_EVENT);
      else
        NotifyAllAssociatedHosts(APPCACHE_UPDATE_READY_EVENT);
      DiscardDuplicateResponses();
      internal_state_ = COMPLETED;
      LogHistogramStats(UPDATE_OK, GURL());
      break;
    case CACHE_FAILURE:
      NOTREACHED();  // See HandleCacheFailure
      break;
    default:
      break;
  }

  // Let the stack unwind before deletion to make it less risky as this
  // method is called from multiple places in this file.
  if (internal_state_ == COMPLETED)
    DeleteSoon();
}

void AppCacheUpdateJob::ScheduleUpdateRetry(int delay_ms) {
  // TODO(jennb): post a delayed task with the "same parameters" as this job
  // to retry the update at a later time. Need group, URLs of pending master
  // entries and their hosts.
}

void AppCacheUpdateJob::Cancel() {
  internal_state_ = CANCELLED;

  LogHistogramStats(CANCELLED_ERROR, GURL());

  if (manifest_fetcher_) {
    delete manifest_fetcher_;
    manifest_fetcher_ = NULL;
  }

  for (PendingUrlFetches::iterator it = pending_url_fetches_.begin();
       it != pending_url_fetches_.end(); ++it) {
    delete it->second;
  }
  pending_url_fetches_.clear();

  for (PendingUrlFetches::iterator it = master_entry_fetches_.begin();
       it != master_entry_fetches_.end(); ++it) {
    delete it->second;
  }
  master_entry_fetches_.clear();

  ClearPendingMasterEntries();
  DiscardInprogressCache();

  // Delete response writer to avoid any callbacks.
  if (manifest_response_writer_)
    manifest_response_writer_.reset();

  storage_->CancelDelegateCallbacks(this);
}

void AppCacheUpdateJob::ClearPendingMasterEntries() {
  for (PendingMasters::iterator it = pending_master_entries_.begin();
       it != pending_master_entries_.end(); ++it) {
    PendingHosts& hosts = it->second;
    for (PendingHosts::iterator host_it = hosts.begin();
         host_it != hosts.end(); ++host_it) {
      (*host_it)->RemoveObserver(this);
    }
  }

  pending_master_entries_.clear();
}

void AppCacheUpdateJob::DiscardInprogressCache() {
  if (stored_state_ == STORING) {
    // We can make no assumptions about whether the StoreGroupAndCacheTask
    // actually completed or not. This condition should only be reachable
    // during shutdown. Free things up and return to do no harm.
    inprogress_cache_ = NULL;
    added_master_entries_.clear();
    return;
  }

  storage_->DoomResponses(manifest_url_, stored_response_ids_);

  if (!inprogress_cache_.get()) {
    // We have to undo the changes we made, if any, to the existing cache.
    if (group_ && group_->newest_complete_cache()) {
      for (std::vector<GURL>::iterator iter = added_master_entries_.begin();
           iter != added_master_entries_.end(); ++iter) {
        group_->newest_complete_cache()->RemoveEntry(*iter);
      }
    }
    added_master_entries_.clear();
    return;
  }

  AppCache::AppCacheHosts& hosts = inprogress_cache_->associated_hosts();
  while (!hosts.empty())
    (*hosts.begin())->AssociateNoCache(GURL());

  inprogress_cache_ = NULL;
  added_master_entries_.clear();
}

void AppCacheUpdateJob::DiscardDuplicateResponses() {
  storage_->DoomResponses(manifest_url_, duplicate_response_ids_);
}

void AppCacheUpdateJob::LogHistogramStats(
      ResultType result, const GURL& failed_resource_url) {
  AppCacheHistograms::CountUpdateJobResult(result, manifest_url_.GetOrigin());
  if (result == UPDATE_OK)
    return;

  int percent_complete = 0;
  if (url_file_list_.size() > 0) {
    size_t actual_fetches_completed = url_fetches_completed_;
    if (!failed_resource_url.is_empty() && actual_fetches_completed)
      --actual_fetches_completed;
    percent_complete = (static_cast<double>(actual_fetches_completed) /
                            static_cast<double>(url_file_list_.size())) * 100.0;
    percent_complete = std::min(percent_complete, 99);
  }

  bool was_making_progress =
      base::Time::Now() - last_progress_time_ <
          base::TimeDelta::FromMinutes(5);

  bool off_origin_resource_failure =
      !failed_resource_url.is_empty() &&
          (failed_resource_url.GetOrigin() != manifest_url_.GetOrigin());

  AppCacheHistograms::LogUpdateFailureStats(
      manifest_url_.GetOrigin(),
      percent_complete,
      was_making_progress,
      off_origin_resource_failure);
}

void AppCacheUpdateJob::DeleteSoon() {
  ClearPendingMasterEntries();
  manifest_response_writer_.reset();
  storage_->CancelDelegateCallbacks(this);
  service_->RemoveObserver(this);
  service_ = NULL;

  // Break the connection with the group so the group cannot call delete
  // on this object after we've posted a task to delete ourselves.
  if (group_) {
    group_->SetUpdateAppCacheStatus(AppCacheGroup::IDLE);
    group_ = NULL;
  }

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

}  // namespace content
