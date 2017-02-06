// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites_impl.h"

#include <stdint.h>
#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/page_usage_data.h"
#include "components/history/core/browser/top_sites_cache.h"
#include "components/history/core/browser/top_sites_observer.h"
#include "components/history/core/browser/url_utils.h"
#include "components/history/core/common/thumbnail_score.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_util.h"

using base::DictionaryValue;

namespace history {
namespace {

void RunOrPostGetMostVisitedURLsCallback(
    base::TaskRunner* task_runner,
    bool include_forced_urls,
    const TopSitesImpl::GetMostVisitedURLsCallback& callback,
    const MostVisitedURLList& all_urls,
    const MostVisitedURLList& nonforced_urls) {
  const MostVisitedURLList* urls =
      include_forced_urls ? &all_urls : &nonforced_urls;
  if (task_runner->RunsTasksOnCurrentThread())
    callback.Run(*urls);
  else
    task_runner->PostTask(FROM_HERE, base::Bind(callback, *urls));
}

// Compares two MostVisitedURL having a non-null |last_forced_time|.
bool ForcedURLComparator(const MostVisitedURL& first,
                         const MostVisitedURL& second) {
  DCHECK(!first.last_forced_time.is_null() &&
         !second.last_forced_time.is_null());
  return first.last_forced_time < second.last_forced_time;
}

// How many non-forced top sites to store in the cache.
const size_t kNonForcedTopSitesNumber = 20;

// How many forced top sites to store in the cache.
const size_t kForcedTopSitesNumber = 20;

// Max number of temporary images we'll cache. See comment above
// temp_images_ for details.
const size_t kMaxTempTopImages = 8;

const int kDaysOfHistory = 90;
// Time from startup to first HistoryService query.
const int64_t kUpdateIntervalSecs = 15;
// Intervals between requests to HistoryService.
const int64_t kMinUpdateIntervalMinutes = 1;
#if defined(OS_IOS) || defined(OS_ANDROID)
// On mobile, having the max at 60 minutes results in the topsites database
// being not updated often enough since the app isn't usually running for long
// stretches of time.
const int64_t kMaxUpdateIntervalMinutes = 5;
#else
const int64_t kMaxUpdateIntervalMinutes = 60;
#endif  // defined(OS_IOS) || defined(OS_ANDROID)

// Use 100 quality (highest quality) because we're very sensitive to
// artifacts for these small sized, highly detailed images.
const int kTopSitesImageQuality = 100;

// Key for preference listing the URLs that should not be shown as most
// visited thumbnails.
const char kMostVisitedURLsBlacklist[] = "ntp.most_visited_blacklist";

}  // namespace

// Initially, histogram is not recorded.
bool TopSitesImpl::histogram_recorded_ = false;

TopSitesImpl::TopSitesImpl(PrefService* pref_service,
                           HistoryService* history_service,
                           const PrepopulatedPageList& prepopulated_pages,
                           const CanAddURLToHistoryFn& can_add_url_to_history)
    : backend_(nullptr),
      cache_(new TopSitesCache()),
      thread_safe_cache_(new TopSitesCache()),
      last_num_urls_changed_(0),
      prepopulated_pages_(prepopulated_pages),
      pref_service_(pref_service),
      history_service_(history_service),
      can_add_url_to_history_(can_add_url_to_history),
      loaded_(false),
      history_service_observer_(this) {
  DCHECK(pref_service_);
  DCHECK(!can_add_url_to_history_.is_null());
}

void TopSitesImpl::Init(
    const base::FilePath& db_name,
    const scoped_refptr<base::SingleThreadTaskRunner>& db_task_runner) {
  // Create the backend here, rather than in the constructor, so that
  // unit tests that do not need the backend can run without a problem.
  backend_ = new TopSitesBackend(db_task_runner);
  backend_->Init(db_name);
  backend_->GetMostVisitedThumbnails(
      base::Bind(&TopSitesImpl::OnGotMostVisitedThumbnails,
                 base::Unretained(this)),
      &cancelable_task_tracker_);
}

bool TopSitesImpl::SetPageThumbnail(const GURL& url,
                                    const gfx::Image& thumbnail,
                                    const ThumbnailScore& score) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!loaded_) {
    // TODO(sky): I need to cache these and apply them after the load
    // completes.
    return false;
  }

  bool add_temp_thumbnail = false;
  if (!IsKnownURL(url)) {
    if (!IsNonForcedFull()) {
      add_temp_thumbnail = true;
    } else {
      return false;  // This URL is not known to us.
    }
  }

  if (!can_add_url_to_history_.Run(url))
    return false;  // It's not a real webpage.

  scoped_refptr<base::RefCountedBytes> thumbnail_data;
  if (!EncodeBitmap(thumbnail, &thumbnail_data))
    return false;

  if (add_temp_thumbnail) {
    // Always remove the existing entry and then add it back. That way if we end
    // up with too many temp thumbnails we'll prune the oldest first.
    RemoveTemporaryThumbnailByURL(url);
    AddTemporaryThumbnail(url, thumbnail_data.get(), score);
    return true;
  }

  return SetPageThumbnailEncoded(url, thumbnail_data.get(), score);
}

bool TopSitesImpl::SetPageThumbnailToJPEGBytes(
    const GURL& url,
    const base::RefCountedMemory* memory,
    const ThumbnailScore& score) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!loaded_) {
    // TODO(sky): I need to cache these and apply them after the load
    // completes.
    return false;
  }

  bool add_temp_thumbnail = false;
  if (!IsKnownURL(url)) {
    if (!IsNonForcedFull()) {
      add_temp_thumbnail = true;
    } else {
      return false;  // This URL is not known to us.
    }
  }

  if (!can_add_url_to_history_.Run(url))
    return false;  // It's not a real webpage.

  if (add_temp_thumbnail) {
    // Always remove the existing entry and then add it back. That way if we end
    // up with too many temp thumbnails we'll prune the oldest first.
    RemoveTemporaryThumbnailByURL(url);
    AddTemporaryThumbnail(url, memory, score);
    return true;
  }

  return SetPageThumbnailEncoded(url, memory, score);
}

// WARNING: this function may be invoked on any thread.
void TopSitesImpl::GetMostVisitedURLs(
    const GetMostVisitedURLsCallback& callback,
    bool include_forced_urls) {
  MostVisitedURLList filtered_urls;
  {
    base::AutoLock lock(lock_);
    if (!loaded_) {
      // A request came in before we finished loading. Store the callback and
      // we'll run it on current thread when we finish loading.
      pending_callbacks_.push_back(
          base::Bind(&RunOrPostGetMostVisitedURLsCallback,
                     base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                     include_forced_urls, callback));
      return;
    }
    if (include_forced_urls) {
      filtered_urls = thread_safe_cache_->top_sites();
    } else {
      filtered_urls.assign(thread_safe_cache_->top_sites().begin() +
                              thread_safe_cache_->GetNumForcedURLs(),
                           thread_safe_cache_->top_sites().end());
    }
  }
  callback.Run(filtered_urls);
}

bool TopSitesImpl::GetPageThumbnail(
    const GURL& url,
    bool prefix_match,
    scoped_refptr<base::RefCountedMemory>* bytes) {
  // WARNING: this may be invoked on any thread.
  // Perform exact match.
  {
    base::AutoLock lock(lock_);
    if (thread_safe_cache_->GetPageThumbnail(url, bytes))
      return true;
  }

  // Resource bundle is thread safe.
  for (const auto& prepopulated_page : prepopulated_pages_) {
    if (url == prepopulated_page.most_visited.url) {
      *bytes =
          ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
              prepopulated_page.thumbnail_id, ui::SCALE_FACTOR_100P);
      return true;
    }
  }

  if (prefix_match) {
    // If http or https, search with |url| first, then try the other one.
    std::vector<GURL> url_list;
    url_list.push_back(url);
    if (url.SchemeIsHTTPOrHTTPS())
      url_list.push_back(ToggleHTTPAndHTTPS(url));

    for (std::vector<GURL>::iterator it = url_list.begin();
         it != url_list.end(); ++it) {
      base::AutoLock lock(lock_);

      GURL canonical_url;
      // Test whether any stored URL is a prefix of |url|.
      canonical_url = thread_safe_cache_->GetGeneralizedCanonicalURL(*it);
      if (!canonical_url.is_empty() &&
          thread_safe_cache_->GetPageThumbnail(canonical_url, bytes)) {
        return true;
      }
    }
  }

  return false;
}

bool TopSitesImpl::GetPageThumbnailScore(const GURL& url,
                                         ThumbnailScore* score) {
  // WARNING: this may be invoked on any thread.
  base::AutoLock lock(lock_);
  return thread_safe_cache_->GetPageThumbnailScore(url, score);
}

bool TopSitesImpl::GetTemporaryPageThumbnailScore(const GURL& url,
                                                  ThumbnailScore* score) {
  for (TempImages::iterator i = temp_images_.begin(); i != temp_images_.end();
       ++i) {
    if (i->first == url) {
      *score = i->second.thumbnail_score;
      return true;
    }
  }
  return false;
}


// Returns the index of |url| in |urls|, or -1 if not found.
static int IndexOf(const MostVisitedURLList& urls, const GURL& url) {
  for (size_t i = 0; i < urls.size(); i++) {
    if (urls[i].url == url)
      return i;
  }
  return -1;
}

void TopSitesImpl::SyncWithHistory() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (loaded_ && temp_images_.size()) {
    // If we have temporary thumbnails it means there isn't much data, and most
    // likely the user is first running Chrome. During this time we throttle
    // updating from history by 30 seconds. If the user creates a new tab page
    // during this window of time we force updating from history so that the new
    // tab page isn't so far out of date.
    timer_.Stop();
    StartQueryForMostVisited();
  }
}

bool TopSitesImpl::HasBlacklistedItems() const {
  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  return blacklist && !blacklist->empty();
}

void TopSitesImpl::AddBlacklistedURL(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::unique_ptr<base::Value> dummy = base::Value::CreateNullValue();
  {
    DictionaryPrefUpdate update(pref_service_, kMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->SetWithoutPathExpansion(GetURLHash(url), std::move(dummy));
  }

  ResetThreadSafeCache();
  NotifyTopSitesChanged(TopSitesObserver::ChangeReason::BLACKLIST);
}

void TopSitesImpl::RemoveBlacklistedURL(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    DictionaryPrefUpdate update(pref_service_, kMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->RemoveWithoutPathExpansion(GetURLHash(url), nullptr);
  }
  ResetThreadSafeCache();
  NotifyTopSitesChanged(TopSitesObserver::ChangeReason::BLACKLIST);
}

bool TopSitesImpl::IsBlacklisted(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  return blacklist && blacklist->HasKey(GetURLHash(url));
}

void TopSitesImpl::ClearBlacklistedURLs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    DictionaryPrefUpdate update(pref_service_, kMostVisitedURLsBlacklist);
    base::DictionaryValue* blacklist = update.Get();
    blacklist->Clear();
  }
  ResetThreadSafeCache();
  NotifyTopSitesChanged(TopSitesObserver::ChangeReason::BLACKLIST);
}

void TopSitesImpl::ShutdownOnUIThread() {
  history_service_ = nullptr;
  history_service_observer_.RemoveAll();
  // Cancel all requests so that the service doesn't callback to us after we've
  // invoked Shutdown (this could happen if we have a pending request and
  // Shutdown is invoked).
  cancelable_task_tracker_.TryCancelAll();
  if (backend_)
    backend_->Shutdown();
}

// static
void TopSitesImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kMostVisitedURLsBlacklist);
}

// static
void TopSitesImpl::DiffMostVisited(const MostVisitedURLList& old_list,
                                   const MostVisitedURLList& new_list,
                                   TopSitesDelta* delta) {

  // Add all the old URLs for quick lookup. This maps URLs to the corresponding
  // index in the input.
  std::map<GURL, size_t> all_old_urls;
  size_t num_old_forced = 0;
  for (size_t i = 0; i < old_list.size(); i++) {
    if (!old_list[i].last_forced_time.is_null())
      num_old_forced++;
    DCHECK(old_list[i].last_forced_time.is_null() || i < num_old_forced)
        << "Forced URLs must all appear before non-forced URLs.";
    all_old_urls[old_list[i].url] = i;
  }

  // Check all the URLs in the new set to see which ones are new or just moved.
  // When we find a match in the old set, we'll reset its index to our special
  // marker. This allows us to quickly identify the deleted ones in a later
  // pass.
  const size_t kAlreadyFoundMarker = static_cast<size_t>(-1);
  int rank = -1;  // Forced URLs have a rank of -1.
  for (size_t i = 0; i < new_list.size(); i++) {
    // Increase the rank if we're going through forced URLs. This works because
    // non-forced URLs all come after forced URLs.
    if (new_list[i].last_forced_time.is_null())
      rank++;
    DCHECK(new_list[i].last_forced_time.is_null() == (rank != -1))
        << "Forced URLs must all appear before non-forced URLs.";
    std::map<GURL, size_t>::iterator found = all_old_urls.find(new_list[i].url);
    if (found == all_old_urls.end()) {
      MostVisitedURLWithRank added;
      added.url = new_list[i];
      added.rank = rank;
      delta->added.push_back(added);
    } else {
      DCHECK(found->second != kAlreadyFoundMarker)
          << "Same URL appears twice in the new list.";
      int old_rank = found->second >= num_old_forced ?
          found->second - num_old_forced : -1;
      if (old_rank != rank ||
          old_list[found->second].last_forced_time !=
              new_list[i].last_forced_time) {
        MostVisitedURLWithRank moved;
        moved.url = new_list[i];
        moved.rank = rank;
        delta->moved.push_back(moved);
      }
      found->second = kAlreadyFoundMarker;
    }
  }

  // Any member without the special marker in the all_old_urls list means that
  // there wasn't a "new" URL that mapped to it, so it was deleted.
  for (std::map<GURL, size_t>::const_iterator i = all_old_urls.begin();
       i != all_old_urls.end(); ++i) {
    if (i->second != kAlreadyFoundMarker)
      delta->deleted.push_back(old_list[i->second]);
  }
}

base::CancelableTaskTracker::TaskId TopSitesImpl::StartQueryForMostVisited() {
  DCHECK(loaded_);
  if (!history_service_)
    return base::CancelableTaskTracker::kBadTaskId;

  return history_service_->QueryMostVisitedURLs(
      num_results_to_request_from_history(), kDaysOfHistory,
      base::Bind(&TopSitesImpl::OnTopSitesAvailableFromHistory,
                 base::Unretained(this)),
      &cancelable_task_tracker_);
}

bool TopSitesImpl::IsKnownURL(const GURL& url) {
  return loaded_ && cache_->IsKnownURL(url);
}

const std::string& TopSitesImpl::GetCanonicalURLString(const GURL& url) const {
  return cache_->GetCanonicalURL(url).spec();
}

bool TopSitesImpl::IsNonForcedFull() {
  return loaded_ && cache_->GetNumNonForcedURLs() >= kNonForcedTopSitesNumber;
}

bool TopSitesImpl::IsForcedFull() {
  return loaded_ && cache_->GetNumForcedURLs() >= kForcedTopSitesNumber;
}

TopSitesImpl::~TopSitesImpl() {
}

bool TopSitesImpl::SetPageThumbnailNoDB(
    const GURL& url,
    const base::RefCountedMemory* thumbnail_data,
    const ThumbnailScore& score) {
  // This should only be invoked when we know about the url.
  DCHECK(cache_->IsKnownURL(url));

  const MostVisitedURL& most_visited =
      cache_->top_sites()[cache_->GetURLIndex(url)];
  Images* image = cache_->GetImage(url);

  // When comparing the thumbnail scores, we need to take into account the
  // redirect hops, which are not generated when the thumbnail is because the
  // redirects weren't known. We fill that in here since we know the redirects.
  ThumbnailScore new_score_with_redirects(score);
  new_score_with_redirects.redirect_hops_from_dest =
      GetRedirectDistanceForURL(most_visited, url);

  if (!ShouldReplaceThumbnailWith(image->thumbnail_score,
                                  new_score_with_redirects) &&
      image->thumbnail.get())
    return false;  // The one we already have is better.

  image->thumbnail = const_cast<base::RefCountedMemory*>(thumbnail_data);
  image->thumbnail_score = new_score_with_redirects;

  ResetThreadSafeImageCache();
  return true;
}

bool TopSitesImpl::SetPageThumbnailEncoded(
    const GURL& url,
    const base::RefCountedMemory* thumbnail,
    const ThumbnailScore& score) {
  if (!SetPageThumbnailNoDB(url, thumbnail, score))
    return false;

  // Update the database.
  if (!cache_->IsKnownURL(url))
    return false;

  size_t index = cache_->GetURLIndex(url);
  int url_rank = index - cache_->GetNumForcedURLs();
  const MostVisitedURL& most_visited = cache_->top_sites()[index];
  backend_->SetPageThumbnail(most_visited,
                             url_rank < 0 ? -1 : url_rank,
                             *(cache_->GetImage(most_visited.url)));
  return true;
}

// static
bool TopSitesImpl::EncodeBitmap(const gfx::Image& bitmap,
                                scoped_refptr<base::RefCountedBytes>* bytes) {
  if (bitmap.IsEmpty())
    return false;
  *bytes = new base::RefCountedBytes();
  std::vector<unsigned char> data;
  if (!gfx::JPEG1xEncodedDataFromImage(bitmap, kTopSitesImageQuality, &data))
    return false;

  // As we're going to cache this data, make sure the vector is only as big as
  // it needs to be, as JPEGCodec::Encode() over-allocates data.capacity().
  // (In a C++0x future, we can just call shrink_to_fit() in Encode())
  (*bytes)->data() = data;
  return true;
}

void TopSitesImpl::RemoveTemporaryThumbnailByURL(const GURL& url) {
  for (TempImages::iterator i = temp_images_.begin(); i != temp_images_.end();
       ++i) {
    if (i->first == url) {
      temp_images_.erase(i);
      return;
    }
  }
}

void TopSitesImpl::AddTemporaryThumbnail(
    const GURL& url,
    const base::RefCountedMemory* thumbnail,
    const ThumbnailScore& score) {
  if (temp_images_.size() == kMaxTempTopImages)
    temp_images_.erase(temp_images_.begin());

  TempImage image;
  image.first = url;
  image.second.thumbnail = const_cast<base::RefCountedMemory*>(thumbnail);
  image.second.thumbnail_score = score;
  temp_images_.push_back(image);
}

void TopSitesImpl::TimerFired() {
  StartQueryForMostVisited();
}

// static
int TopSitesImpl::GetRedirectDistanceForURL(const MostVisitedURL& most_visited,
                                            const GURL& url) {
  for (size_t i = 0; i < most_visited.redirects.size(); i++) {
    if (most_visited.redirects[i] == url)
      return static_cast<int>(most_visited.redirects.size() - i - 1);
  }
  NOTREACHED() << "URL should always be found.";
  return 0;
}

PrepopulatedPageList TopSitesImpl::GetPrepopulatedPages() {
  return prepopulated_pages_;
}

bool TopSitesImpl::loaded() const {
  return loaded_;
}

bool TopSitesImpl::AddForcedURL(const GURL& url, const base::Time& time) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!loaded_) {
    // Optimally we could cache this and apply it after the load completes, but
    // in practice it's not an issue since AddForcedURL will be called again
    // next time the user hits the NTP.
    return false;
  }

  size_t num_forced = cache_->GetNumForcedURLs();
  MostVisitedURLList new_list(cache_->top_sites());
  MostVisitedURL new_url;

  if (cache_->IsKnownURL(url)) {
    size_t index = cache_->GetURLIndex(url);
    // Do nothing if we currently have that URL as non-forced.
    if (new_list[index].last_forced_time.is_null())
      return false;

    // Update the |last_forced_time| of the already existing URL. Delete it and
    // reinsert it at the right location.
    new_url = new_list[index];
    new_list.erase(new_list.begin() + index);
    num_forced--;
  } else {
    new_url.url = url;
    new_url.redirects.push_back(url);
  }
  new_url.last_forced_time = time;
  // Add forced URLs and sort. Added to the end of the list of forced URLs
  // since this is almost always where it needs to go, unless the user's local
  // clock is fiddled with.
  MostVisitedURLList::iterator mid = new_list.begin() + num_forced;
  new_list.insert(mid, new_url);
  mid = new_list.begin() + num_forced;  // Mid was invalidated.
  std::inplace_merge(new_list.begin(), mid, mid + 1, ForcedURLComparator);
  SetTopSites(new_list, CALL_LOCATION_FROM_FORCED_URLS);
  return true;
}

void TopSitesImpl::OnNavigationCommitted(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!loaded_ || IsNonForcedFull())
    return;

  if (!cache_->IsKnownURL(url) && can_add_url_to_history_.Run(url)) {
    // To avoid slamming history we throttle requests when the url updates. To
    // do otherwise negatively impacts perf tests.
    RestartQueryForTopSitesTimer(GetUpdateDelay());
  }
}

bool TopSitesImpl::AddPrepopulatedPages(MostVisitedURLList* urls,
                                        size_t num_forced_urls) {
  bool added = false;
  for (const auto& prepopulated_page : prepopulated_pages_) {
    if (urls->size() - num_forced_urls < kNonForcedTopSitesNumber &&
        IndexOf(*urls, prepopulated_page.most_visited.url) == -1) {
      urls->push_back(prepopulated_page.most_visited);
      added = true;
    }
  }
  return added;
}

size_t TopSitesImpl::MergeCachedForcedURLs(MostVisitedURLList* new_list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Add all the new URLs for quick lookup. Take that opportunity to count the
  // number of forced URLs in |new_list|.
  std::set<GURL> all_new_urls;
  size_t num_forced = 0;
  for (size_t i = 0; i < new_list->size(); ++i) {
    for (size_t j = 0; j < (*new_list)[i].redirects.size(); j++) {
      all_new_urls.insert((*new_list)[i].redirects[j]);
    }
    if (!(*new_list)[i].last_forced_time.is_null())
      ++num_forced;
  }

  // Keep the forced URLs from |cache_| that are not found in |new_list|.
  MostVisitedURLList filtered_forced_urls;
  for (size_t i = 0; i < cache_->GetNumForcedURLs(); ++i) {
    if (all_new_urls.find(cache_->top_sites()[i].url) == all_new_urls.end())
      filtered_forced_urls.push_back(cache_->top_sites()[i]);
  }
  num_forced += filtered_forced_urls.size();

  // Prepend forced URLs and sort in order of ascending |last_forced_time|.
  new_list->insert(new_list->begin(), filtered_forced_urls.begin(),
                   filtered_forced_urls.end());
  std::inplace_merge(
      new_list->begin(), new_list->begin() + filtered_forced_urls.size(),
      new_list->begin() + num_forced, ForcedURLComparator);

  // Drop older forced URLs if the list overflows. Since forced URLs are always
  // sort in increasing order of |last_forced_time|, drop the first ones.
  if (num_forced > kForcedTopSitesNumber) {
    new_list->erase(new_list->begin(),
                    new_list->begin() + (num_forced - kForcedTopSitesNumber));
    num_forced = kForcedTopSitesNumber;
  }

  return num_forced;
}

void TopSitesImpl::ApplyBlacklist(const MostVisitedURLList& urls,
                                  MostVisitedURLList* out) {
  // Log the number of times ApplyBlacklist is called so we can compute the
  // average number of blacklisted items per user.
  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  UMA_HISTOGRAM_BOOLEAN("TopSites.NumberOfApplyBlacklist", true);
  UMA_HISTOGRAM_COUNTS_100("TopSites.NumberOfBlacklistedItems",
      (blacklist ? blacklist->size() : 0));
  size_t num_non_forced_urls = 0;
  size_t num_forced_urls = 0;
  for (size_t i = 0; i < urls.size(); ++i) {
    if (!IsBlacklisted(urls[i].url)) {
      if (urls[i].last_forced_time.is_null()) {
        // Non-forced URL.
        if (num_non_forced_urls >= kNonForcedTopSitesNumber)
          continue;
        num_non_forced_urls++;
      } else {
        // Forced URL.
        if (num_forced_urls >= kForcedTopSitesNumber)
          continue;
        num_forced_urls++;
      }
      out->push_back(urls[i]);
    }
  }
}

std::string TopSitesImpl::GetURLHash(const GURL& url) {
  // We don't use canonical URLs here to be able to blacklist only one of
  // the two 'duplicate' sites, e.g. 'gmail.com' and 'mail.google.com'.
  return base::MD5String(url.spec());
}

base::TimeDelta TopSitesImpl::GetUpdateDelay() {
  if (cache_->top_sites().size() <= prepopulated_pages_.size())
    return base::TimeDelta::FromSeconds(30);

  int64_t range = kMaxUpdateIntervalMinutes - kMinUpdateIntervalMinutes;
  int64_t minutes = kMaxUpdateIntervalMinutes -
                    last_num_urls_changed_ * range / cache_->top_sites().size();
  return base::TimeDelta::FromMinutes(minutes);
}

void TopSitesImpl::SetTopSites(const MostVisitedURLList& new_top_sites,
                               const CallLocation location) {
  DCHECK(thread_checker_.CalledOnValidThread());

  MostVisitedURLList top_sites(new_top_sites);
  size_t num_forced_urls = MergeCachedForcedURLs(&top_sites);
  AddPrepopulatedPages(&top_sites, num_forced_urls);

  TopSitesDelta delta;
  DiffMostVisited(cache_->top_sites(), top_sites, &delta);

  TopSitesBackend::RecordHistogram record_or_not =
      TopSitesBackend::RECORD_HISTOGRAM_NO;

  // Record the delta size into a histogram if this function is called from
  // function OnGotMostVisitedThumbnails and no histogram value has been
  // recorded before.
  if (location == CALL_LOCATION_FROM_ON_GOT_MOST_VISITED_THUMBNAILS &&
      !histogram_recorded_) {
    size_t delta_size =
        delta.deleted.size() + delta.added.size() + delta.moved.size();
    UMA_HISTOGRAM_COUNTS_100("History.FirstSetTopSitesDeltaSize", delta_size);
    // Will be passed to TopSitesBackend to let it record the histogram too.
    record_or_not = TopSitesBackend::RECORD_HISTOGRAM_YES;
    // Change it to true so that the histogram will not be recorded any more.
    histogram_recorded_ = true;
  }

  if (!delta.deleted.empty() || !delta.added.empty() || !delta.moved.empty()) {
    backend_->UpdateTopSites(delta, record_or_not);
  }

  last_num_urls_changed_ = delta.added.size() + delta.moved.size();

  // We always do the following steps (setting top sites in cache, and resetting
  // thread safe cache ...) as this method is invoked during startup at which
  // point the caches haven't been updated yet.
  cache_->SetTopSites(top_sites);

  // See if we have any tmp thumbnails for the new sites.
  if (!temp_images_.empty()) {
    for (size_t i = 0; i < top_sites.size(); ++i) {
      const MostVisitedURL& mv = top_sites[i];
      GURL canonical_url = cache_->GetCanonicalURL(mv.url);
      // At the time we get the thumbnail redirects aren't known, so we have to
      // iterate through all the images.
      for (TempImages::iterator it = temp_images_.begin();
           it != temp_images_.end(); ++it) {
        if (canonical_url == cache_->GetCanonicalURL(it->first)) {
          SetPageThumbnailEncoded(
              mv.url, it->second.thumbnail.get(), it->second.thumbnail_score);
          temp_images_.erase(it);
          break;
        }
      }
    }
  }

  if (top_sites.size() - num_forced_urls >= kNonForcedTopSitesNumber)
    temp_images_.clear();

  ResetThreadSafeCache();
  ResetThreadSafeImageCache();
  if (location == CALL_LOCATION_FROM_FORCED_URLS)
    NotifyTopSitesChanged(TopSitesObserver::ChangeReason::FORCED_URL);
  else
    NotifyTopSitesChanged(TopSitesObserver::ChangeReason::MOST_VISITED);

  // Restart the timer that queries history for top sites. This is done to
  // ensure we stay in sync with history.
  RestartQueryForTopSitesTimer(GetUpdateDelay());
}

int TopSitesImpl::num_results_to_request_from_history() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(kMostVisitedURLsBlacklist);
  return kNonForcedTopSitesNumber + (blacklist ? blacklist->size() : 0);
}

void TopSitesImpl::MoveStateToLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());

  MostVisitedURLList filtered_urls_all;
  MostVisitedURLList filtered_urls_nonforced;
  PendingCallbacks pending_callbacks;
  {
    base::AutoLock lock(lock_);

    if (loaded_)
      return;  // Don't do anything if we're already loaded.
    loaded_ = true;

    // Now that we're loaded we can service the queued up callbacks. Copy them
    // here and service them outside the lock.
    if (!pending_callbacks_.empty()) {
      // We always filter out forced URLs because callers of GetMostVisitedURLs
      // are not interested in them.
      filtered_urls_all = thread_safe_cache_->top_sites();
      filtered_urls_nonforced.assign(thread_safe_cache_->top_sites().begin() +
                                       thread_safe_cache_->GetNumForcedURLs(),
                                     thread_safe_cache_->top_sites().end());
      pending_callbacks.swap(pending_callbacks_);
    }
  }

  for (size_t i = 0; i < pending_callbacks.size(); i++)
    pending_callbacks[i].Run(filtered_urls_all, filtered_urls_nonforced);

  if (history_service_)
    history_service_observer_.Add(history_service_);

  NotifyTopSitesLoaded();
}

void TopSitesImpl::ResetThreadSafeCache() {
  base::AutoLock lock(lock_);
  MostVisitedURLList cached;
  ApplyBlacklist(cache_->top_sites(), &cached);
  thread_safe_cache_->SetTopSites(cached);
}

void TopSitesImpl::ResetThreadSafeImageCache() {
  base::AutoLock lock(lock_);
  thread_safe_cache_->SetThumbnails(cache_->images());
}

void TopSitesImpl::RestartQueryForTopSitesTimer(base::TimeDelta delta) {
  if (timer_.IsRunning() && ((timer_start_time_ + timer_.GetCurrentDelay()) <
                             (base::TimeTicks::Now() + delta))) {
    return;
  }

  timer_start_time_ = base::TimeTicks::Now();
  timer_.Stop();
  timer_.Start(FROM_HERE, delta, this, &TopSitesImpl::TimerFired);
}

void TopSitesImpl::OnGotMostVisitedThumbnails(
    const scoped_refptr<MostVisitedThumbnails>& thumbnails) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Set the top sites directly in the cache so that SetTopSites diffs
  // correctly.
  cache_->SetTopSites(thumbnails->most_visited);
  SetTopSites(thumbnails->most_visited,
              CALL_LOCATION_FROM_ON_GOT_MOST_VISITED_THUMBNAILS);
  cache_->SetThumbnails(thumbnails->url_to_images_map);

  ResetThreadSafeImageCache();

  MoveStateToLoaded();

  // Start a timer that refreshes top sites from history.
  RestartQueryForTopSitesTimer(
      base::TimeDelta::FromSeconds(kUpdateIntervalSecs));
}

void TopSitesImpl::OnTopSitesAvailableFromHistory(
    const MostVisitedURLList* pages) {
  DCHECK(pages);
  SetTopSites(*pages, CALL_LOCATION_FROM_OTHER_PLACES);
}

void TopSitesImpl::OnURLsDeleted(HistoryService* history_service,
                                 bool all_history,
                                 bool expired,
                                 const URLRows& deleted_rows,
                                 const std::set<GURL>& favicon_urls) {
  if (!loaded_)
    return;

  if (all_history) {
    SetTopSites(MostVisitedURLList(), CALL_LOCATION_FROM_OTHER_PLACES);
    backend_->ResetDatabase();
  } else {
    std::set<size_t> indices_to_delete;  // Indices into top_sites_.
    for (const auto& row : deleted_rows) {
      if (cache_->IsKnownURL(row.url()))
        indices_to_delete.insert(cache_->GetURLIndex(row.url()));
    }

    if (indices_to_delete.empty())
      return;

    MostVisitedURLList new_top_sites(cache_->top_sites());
    for (std::set<size_t>::reverse_iterator i = indices_to_delete.rbegin();
         i != indices_to_delete.rend(); i++) {
      new_top_sites.erase(new_top_sites.begin() + *i);
    }
    SetTopSites(new_top_sites, CALL_LOCATION_FROM_OTHER_PLACES);
  }
  StartQueryForMostVisited();
}

}  // namespace history
