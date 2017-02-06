// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The history system runs on a background thread so that potentially slow
// database operations don't delay the browser. This backend processing is
// represented by HistoryBackend. The HistoryService's job is to dispatch to
// that thread.
//
// Main thread                       History thread
// -----------                       --------------
// HistoryService <----------------> HistoryBackend
//                                   -> HistoryDatabase
//                                      -> SQLite connection to History
//                                   -> ThumbnailDatabase
//                                      -> SQLite connection to Thumbnails
//                                         (and favicons)

#include "components/history/core/browser/history_service.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/history/core/browser/download_row.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_backend_client.h"
#include "components/history/core/browser/history_client.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/in_memory_history_backend.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/visit_database.h"
#include "components/history/core/browser/visit_delegate.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/history/core/common/thumbnail_score.h"
#include "sync/api/sync_error_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_IOS)
#include "base/critical_closure.h"
#endif

using base::Time;

namespace history {
namespace {

static const char* kHistoryThreadName = "Chrome_HistoryThread";

void RunWithFaviconResults(
    const favicon_base::FaviconResultsCallback& callback,
    std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results) {
  callback.Run(*bitmap_results);
}

void RunWithFaviconResult(
    const favicon_base::FaviconRawBitmapCallback& callback,
    favicon_base::FaviconRawBitmapResult* bitmap_result) {
  callback.Run(*bitmap_result);
}

void RunWithQueryURLResult(const HistoryService::QueryURLCallback& callback,
                           const QueryURLResult* result) {
  callback.Run(result->success, result->row, result->visits);
}

void RunWithVisibleVisitCountToHostResult(
    const HistoryService::GetVisibleVisitCountToHostCallback& callback,
    const VisibleVisitCountToHostResult* result) {
  callback.Run(result->success, result->count, result->first_visit);
}

// Callback from WebHistoryService::ExpireWebHistory().
void ExpireWebHistoryComplete(bool success) {
  // Ignore the result.
  //
  // TODO(davidben): ExpireLocalAndRemoteHistoryBetween callback should not fire
  // until this completes.
}

}  // namespace

// Sends messages from the backend to us on the main thread. This must be a
// separate class from the history service so that it can hold a reference to
// the history service (otherwise we would have to manually AddRef and
// Release when the Backend has a reference to us).
class HistoryService::BackendDelegate : public HistoryBackend::Delegate {
 public:
  BackendDelegate(
      const base::WeakPtr<HistoryService>& history_service,
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner)
      : history_service_(history_service),
        service_task_runner_(service_task_runner) {}

  void NotifyProfileError(sql::InitStatus init_status) override {
    // Send to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&HistoryService::NotifyProfileError,
                              history_service_, init_status));
  }

  void SetInMemoryBackend(
      std::unique_ptr<InMemoryHistoryBackend> backend) override {
    // Send the backend to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&HistoryService::SetInMemoryBackend,
                              history_service_, base::Passed(&backend)));
  }

  void NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                             const GURL& icon_url) override {
    // Send the notification to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&HistoryService::NotifyFaviconsChanged,
                              history_service_, page_urls, icon_url));
  }

  void NotifyURLVisited(ui::PageTransition transition,
                        const URLRow& row,
                        const RedirectList& redirects,
                        base::Time visit_time) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::NotifyURLVisited, history_service_,
                   transition, row, redirects, visit_time));
  }

  void NotifyURLsModified(const URLRows& changed_urls) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&HistoryService::NotifyURLsModified,
                              history_service_, changed_urls));
  }

  void NotifyURLsDeleted(bool all_history,
                         bool expired,
                         const URLRows& deleted_rows,
                         const std::set<GURL>& favicon_urls) override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::NotifyURLsDeleted, history_service_,
                   all_history, expired, deleted_rows, favicon_urls));
  }

  void NotifyKeywordSearchTermUpdated(const URLRow& row,
                                      KeywordID keyword_id,
                                      const base::string16& term) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&HistoryService::NotifyKeywordSearchTermUpdated,
                              history_service_, row, keyword_id, term));
  }

  void NotifyKeywordSearchTermDeleted(URLID url_id) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&HistoryService::NotifyKeywordSearchTermDeleted,
                              history_service_, url_id));
  }

  void DBLoaded() override {
    service_task_runner_->PostTask(
        FROM_HERE, base::Bind(&HistoryService::OnDBLoaded, history_service_));
  }

 private:
  const base::WeakPtr<HistoryService> history_service_;
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
};

// The history thread is intentionally not a BrowserThread because the
// sync integration unit tests depend on being able to create more than one
// history thread.
HistoryService::HistoryService()
    : thread_(new base::Thread(kHistoryThreadName)),
      history_client_(nullptr),
      backend_loaded_(false),
      weak_ptr_factory_(this) {
}

HistoryService::HistoryService(std::unique_ptr<HistoryClient> history_client,
                               std::unique_ptr<VisitDelegate> visit_delegate)
    : thread_(new base::Thread(kHistoryThreadName)),
      history_client_(std::move(history_client)),
      visit_delegate_(std::move(visit_delegate)),
      backend_loaded_(false),
      weak_ptr_factory_(this) {}

HistoryService::~HistoryService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
  Cleanup();
}

bool HistoryService::BackendLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return backend_loaded_;
}

#if defined(OS_IOS)
void HistoryService::HandleBackgrounding() {
  if (!thread_ || !history_backend_.get())
    return;

  ScheduleTask(PRIORITY_NORMAL,
               base::MakeCriticalClosure(base::Bind(
                   &HistoryBackend::PersistState, history_backend_.get())));
}
#endif

void HistoryService::ClearCachedDataForContextID(ContextID context_id) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL,
               base::Bind(&HistoryBackend::ClearCachedDataForContextID,
                          history_backend_.get(), context_id));
}

URLDatabase* HistoryService::InMemoryDatabase() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return in_memory_backend_ ? in_memory_backend_->db() : nullptr;
}

TypedUrlSyncableService* HistoryService::GetTypedUrlSyncableService() const {
  return history_backend_->GetTypedUrlSyncableService();
}

void HistoryService::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Cleanup();
}

void HistoryService::SetKeywordSearchTermsForURL(const GURL& url,
                                                 KeywordID keyword_id,
                                                 const base::string16& term) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_UI,
               base::Bind(&HistoryBackend::SetKeywordSearchTermsForURL,
                          history_backend_.get(), url, keyword_id, term));
}

void HistoryService::DeleteAllSearchTermsForKeyword(KeywordID keyword_id) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (in_memory_backend_)
    in_memory_backend_->DeleteAllSearchTermsForKeyword(keyword_id);

  ScheduleTask(PRIORITY_UI,
               base::Bind(&HistoryBackend::DeleteAllSearchTermsForKeyword,
                          history_backend_.get(), keyword_id));
}

void HistoryService::DeleteKeywordSearchTermForURL(const GURL& url) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_UI,
               base::Bind(&HistoryBackend::DeleteKeywordSearchTermForURL,
                          history_backend_.get(), url));
}

void HistoryService::DeleteMatchingURLsForKeyword(KeywordID keyword_id,
                                                  const base::string16& term) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_UI,
               base::Bind(&HistoryBackend::DeleteMatchingURLsForKeyword,
                          history_backend_.get(), keyword_id, term));
}

void HistoryService::URLsNoLongerBookmarked(const std::set<GURL>& urls) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL,
               base::Bind(&HistoryBackend::URLsNoLongerBookmarked,
                          history_backend_.get(), urls));
}

void HistoryService::AddObserver(HistoryServiceObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void HistoryService::RemoveObserver(HistoryServiceObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

base::CancelableTaskTracker::TaskId HistoryService::ScheduleDBTask(
    std::unique_ptr<HistoryDBTask> task,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  base::CancelableTaskTracker::TaskId task_id =
      tracker->NewTrackedTaskId(&is_canceled);
  // Use base::ThreadTaskRunnerHandler::Get() to get a message loop proxy to
  // the current message loop so that we can forward the call to the method
  // HistoryDBTask::DoneRunOnMainThread() in the correct thread.
  thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&HistoryBackend::ProcessDBTask,
                            history_backend_.get(), base::Passed(&task),
                            base::ThreadTaskRunnerHandle::Get(), is_canceled));
  return task_id;
}

void HistoryService::FlushForTest(const base::Closure& flushed) {
  thread_->task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&base::DoNothing), flushed);
}

void HistoryService::SetOnBackendDestroyTask(const base::Closure& task) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(
      PRIORITY_NORMAL,
      base::Bind(&HistoryBackend::SetOnBackendDestroyTask,
                 history_backend_.get(), base::MessageLoop::current(), task));
}

void HistoryService::TopHosts(size_t num_hosts,
                              const TopHostsCallback& callback) const {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  PostTaskAndReplyWithResult(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::TopHosts, history_backend_.get(), num_hosts),
      callback);
}

void HistoryService::GetCountsAndLastVisitForOrigins(
    const std::set<GURL>& origins,
    const GetCountsAndLastVisitForOriginsCallback& callback) const {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  PostTaskAndReplyWithResult(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::GetCountsAndLastVisitForOrigins,
                 history_backend_.get(), origins),
      callback);
}

void HistoryService::HostRankIfAvailable(
    const GURL& url,
    const base::Callback<void(int)>& callback) const {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  PostTaskAndReplyWithResult(thread_->task_runner().get(), FROM_HERE,
                             base::Bind(&HistoryBackend::HostRankIfAvailable,
                                        history_backend_.get(), url),
                             callback);
}

void HistoryService::AddPage(const GURL& url,
                             Time time,
                             ContextID context_id,
                             int nav_entry_id,
                             const GURL& referrer,
                             const RedirectList& redirects,
                             ui::PageTransition transition,
                             VisitSource visit_source,
                             bool did_replace_entry) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AddPage(HistoryAddPageArgs(url, time, context_id, nav_entry_id, referrer,
                             redirects, transition, visit_source,
                             did_replace_entry));
}

void HistoryService::AddPage(const GURL& url,
                             base::Time time,
                             VisitSource visit_source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AddPage(HistoryAddPageArgs(url, time, nullptr, 0, GURL(), RedirectList(),
                             ui::PAGE_TRANSITION_LINK, visit_source, false));
}

void HistoryService::AddPage(const HistoryAddPageArgs& add_page_args) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  // Filter out unwanted URLs. We don't add auto-subframe URLs. They are a
  // large part of history (think iframes for ads) and we never display them in
  // history UI. We will still add manual subframes, which are ones the user
  // has clicked on to get.
  if (history_client_ && !history_client_->CanAddURL(add_page_args.url))
    return;

  // Inform VisitedDelegate of all links and redirects.
  if (visit_delegate_) {
    if (!add_page_args.redirects.empty()) {
      // We should not be asked to add a page in the middle of a redirect chain,
      // and thus add_page_args.url should be the last element in the array
      // add_page_args.redirects which mean we can use VisitDelegate::AddURLs()
      // with the whole array.
      DCHECK_EQ(add_page_args.url, add_page_args.redirects.back());
      visit_delegate_->AddURLs(add_page_args.redirects);
    } else {
      visit_delegate_->AddURL(add_page_args.url);
    }
  }

  ScheduleTask(PRIORITY_NORMAL,
               base::Bind(&HistoryBackend::AddPage, history_backend_.get(),
                          add_page_args));
}

void HistoryService::AddPageNoVisitForBookmark(const GURL& url,
                                               const base::string16& title) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (history_client_ && !history_client_->CanAddURL(url))
    return;

  ScheduleTask(PRIORITY_NORMAL,
               base::Bind(&HistoryBackend::AddPageNoVisitForBookmark,
                          history_backend_.get(), url, title));
}

void HistoryService::SetPageTitle(const GURL& url,
                                  const base::string16& title) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL, base::Bind(&HistoryBackend::SetPageTitle,
                                           history_backend_.get(), url, title));
}

void HistoryService::UpdateWithPageEndTime(ContextID context_id,
                                           int nav_entry_id,
                                           const GURL& url,
                                           Time end_ts) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(
      PRIORITY_NORMAL,
      base::Bind(&HistoryBackend::UpdateWithPageEndTime, history_backend_.get(),
                 context_id, nav_entry_id, url, end_ts));
}

void HistoryService::AddPageWithDetails(const GURL& url,
                                        const base::string16& title,
                                        int visit_count,
                                        int typed_count,
                                        Time last_visit,
                                        bool hidden,
                                        VisitSource visit_source) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  // Filter out unwanted URLs.
  if (history_client_ && !history_client_->CanAddURL(url))
    return;

  // Inform VisitDelegate of the URL.
  if (visit_delegate_)
    visit_delegate_->AddURL(url);

  URLRow row(url);
  row.set_title(title);
  row.set_visit_count(visit_count);
  row.set_typed_count(typed_count);
  row.set_last_visit(last_visit);
  row.set_hidden(hidden);

  URLRows rows;
  rows.push_back(row);

  ScheduleTask(PRIORITY_NORMAL,
               base::Bind(&HistoryBackend::AddPagesWithDetails,
                          history_backend_.get(), rows, visit_source));
}

void HistoryService::AddPagesWithDetails(const URLRows& info,
                                         VisitSource visit_source) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  // Inform the VisitDelegate of the URLs
  if (!info.empty() && visit_delegate_) {
    std::vector<GURL> urls;
    urls.reserve(info.size());
    for (const auto& row : info)
      urls.push_back(row.url());
    visit_delegate_->AddURLs(urls);
  }

  ScheduleTask(PRIORITY_NORMAL,
               base::Bind(&HistoryBackend::AddPagesWithDetails,
                          history_backend_.get(), info, visit_source));
}

base::CancelableTaskTracker::TaskId HistoryService::GetFavicons(
    const std::vector<GURL>& icon_urls,
    int icon_types,
    const std::vector<int>& desired_sizes,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::GetFavicons, history_backend_.get(),
                 icon_urls, icon_types, desired_sizes, results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetFaviconsForURL(
    const GURL& page_url,
    int icon_types,
    const std::vector<int>& desired_sizes,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::GetFaviconsForURL, history_backend_.get(),
                 page_url, icon_types, desired_sizes, results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetLargestFaviconForURL(
    const GURL& page_url,
    const std::vector<int>& icon_types,
    int minimum_size_in_pixels,
    const favicon_base::FaviconRawBitmapCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  favicon_base::FaviconRawBitmapResult* result =
      new favicon_base::FaviconRawBitmapResult();
  return tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::GetLargestFaviconForURL,
                 history_backend_.get(), page_url, icon_types,
                 minimum_size_in_pixels, result),
      base::Bind(&RunWithFaviconResult, callback, base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetFaviconForID(
    favicon_base::FaviconID favicon_id,
    int desired_size,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::GetFaviconForID, history_backend_.get(),
                 favicon_id, desired_size, results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

base::CancelableTaskTracker::TaskId
HistoryService::UpdateFaviconMappingsAndFetch(
    const GURL& page_url,
    const std::vector<GURL>& icon_urls,
    int icon_types,
    const std::vector<int>& desired_sizes,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::UpdateFaviconMappingsAndFetch,
                 history_backend_.get(), page_url, icon_urls, icon_types,
                 desired_sizes, results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

void HistoryService::MergeFavicon(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    const gfx::Size& pixel_size) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (history_client_ && !history_client_->CanAddURL(page_url))
    return;

  ScheduleTask(
      PRIORITY_NORMAL,
      base::Bind(&HistoryBackend::MergeFavicon, history_backend_.get(),
                 page_url, icon_url, icon_type, bitmap_data, pixel_size));
}

void HistoryService::SetFavicons(const GURL& page_url,
                                 favicon_base::IconType icon_type,
                                 const GURL& icon_url,
                                 const std::vector<SkBitmap>& bitmaps) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (history_client_ && !history_client_->CanAddURL(page_url))
    return;

  ScheduleTask(PRIORITY_NORMAL,
               base::Bind(&HistoryBackend::SetFavicons, history_backend_.get(),
                          page_url, icon_type, icon_url, bitmaps));
}

void HistoryService::SetFaviconsOutOfDateForPage(const GURL& page_url) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL,
               base::Bind(&HistoryBackend::SetFaviconsOutOfDateForPage,
                          history_backend_.get(), page_url));
}

void HistoryService::SetImportedFavicons(
    const favicon_base::FaviconUsageDataList& favicon_usage) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL,
               base::Bind(&HistoryBackend::SetImportedFavicons,
                          history_backend_.get(), favicon_usage));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryURL(
    const GURL& url,
    bool want_visits,
    const QueryURLCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  QueryURLResult* query_url_result = new QueryURLResult();
  return tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::QueryURL, history_backend_.get(), url,
                 want_visits, base::Unretained(query_url_result)),
      base::Bind(&RunWithQueryURLResult, callback,
                 base::Owned(query_url_result)));
}

// Statistics ------------------------------------------------------------------

base::CancelableTaskTracker::TaskId HistoryService::GetHistoryCount(
    const Time& begin_time,
    const Time& end_time,
    const GetHistoryCountCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  return tracker->PostTaskAndReplyWithResult(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::GetHistoryCount,
                 history_backend_.get(),
                 begin_time,
                 end_time),
      callback);
}

// Downloads -------------------------------------------------------------------

// Handle creation of a download by creating an entry in the history service's
// 'downloads' table.
void HistoryService::CreateDownload(
    const DownloadRow& create_info,
    const HistoryService::DownloadCreateCallback& callback) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  PostTaskAndReplyWithResult(thread_->task_runner().get(), FROM_HERE,
                             base::Bind(&HistoryBackend::CreateDownload,
                                        history_backend_.get(), create_info),
                             callback);
}

void HistoryService::GetNextDownloadId(const DownloadIdCallback& callback) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  PostTaskAndReplyWithResult(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::GetNextDownloadId, history_backend_.get()),
      callback);
}

// Handle queries for a list of all downloads in the history database's
// 'downloads' table.
void HistoryService::QueryDownloads(const DownloadQueryCallback& callback) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<DownloadRow>* rows = new std::vector<DownloadRow>();
  std::unique_ptr<std::vector<DownloadRow>> scoped_rows(rows);
  // Beware! The first Bind() does not simply |scoped_rows.get()| because
  // base::Passed(&scoped_rows) nullifies |scoped_rows|, and compilers do not
  // guarantee that the first Bind's arguments are evaluated before the second
  // Bind's arguments.
  thread_->task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&HistoryBackend::QueryDownloads, history_backend_.get(), rows),
      base::Bind(callback, base::Passed(&scoped_rows)));
}

// Handle updates for a particular download. This is a 'fire and forget'
// operation, so we don't need to be called back.
void HistoryService::UpdateDownload(const DownloadRow& data) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL, base::Bind(&HistoryBackend::UpdateDownload,
                                           history_backend_.get(), data));
}

void HistoryService::RemoveDownloads(const std::set<uint32_t>& ids) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_NORMAL, base::Bind(&HistoryBackend::RemoveDownloads,
                                           history_backend_.get(), ids));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryHistory(
    const base::string16& text_query,
    const QueryOptions& options,
    const QueryHistoryCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  QueryResults* query_results = new QueryResults();
  return tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::QueryHistory, history_backend_.get(),
                 text_query, options, base::Unretained(query_results)),
      base::Bind(callback, base::Owned(query_results)));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryRedirectsFrom(
    const GURL& from_url,
    const QueryRedirectsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  RedirectList* result = new RedirectList();
  return tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::QueryRedirectsFrom, history_backend_.get(),
                 from_url, base::Unretained(result)),
      base::Bind(callback, base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryRedirectsTo(
    const GURL& to_url,
    const QueryRedirectsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  RedirectList* result = new RedirectList();
  return tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::QueryRedirectsTo, history_backend_.get(),
                 to_url, base::Unretained(result)),
      base::Bind(callback, base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetVisibleVisitCountToHost(
    const GURL& url,
    const GetVisibleVisitCountToHostCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  VisibleVisitCountToHostResult* result = new VisibleVisitCountToHostResult();
  return tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::GetVisibleVisitCountToHost,
                 history_backend_.get(), url, base::Unretained(result)),
      base::Bind(&RunWithVisibleVisitCountToHostResult, callback,
                 base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryMostVisitedURLs(
    int result_count,
    int days_back,
    const QueryMostVisitedURLsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  MostVisitedURLList* result = new MostVisitedURLList();
  return tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::QueryMostVisitedURLs, history_backend_.get(),
                 result_count, days_back, base::Unretained(result)),
      base::Bind(callback, base::Owned(result)));
}

void HistoryService::Cleanup() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!thread_) {
    // We've already cleaned up.
    return;
  }

  NotifyHistoryServiceBeingDeleted();

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Inform the HistoryClient that we are shuting down.
  if (history_client_)
    history_client_->Shutdown();

  // Unload the backend.
  if (history_backend_.get()) {
    // Get rid of the in-memory backend.
    in_memory_backend_.reset();

    // The backend's destructor must run on the history thread since it is not
    // threadsafe. So this thread must not be the last thread holding a
    // reference to the backend, or a crash could happen.
    //
    // We have a reference to the history backend. There is also an extra
    // reference held by our delegate installed in the backend, which
    // HistoryBackend::Closing will release. This means if we scheduled a call
    // to HistoryBackend::Closing and *then* released our backend reference,
    // there will be a race between us and the backend's Closing function to see
    // who is the last holder of a reference. If the backend thread's Closing
    // manages to run before we release our backend refptr, the last reference
    // will be held by this thread and the destructor will be called from here.
    //
    // Therefore, we create a closure to run the Closing operation first. This
    // holds a reference to the backend. Then we release our reference, then we
    // schedule the task to run. After the task runs, it will delete its
    // reference from the history thread, ensuring everything works properly.
    //
    // TODO(ajwong): Cleanup HistoryBackend lifetime issues.
    //     See http://crbug.com/99767.
    history_backend_->AddRef();
    base::Closure closing_task =
        base::Bind(&HistoryBackend::Closing, history_backend_.get());
    ScheduleTask(PRIORITY_NORMAL, closing_task);
    closing_task.Reset();
    HistoryBackend* raw_ptr = history_backend_.get();
    history_backend_ = nullptr;
    thread_->task_runner()->ReleaseSoon(FROM_HERE, raw_ptr);
  }

  // Delete the thread, which joins with the background thread. We defensively
  // nullptr the pointer before deleting it in case somebody tries to use it
  // during shutdown, but this shouldn't happen.
  base::Thread* thread = thread_;
  thread_ = nullptr;
  delete thread;
}

bool HistoryService::Init(
    bool no_db,
    const HistoryDatabaseParams& history_database_params) {
  TRACE_EVENT0("browser,startup", "HistoryService::Init")
  SCOPED_UMA_HISTOGRAM_TIMER("History.HistoryServiceInitTime");
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  base::Thread::Options options;
  options.timer_slack = base::TIMER_SLACK_MAXIMUM;
  if (!thread_->StartWithOptions(options)) {
    Cleanup();
    return false;
  }

  // Create the history backend.
  scoped_refptr<HistoryBackend> backend(new HistoryBackend(
      new BackendDelegate(weak_ptr_factory_.GetWeakPtr(),
                          base::ThreadTaskRunnerHandle::Get()),
      history_client_ ? history_client_->CreateBackendClient() : nullptr,
      thread_->task_runner()));
  history_backend_.swap(backend);

  ScheduleTask(PRIORITY_UI,
               base::Bind(&HistoryBackend::Init, history_backend_.get(),
                          no_db, history_database_params));

  if (visit_delegate_ && !visit_delegate_->Init(this))
    return false;

  if (history_client_)
    history_client_->OnHistoryServiceCreated(this);

  return true;
}

void HistoryService::ScheduleAutocomplete(
    const base::Callback<void(HistoryBackend*, URLDatabase*)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleTask(PRIORITY_UI, base::Bind(&HistoryBackend::ScheduleAutocomplete,
                                       history_backend_.get(), callback));
}

void HistoryService::ScheduleTask(SchedulePriority priority,
                                  const base::Closure& task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(thread_);
  CHECK(thread_->message_loop());
  // TODO(brettw): Do prioritization.
  thread_->task_runner()->PostTask(FROM_HERE, task);
}

base::WeakPtr<HistoryService> HistoryService::AsWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

syncer::SyncMergeResult HistoryService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::HISTORY_DELETE_DIRECTIVES);
  delete_directive_handler_.Start(this, initial_sync_data,
                                  std::move(sync_processor));
  return syncer::SyncMergeResult(type);
}

void HistoryService::StopSyncing(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::HISTORY_DELETE_DIRECTIVES);
  delete_directive_handler_.Stop();
}

syncer::SyncDataList HistoryService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::HISTORY_DELETE_DIRECTIVES);
  // TODO(akalin): Keep track of existing delete directives.
  return syncer::SyncDataList();
}

syncer::SyncError HistoryService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  delete_directive_handler_.ProcessSyncChanges(this, change_list);
  return syncer::SyncError();
}

syncer::SyncError HistoryService::ProcessLocalDeleteDirective(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return delete_directive_handler_.ProcessLocalDeleteDirective(
      delete_directive);
}

void HistoryService::SetInMemoryBackend(
    std::unique_ptr<InMemoryHistoryBackend> mem_backend) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!in_memory_backend_) << "Setting mem DB twice";
  in_memory_backend_.reset(mem_backend.release());

  // The database requires additional initialization once we own it.
  in_memory_backend_->AttachToHistoryService(this);
}

void HistoryService::NotifyProfileError(sql::InitStatus init_status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (history_client_)
    history_client_->NotifyProfileError(init_status);
}

void HistoryService::DeleteURL(const GURL& url) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  // We will update the visited links when we observe the delete notifications.
  ScheduleTask(PRIORITY_NORMAL, base::Bind(&HistoryBackend::DeleteURL,
                                           history_backend_.get(), url));
}

void HistoryService::DeleteURLsForTest(const std::vector<GURL>& urls) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  // We will update the visited links when we observe the delete
  // notifications.
  ScheduleTask(PRIORITY_NORMAL, base::Bind(&HistoryBackend::DeleteURLs,
                                           history_backend_.get(), urls));
}

void HistoryService::ExpireHistoryBetween(
    const std::set<GURL>& restrict_urls,
    Time begin_time,
    Time end_time,
    const base::Closure& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::ExpireHistoryBetween, history_backend_,
                 restrict_urls, begin_time, end_time),
      callback);
}

void HistoryService::ExpireHistory(
    const std::vector<ExpireHistoryArgs>& expire_list,
    const base::Closure& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  tracker->PostTaskAndReply(
      thread_->task_runner().get(), FROM_HERE,
      base::Bind(&HistoryBackend::ExpireHistory, history_backend_, expire_list),
      callback);
}

void HistoryService::ExpireLocalAndRemoteHistoryBetween(
    WebHistoryService* web_history,
    const std::set<GURL>& restrict_urls,
    Time begin_time,
    Time end_time,
    const base::Closure& callback,
    base::CancelableTaskTracker* tracker) {
  // TODO(dubroy): This should be factored out into a separate class that
  // dispatches deletions to the proper places.
  if (web_history) {
    // TODO(dubroy): This API does not yet support deletion of specific URLs.
    DCHECK(restrict_urls.empty());

    delete_directive_handler_.CreateDeleteDirectives(std::set<int64_t>(),
                                                     begin_time, end_time);

    // Attempt online deletion from the history server, but ignore the result.
    // Deletion directives ensure that the results will eventually be deleted.
    //
    // TODO(davidben): |callback| should not run until this operation completes
    // too.
    web_history->ExpireHistoryBetween(restrict_urls, begin_time, end_time,
                                      base::Bind(&ExpireWebHistoryComplete));
  }
  ExpireHistoryBetween(restrict_urls, begin_time, end_time, callback, tracker);
}

void HistoryService::OnDBLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  backend_loaded_ = true;
  NotifyHistoryServiceLoaded();
}

void HistoryService::NotifyURLVisited(ui::PageTransition transition,
                                      const URLRow& row,
                                      const RedirectList& redirects,
                                      base::Time visit_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(HistoryServiceObserver, observers_,
                    OnURLVisited(this, transition, row, redirects, visit_time));
}

void HistoryService::NotifyURLsModified(const URLRows& changed_urls) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(HistoryServiceObserver, observers_,
                    OnURLsModified(this, changed_urls));
}

void HistoryService::NotifyURLsDeleted(bool all_history,
                                       bool expired,
                                       const URLRows& deleted_rows,
                                       const std::set<GURL>& favicon_urls) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!thread_)
    return;

  // Inform the VisitDelegate of the deleted URLs. We will inform the delegate
  // of added URLs as soon as we get the add notification (we don't have to wait
  // for the backend, which allows us to be faster to update the state).
  //
  // For deleted URLs, we don't typically know what will be deleted since
  // delete notifications are by time. We would also like to be more
  // respectful of privacy and never tell the user something is gone when it
  // isn't. Therefore, we update the delete URLs after the fact.
  if (visit_delegate_) {
    if (all_history) {
      visit_delegate_->DeleteAllURLs();
    } else {
      std::vector<GURL> urls;
      urls.reserve(deleted_rows.size());
      for (const auto& row : deleted_rows)
        urls.push_back(row.url());
      visit_delegate_->DeleteURLs(urls);
    }
  }

  FOR_EACH_OBSERVER(
      HistoryServiceObserver, observers_,
      OnURLsDeleted(this, all_history, expired, deleted_rows, favicon_urls));
}

void HistoryService::NotifyHistoryServiceLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(HistoryServiceObserver, observers_,
                    OnHistoryServiceLoaded(this));
}

void HistoryService::NotifyHistoryServiceBeingDeleted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(HistoryServiceObserver, observers_,
                    HistoryServiceBeingDeleted(this));
}

void HistoryService::NotifyKeywordSearchTermUpdated(
    const URLRow& row,
    KeywordID keyword_id,
    const base::string16& term) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(HistoryServiceObserver, observers_,
                    OnKeywordSearchTermUpdated(this, row, keyword_id, term));
}

void HistoryService::NotifyKeywordSearchTermDeleted(URLID url_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(HistoryServiceObserver, observers_,
                    OnKeywordSearchTermDeleted(this, url_id));
}

std::unique_ptr<
    base::CallbackList<void(const std::set<GURL>&, const GURL&)>::Subscription>
HistoryService::AddFaviconsChangedCallback(
    const HistoryService::OnFaviconsChangedCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return favicon_changed_callback_list_.Add(callback);
}

void HistoryService::NotifyFaviconsChanged(const std::set<GURL>& page_urls,
                                           const GURL& icon_url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  favicon_changed_callback_list_.Notify(page_urls, icon_url);
}

}  // namespace history
