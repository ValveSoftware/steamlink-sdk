// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_JOB_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_JOB_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_response.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "content/browser/appcache/appcache_storage.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/content_export.h"
#include "net/base/completion_callback.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace content {
FORWARD_DECLARE_TEST(AppCacheGroupTest, QueueUpdate);
class AppCacheGroupTest;
class AppCacheUpdateJobTest;
class HostNotifier;

// Application cache Update algorithm and state.
class CONTENT_EXPORT AppCacheUpdateJob
    : public AppCacheStorage::Delegate,
      public AppCacheHost::Observer,
      public AppCacheServiceImpl::Observer {
 public:
  // Used for uma stats only for now, so new values are append only.
  enum ResultType {
    UPDATE_OK, DB_ERROR, DISKCACHE_ERROR, QUOTA_ERROR, REDIRECT_ERROR,
    MANIFEST_ERROR, NETWORK_ERROR, SERVER_ERROR, CANCELLED_ERROR,
    SECURITY_ERROR, NUM_UPDATE_JOB_RESULT_TYPES
  };

  AppCacheUpdateJob(AppCacheServiceImpl* service, AppCacheGroup* group);
  ~AppCacheUpdateJob() override;

  // Triggers the update process or adds more info if this update is already
  // in progress.
  void StartUpdate(AppCacheHost* host, const GURL& new_master_resource);

 private:
  friend class content::AppCacheGroupTest;
  friend class content::AppCacheUpdateJobTest;
  class URLFetcher;

  // Master entries have multiple hosts, for example, the same page is opened
  // in different tabs.
  typedef std::vector<AppCacheHost*> PendingHosts;
  typedef std::map<GURL, PendingHosts> PendingMasters;
  typedef std::map<GURL, URLFetcher*> PendingUrlFetches;
  typedef std::map<int64_t, GURL> LoadingResponses;

  static const int kRerunDelayMs = 1000;

  // TODO(michaeln): Rework the set of states vs update types vs stored states.
  // The NO_UPDATE state is really more of an update type. For all update types
  // storing the results is relevant.

  enum UpdateType {
    UNKNOWN_TYPE,
    UPGRADE_ATTEMPT,
    CACHE_ATTEMPT,
  };

  enum InternalUpdateState {
    FETCH_MANIFEST,
    NO_UPDATE,
    DOWNLOADING,

    // Every state after this comment indicates the update is terminating.
    REFETCH_MANIFEST,
    CACHE_FAILURE,
    CANCELLED,
    COMPLETED,
  };

  enum StoredState {
    UNSTORED,
    STORING,
    STORED,
  };

  struct UrlToFetch {
    UrlToFetch(const GURL& url, bool checked, AppCacheResponseInfo* info);
    UrlToFetch(const UrlToFetch& other);
    ~UrlToFetch();

    GURL url;
    bool storage_checked;
    scoped_refptr<AppCacheResponseInfo> existing_response_info;
  };

  class URLFetcher : public net::URLRequest::Delegate {
   public:
    enum FetchType {
      MANIFEST_FETCH,
      URL_FETCH,
      MASTER_ENTRY_FETCH,
      MANIFEST_REFETCH,
    };
    URLFetcher(const GURL& url,
               FetchType fetch_type,
               AppCacheUpdateJob* job);
    ~URLFetcher() override;
    void Start();
    FetchType fetch_type() const { return fetch_type_; }
    net::URLRequest* request() const { return request_.get(); }
    const AppCacheEntry& existing_entry() const { return existing_entry_; }
    const std::string& manifest_data() const { return manifest_data_; }
    AppCacheResponseWriter* response_writer() const {
      return response_writer_.get();
    }
    void set_existing_response_headers(net::HttpResponseHeaders* headers) {
      existing_response_headers_ = headers;
    }
    void set_existing_entry(const AppCacheEntry& entry) {
      existing_entry_ = entry;
    }
    ResultType result() const { return result_; }
    int redirect_response_code() const { return redirect_response_code_; }

   private:
    // URLRequest::Delegate overrides
    void OnReceivedRedirect(net::URLRequest* request,
                            const net::RedirectInfo& redirect_info,
                            bool* defer_redirect) override;
    void OnResponseStarted(net::URLRequest* request, int net_error) override;
    void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

    void AddConditionalHeaders(const net::HttpResponseHeaders* headers);
    void OnWriteComplete(int result);
    void ReadResponseData();
    bool ConsumeResponseData(int bytes_read);
    void OnResponseCompleted(int net_error);
    bool MaybeRetryRequest();

    GURL url_;
    AppCacheUpdateJob* job_;
    FetchType fetch_type_;
    int retry_503_attempts_;
    scoped_refptr<net::IOBuffer> buffer_;
    std::unique_ptr<net::URLRequest> request_;
    AppCacheEntry existing_entry_;
    scoped_refptr<net::HttpResponseHeaders> existing_response_headers_;
    std::string manifest_data_;
    ResultType result_;
    int redirect_response_code_;
    std::unique_ptr<AppCacheResponseWriter> response_writer_;
  };  // class URLFetcher

  AppCacheResponseWriter* CreateResponseWriter();

  // Methods for AppCacheStorage::Delegate.
  void OnResponseInfoLoaded(AppCacheResponseInfo* response_info,
                            int64_t response_id) override;
  void OnGroupAndNewestCacheStored(AppCacheGroup* group,
                                   AppCache* newest_cache,
                                   bool success,
                                   bool would_exceed_quota) override;
  void OnGroupMadeObsolete(AppCacheGroup* group,
                           bool success,
                           int response_code) override;

  // Methods for AppCacheHost::Observer.
  void OnCacheSelectionComplete(AppCacheHost* host) override {}  // N/A
  void OnDestructionImminent(AppCacheHost* host) override;

  // Methods for AppCacheServiceImpl::Observer.
  void OnServiceReinitialized(AppCacheStorageReference* old_storage) override;

  void HandleCacheFailure(const AppCacheErrorDetails& details,
                          ResultType result,
                          const GURL& failed_resource_url);

  void FetchManifest(bool is_first_fetch);
  void HandleManifestFetchCompleted(URLFetcher* fetcher, int net_error);
  void ContinueHandleManifestFetchCompleted(bool changed);

  void HandleUrlFetchCompleted(URLFetcher* fetcher, int net_error);
  void HandleMasterEntryFetchCompleted(URLFetcher* fetcher, int net_error);

  void HandleManifestRefetchCompleted(URLFetcher* fetcher, int net_error);
  void OnManifestInfoWriteComplete(int result);
  void OnManifestDataWriteComplete(int result);

  void StoreGroupAndCache();

  void NotifySingleHost(AppCacheHost* host, AppCacheEventID event_id);
  void NotifyAllAssociatedHosts(AppCacheEventID event_id);
  void NotifyAllProgress(const GURL& url);
  void NotifyAllFinalProgress();
  void NotifyAllError(const AppCacheErrorDetails& detals);
  void LogConsoleMessageToAll(const std::string& message);
  void AddAllAssociatedHostsToNotifier(HostNotifier* notifier);

  // Checks if manifest is byte for byte identical with the manifest
  // in the newest application cache.
  void CheckIfManifestChanged();
  void OnManifestDataReadComplete(int result);

  // Creates the list of files that may need to be fetched and initiates
  // fetches. Section 6.9.4 steps 12-17
  void BuildUrlFileList(const AppCacheManifest& manifest);
  void AddUrlToFileList(const GURL& url, int type);
  void FetchUrls();
  void CancelAllUrlFetches();
  bool ShouldSkipUrlFetch(const AppCacheEntry& entry);

  // If entry already exists in the cache currently being updated, merge
  // the entry type information with the existing entry.
  // Returns true if entry exists in cache currently being updated.
  bool AlreadyFetchedEntry(const GURL& url, int entry_type);

  // TODO(jennb): Delete when update no longer fetches master entries directly.
  // Creates the list of master entries that need to be fetched and initiates
  // fetches.
  void AddMasterEntryToFetchList(AppCacheHost* host, const GURL& url,
                                 bool is_new);
  void FetchMasterEntries();
  void CancelAllMasterEntryFetches(const AppCacheErrorDetails& details);

  // Asynchronously loads the entry from the newest complete cache if the
  // HTTP caching semantics allow.
  // Returns false if immediately obvious that data cannot be loaded from
  // newest complete cache.
  bool MaybeLoadFromNewestCache(const GURL& url, AppCacheEntry& entry);
  void LoadFromNewestCacheFailed(const GURL& url,
                                 AppCacheResponseInfo* newest_response_info);

  // Does nothing if update process is still waiting for pending master
  // entries or URL fetches to complete downloading. Otherwise, completes
  // the update process.
  void MaybeCompleteUpdate();

  // Schedules a rerun of the entire update with the same parameters as
  // this update job after a short delay.
  void ScheduleUpdateRetry(int delay_ms);

  void Cancel();
  void ClearPendingMasterEntries();
  void DiscardInprogressCache();
  void DiscardDuplicateResponses();

  void LogHistogramStats(ResultType result, const GURL& failed_resource_url);
  void MadeProgress() { last_progress_time_ = base::Time::Now(); }

  // Deletes this object after letting the stack unwind.
  void DeleteSoon();

  bool IsTerminating() { return internal_state_ >= REFETCH_MANIFEST ||
                                stored_state_ != UNSTORED; }

  AppCacheServiceImpl* service_;
  const GURL manifest_url_;  // here for easier access

  // Defined prior to refs to AppCaches and Groups because destruction
  // order matters, the disabled_storage_reference_ must outlive those
  // objects.
  scoped_refptr<AppCacheStorageReference> disabled_storage_reference_;

  scoped_refptr<AppCache> inprogress_cache_;

  AppCacheGroup* group_;

  UpdateType update_type_;
  InternalUpdateState internal_state_;
  base::Time last_progress_time_;
  bool doing_full_update_check_;

  PendingMasters pending_master_entries_;
  size_t master_entries_completed_;
  std::set<GURL> failed_master_entries_;

  // TODO(jennb): Delete when update no longer fetches master entries directly.
  // Helper containers to track which pending master entries have yet to be
  // fetched and which are currently being fetched. Master entries that
  // are listed in the manifest may be fetched as a regular URL instead of
  // as a separate master entry fetch to optimize against duplicate fetches.
  std::set<GURL> master_entries_to_fetch_;
  PendingUrlFetches master_entry_fetches_;

  // URLs of files to fetch along with their flags.
  AppCache::EntryMap url_file_list_;
  size_t url_fetches_completed_;

  // Helper container to track which urls have not been fetched yet. URLs are
  // removed when the fetch is initiated. Flag indicates whether an attempt
  // to load the URL from storage has already been tried and failed.
  std::deque<UrlToFetch> urls_to_fetch_;

  // Helper container to track which urls are being loaded from response
  // storage.
  LoadingResponses loading_responses_;

  // Keep track of pending URL requests so we can cancel them if necessary.
  URLFetcher* manifest_fetcher_;
  PendingUrlFetches pending_url_fetches_;

  // Temporary storage of manifest response data for parsing and comparison.
  std::string manifest_data_;
  std::unique_ptr<net::HttpResponseInfo> manifest_response_info_;
  std::unique_ptr<AppCacheResponseWriter> manifest_response_writer_;
  scoped_refptr<net::IOBuffer> read_manifest_buffer_;
  std::string loaded_manifest_data_;
  std::unique_ptr<AppCacheResponseReader> manifest_response_reader_;
  bool manifest_has_valid_mime_type_;

  // New master entries added to the cache by this job, used to cleanup
  // in error conditions.
  std::vector<GURL> added_master_entries_;

  // Response ids stored by this update job, used to cleanup in
  // error conditions.
  std::vector<int64_t> stored_response_ids_;

  // In some cases we fetch the same resource multiple times, and then
  // have to delete the duplicates upon successful update. These ids
  // are also in the stored_response_ids_ collection so we only schedule
  // these for deletion on success.
  // TODO(michaeln): Rework when we no longer fetches master entries directly.
  std::vector<int64_t> duplicate_response_ids_;

  // Whether we've stored the resulting group/cache yet.
  StoredState stored_state_;

  AppCacheStorage* storage_;
  base::WeakPtrFactory<AppCacheUpdateJob> weak_factory_;

  FRIEND_TEST_ALL_PREFIXES(content::AppCacheGroupTest, QueueUpdate);

  DISALLOW_COPY_AND_ASSIGN(AppCacheUpdateJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_UPDATE_JOB_H_
