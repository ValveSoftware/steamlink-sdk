// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRECACHE_CORE_PRECACHE_FETCHER_H_
#define COMPONENTS_PRECACHE_CORE_PRECACHE_FETCHER_H_

#include <stdint.h>

#include <deque>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/precache/core/fetcher_pool.h"
#include "components/precache/core/proto/quota.pb.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace precache {

class PrecacheConfigurationSettings;
class PrecacheDatabase;
class PrecacheUnfinishedWork;

// Visible for testing.
extern const int kNoTracking;
extern const int kMaxParallelFetches;

// Information about the manifest for a host.
struct ManifestHostInfo {
  ManifestHostInfo(int64_t manifest_id,
                   const std::string& hostname,
                   int64_t visits,
                   const std::string& used_url_hash,
                   const std::string& downloaded_url_hash);
  ~ManifestHostInfo();
  ManifestHostInfo(ManifestHostInfo&&);
  ManifestHostInfo& operator=(ManifestHostInfo&&);
  // Copy constructor and assignment operator are implicitly deleted.

  int64_t manifest_id;
  std::string hostname;
  GURL manifest_url;
  int64_t visits;
  std::string used_url_hash;
  std::string downloaded_url_hash;
};

// Information about a resource to be downloaded.
struct ResourceInfo {
  ResourceInfo(const GURL& url, const std::string& referrer, double weight);
  ~ResourceInfo();
  ResourceInfo(ResourceInfo&&);
  ResourceInfo& operator=(ResourceInfo&&);
  // Copy constructor and assignment operator are implicitly deleted.

  GURL url;              // The resource being requested.
  std::string referrer;  // The host of the manifest requesting this resource.
  double weight;         // Estimate of the expected utility of this resource.
};

// Public interface to code that fetches resources that the user is likely to
// want to fetch in the future, putting them in the network stack disk cache.
// Precaching is intended to be done when Chrome is not actively in use, likely
// hours ahead of the time when the resources are actually needed.
//
// This class takes as input a prioritized list of URL domains that the user
// commonly visits, referred to as starting hosts. This class interacts with a
// server, sending it the list of starting hosts sequentially. For each starting
// host, the server returns a manifest of resource URLs that are good candidates
// for precaching. Every resource returned is fetched, and responses are cached
// as they are received. Destroying the PrecacheFetcher while it is precaching
// will cancel any fetch in progress and cancel precaching.
//
// The URLs of the server-side component must be specified in order for the
// PrecacheFetcher to work. This includes the URL that the precache
// configuration settings are fetched from and the prefix of URLs where precache
// manifests are fetched from. These can be set by using command line switches
// or by providing default values.
//
// Sample interaction:
//
// class MyPrecacheFetcherDelegate : public PrecacheFetcher::PrecacheDelegate {
//  public:
//   void PrecacheResourcesForTopURLs(
//       net::URLRequestContextGetter* request_context,
//       const std::list<GURL>& top_urls) {
//     fetcher_.reset(new PrecacheFetcher(...));
//     fetcher_->Start();
//   }
//
//   void Cancel() {
//     std::unique_ptr<PrecacheUnfinishedWork> unfinished_work =
//         fetcher_->CancelPrecaching();
//     fetcher_.reset();
//   }
//
//   virtual void OnDone() {
//     // Do something when precaching is done.
//   }
//
//  private:
//   std::unique_ptr<PrecacheFetcher> fetcher_;
// };
class PrecacheFetcher : public base::SupportsWeakPtr<PrecacheFetcher> {
 public:
  class PrecacheDelegate {
   public:
    // Called when the fetching of resources has finished, whether the resources
    // were fetched or not. If the PrecacheFetcher is destroyed before OnDone is
    // called, then precaching will be canceled and OnDone will not be called.
    virtual void OnDone() = 0;
  };

  // Visible for testing.
  class Fetcher;

  static void RecordCompletionStatistics(
      const PrecacheUnfinishedWork& unfinished_work,
      size_t remaining_manifest_urls_to_fetch,
      size_t remaining_resource_urls_to_fetch);

  static std::string GetResourceURLBase64HashForTesting(
      const std::vector<GURL>& urls);

  // Constructs a new PrecacheFetcher. The |unfinished_work| contains the
  // prioritized list of hosts that the user commonly visits. These hosts are
  // used by a server side component to construct a list of resource URLs that
  // the user is likely to fetch. Takes ownership of |unfinished_work|.
  // |precache_database| should be accessed only in |db_task_runner|.
  PrecacheFetcher(
      net::URLRequestContextGetter* request_context,
      const GURL& config_url,
      const std::string& manifest_url_prefix,
      std::unique_ptr<PrecacheUnfinishedWork> unfinished_work,
      uint32_t experiment_id,
      const base::WeakPtr<PrecacheDatabase>& precache_database,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_task_runner,
      PrecacheDelegate* precache_delegate);

  virtual ~PrecacheFetcher();

  // Starts fetching resources to precache. URLs are fetched sequentially. Can
  // be called from any thread. Start should only be called once on a
  // PrecacheFetcher instance.
  void Start();

  // Stops all precaching work. The PreacheFetcher should not be used after
  // calling this method.
  std::unique_ptr<PrecacheUnfinishedWork> CancelPrecaching();

 private:
  friend class PrecacheFetcherTest;
  FRIEND_TEST_ALL_PREFIXES(PrecacheFetcherTest,
                           GloballyRankResourcesAfterPauseResume);
  FRIEND_TEST_ALL_PREFIXES(PrecacheFetcherTest, FetcherPoolMaxLimitReached);
  FRIEND_TEST_ALL_PREFIXES(PrecacheFetcherTest,
                           CancelPrecachingAfterAllManifestFetch);
  FRIEND_TEST_ALL_PREFIXES(PrecacheFetcherTest, DailyQuota);

  // Notifies the precache delete that precaching is done, and report
  // completion statistics.
  void NotifyDone(size_t remaining_manifest_urls_to_fetch,
                  size_t remaining_resource_urls_to_fetch);

  // Fetches the next resource or manifest URL, if any remain. Fetching is done
  // sequentially and depth-first: all resources are fetched for a manifest
  // before the next manifest is fetched. This is done to limit the length of
  // the |resource_urls_to_fetch_| list, reducing the memory usage.
  void StartNextFetch();

  void StartNextManifestFetches();
  void StartNextResourceFetch();

  // Called when the precache configuration settings have been fetched.
  // Determines the list of manifest URLs to fetch according to the list of
  // |starting_hosts_| and information from the precache configuration settings.
  // If the fetch of the configuration settings fails, then precaching ends.
  void OnConfigFetchComplete(const Fetcher& source);

  // Constructs manifest URLs using a manifest URL prefix, and lists of hosts.
  void DetermineManifests();

  // Called when a precache manifest has been fetched. Builds the list of
  // resource URLs to fetch according to the URLs in the manifest. If the fetch
  // of a manifest fails, then it skips to the next manifest.
  void OnManifestFetchComplete(int64_t host_visits, const Fetcher& source);

  // Moves the pending resource URLs into the to-be-fetched queue, and sorts and
  // truncates if specified by the PrecacheConfigurationSettings. Called by
  // OnManifestFetchComplete after the last manifest is fetched, so that
  // StartNextFetch will begin fetching resource URLs.
  void QueueResourcesForFetch();

  // Called when a resource has been fetched.
  void OnResourceFetchComplete(const Fetcher& source);

  // Adds up the response sizes.
  void UpdateStats(int64_t response_bytes, int64_t network_response_bytes);

  // Callback invoked when the manifest info for all the top hosts is retrieved.
  void OnManifestInfoRetrieved(std::deque<ManifestHostInfo> manifests_info);

  // Callback invoked when the quota is retrieved.
  void OnQuotaInfoRetrieved(const PrecacheQuota& quota);

  // The request context used when fetching URLs.
  const scoped_refptr<net::URLRequestContextGetter> request_context_;

  // The custom URL to use when fetching the config. If not provided, the
  // default flag-specified URL will be used.
  const GURL config_url_;

  // The custom URL prefix to use when fetching manifests. If not provided, the
  // default flag-specified prefix will be used.
  const std::string manifest_url_prefix_;

  // PrecacheDatabase should be accessed on the DB thread.
  base::WeakPtr<PrecacheDatabase> precache_database_;
  scoped_refptr<base::SingleThreadTaskRunner> db_task_runner_;

  // Non-owning pointer. Should not be NULL.
  PrecacheDelegate* precache_delegate_;

  // Top hosts for which manifests still need to be fetched (i.e. no Fetcher has
  // been created yet).
  std::deque<ManifestHostInfo> top_hosts_to_fetch_;

  // Top hosts for which manifests are currently being fetched.
  std::list<ManifestHostInfo> top_hosts_fetching_;

  // Resources to be fetched, in desired fetch order. Populated only after
  // manifest fetching is complete.
  std::deque<ResourceInfo> resources_to_fetch_;

  // Resources currently being fetched, in the order requested.
  std::list<ResourceInfo> resources_fetching_;

  // Resources to be fetched, not yet ranked. Valid until manifest fetching is
  // done, after which resources are sorted and places in resources_to_fetch_.
  std::deque<ResourceInfo> resources_to_rank_;

  FetcherPool<Fetcher> pool_;

  std::unique_ptr<PrecacheUnfinishedWork> unfinished_work_;

  // Daily quota.
  PrecacheQuota quota_;

  // The fieldtrial experiment ID.
  uint32_t experiment_id_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheFetcher);
};

// Class that fetches a URL, and runs the specified callback when the fetch is
// complete. This class exists so that a different method can be run in
// response to different kinds of fetches, e.g. OnConfigFetchComplete when
// configuration settings are fetched, OnManifestFetchComplete when a manifest
// is fetched, etc.
//
// This class tries to increase freshness while limiting network usage, by using
// the following strategy:
// 1.  Fetch the URL from the cache.
// 2a. If it's present and lacks revalidation headers, then stop.
// 2b. If it's not present, or it's present and has revalidation headers, then
//     refetch over the network.
//
// This allows the precache to "refresh" cache entries by increasing their
// expiration date, but minimizes the network impact of doing so, by performing
// only conditional GETs.
//
// On completion it calls the given callback. This class cancels requests whose
// responses are or will be larger than max_bytes. In such cases,
// network_url_fetcher() will return nullptr.
class PrecacheFetcher::Fetcher : public net::URLFetcherDelegate {
 public:
  // Construct a new Fetcher. This will create and start a new URLFetcher
  // immediately. Parameters:
  //   request_context: The request context to pass to the URLFetcher.
  //   url: The URL to fetch.
  //   referrer: The hostname of the manifest requesting this resource. Empty
  //       for config fetches.
  //   callback: Called when the fetch is finished or cancelled.
  //   is_resource_request: If true, the URL may be refreshed using
  //       LOAD_VALIDATE_CACHE.
  //   max_bytes: The number of bytes to download before cancelling.
  //   revalidation_only: If true, the URL is fetched only if it has an existing
  //       cache entry with conditional headers.
  Fetcher(net::URLRequestContextGetter* request_context,
          const GURL& url,
          const std::string& referrer,
          const base::Callback<void(const Fetcher&)>& callback,
          bool is_resource_request,
          size_t max_bytes,
          bool revalidation_only);
  ~Fetcher() override;
  void OnURLFetchDownloadProgress(const net::URLFetcher* source,
                                  int64_t current,
                                  int64_t total,
                                  int64_t current_network_bytes) override;
  void OnURLFetchComplete(const net::URLFetcher* source) override;
  int64_t response_bytes() const { return response_bytes_; }
  int64_t network_response_bytes() const { return network_response_bytes_; }
  const net::URLFetcher* network_url_fetcher() const {
    return network_url_fetcher_.get();
  }
  const GURL& url() const { return url_; }
  const std::string& referrer() const { return referrer_; }
  bool is_resource_request() const { return is_resource_request_; }
  bool was_cached() const { return was_cached_; }

 private:
  enum class FetchStage { CACHE, NETWORK };

  void LoadFromCache();
  void LoadFromNetwork();

  // The arguments to this Fetcher's constructor.
  net::URLRequestContextGetter* const request_context_;
  const GURL url_;
  const std::string referrer_;
  const base::Callback<void(const Fetcher&)> callback_;
  const bool is_resource_request_;
  const size_t max_bytes_;
  const bool revalidation_only_;

  FetchStage fetch_stage_;
  // The cache_url_fetcher_ is kept alive until Fetcher destruction for testing.
  std::unique_ptr<net::URLFetcher> cache_url_fetcher_;
  std::unique_ptr<net::URLFetcher> network_url_fetcher_;
  int64_t response_bytes_;
  int64_t network_response_bytes_;
  bool was_cached_;

  DISALLOW_COPY_AND_ASSIGN(Fetcher);
};

}  // namespace precache

#endif  // COMPONENTS_PRECACHE_CORE_PRECACHE_FETCHER_H_
