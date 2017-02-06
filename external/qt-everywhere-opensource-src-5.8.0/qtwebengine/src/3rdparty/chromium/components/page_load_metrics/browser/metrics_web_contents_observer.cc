// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/page_transition_types.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    page_load_metrics::MetricsWebContentsObserver);

namespace page_load_metrics {

namespace internal {

const char kErrorEvents[] = "PageLoad.Events.InternalError";
const char kAbortChainSizeReload[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.Reload";
const char kAbortChainSizeForwardBack[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.ForwardBack";
const char kAbortChainSizeNewNavigation[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.NewNavigation";
const char kAbortChainSizeSameURL[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.SameURL";
const char kAbortChainSizeNoCommit[] =
    "PageLoad.Internal.ProvisionalAbortChainSize.NoCommit";
const char kClientRedirectDelayAfterPaint[] =
    "PageLoad.Internal.ClientRedirectDelayAfterPaint";

}  // namespace internal

namespace {

// The url we see from the renderer side is not always the same as what
// we see from the browser side (e.g. chrome://newtab). We want to be
// sure here that we aren't logging UMA for internal pages.
bool IsRelevantNavigation(content::NavigationHandle* navigation_handle,
                          const GURL& browser_url,
                          const std::string& mime_type) {
  DCHECK(navigation_handle->HasCommitted());
  return navigation_handle->IsInMainFrame() &&
         !navigation_handle->IsSamePage() &&
         !navigation_handle->IsErrorPage() &&
         navigation_handle->GetURL().SchemeIsHTTPOrHTTPS() &&
         browser_url.SchemeIsHTTPOrHTTPS() &&
         (mime_type == "text/html" || mime_type == "application/xhtml+xml");
}

// If second is non-zero, first must also be non-zero and less than or equal to
// second.
bool EventsInOrder(base::TimeDelta first, base::TimeDelta second) {
  if (second.is_zero()) {
    return true;
  }
  return !first.is_zero() && first <= second;
}

bool IsValidPageLoadTiming(const PageLoadTiming& timing) {
  if (timing.IsEmpty())
    return false;

  // If we have a non-empty timing, it should always have a navigation start.
  if (timing.navigation_start.is_null()) {
    NOTREACHED() << "Received null navigation_start.";
    return false;
  }

  // Verify proper ordering between the various timings.

  if (!EventsInOrder(timing.response_start, timing.dom_loading)) {
    // We sometimes get a zero response_start with a non-zero DOM loading. See
    // crbug.com/590212.
    DLOG(ERROR) << "Invalid response_start " << timing.response_start
                << " for dom_loading " << timing.dom_loading;
    return false;
  }

  if (!EventsInOrder(timing.response_start, timing.parse_start)) {
    // We sometimes get a zero response_start with a non-zero parse start. See
    // crbug.com/590212.
    DLOG(ERROR) << "Invalid response_start " << timing.response_start
                << " for parse_start " << timing.parse_start;
    return false;
  }

  if (!EventsInOrder(timing.parse_start, timing.parse_stop)) {
    NOTREACHED() << "Invalid parse_start " << timing.parse_start
                 << " for parse_stop " << timing.parse_stop;
    return false;
  }

  if (!timing.parse_stop.is_zero()) {
    const base::TimeDelta parse_duration =
        timing.parse_stop - timing.parse_start;
    if (timing.parse_blocked_on_script_load_duration > parse_duration) {
      NOTREACHED() << "Invalid parse_blocked_on_script_load_duration "
                   << timing.parse_blocked_on_script_load_duration
                   << " for parse duration " << parse_duration;
      return false;
    }
  }

  if (timing.parse_blocked_on_script_load_from_document_write_duration >
      timing.parse_blocked_on_script_load_duration) {
    NOTREACHED()
        << "Invalid parse_blocked_on_script_load_from_document_write_duration "
        << timing.parse_blocked_on_script_load_from_document_write_duration
        << " for parse_blocked_on_script_load_duration "
        << timing.parse_blocked_on_script_load_duration;
    return false;
  }

  if (!EventsInOrder(timing.dom_loading,
                     timing.dom_content_loaded_event_start)) {
    NOTREACHED() << "Invalid dom_loading " << timing.dom_loading
                 << " for dom_content_loaded_event_start "
                 << timing.dom_content_loaded_event_start;
    return false;
  }

  if (!EventsInOrder(timing.dom_content_loaded_event_start,
                     timing.load_event_start)) {
    NOTREACHED() << "Invalid dom_content_loaded_event_start "
                 << timing.dom_content_loaded_event_start
                 << " for load_event_start " << timing.load_event_start;
    return false;
  }

  if (!EventsInOrder(timing.dom_loading, timing.first_layout)) {
    NOTREACHED() << "Invalid dom_loading " << timing.dom_loading
                 << " for first_layout " << timing.first_layout;
    return false;
  }

  if (!EventsInOrder(timing.first_layout, timing.first_paint)) {
    NOTREACHED() << "Invalid first_layout " << timing.first_layout
                 << " for first_paint " << timing.first_paint;
    return false;
  }

  if (!EventsInOrder(timing.first_paint, timing.first_text_paint)) {
    NOTREACHED() << "Invalid first_paint " << timing.first_paint
                 << " for first_text_paint " << timing.first_text_paint;
    return false;
  }

  if (!EventsInOrder(timing.first_paint, timing.first_image_paint)) {
    NOTREACHED() << "Invalid first_paint " << timing.first_paint
                 << " for first_image_paint " << timing.first_image_paint;
    return false;
  }

  if (!EventsInOrder(timing.first_paint, timing.first_contentful_paint)) {
    NOTREACHED() << "Invalid first_paint " << timing.first_paint
                 << " for first_contentful_paint "
                 << timing.first_contentful_paint;
    return false;
  }

  return true;
}

void RecordInternalError(InternalErrorLoadEvent event) {
  UMA_HISTOGRAM_ENUMERATION(internal::kErrorEvents, event, ERR_LAST_ENTRY);
}

UserAbortType AbortTypeForPageTransition(ui::PageTransition transition) {
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD))
    return ABORT_RELOAD;
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK)
    return ABORT_FORWARD_BACK;
  if (ui::PageTransitionIsNewNavigation(transition))
    return ABORT_NEW_NAVIGATION;
  NOTREACHED()
      << "AbortTypeForPageTransition received unexpected ui::PageTransition: "
      << transition;
  return ABORT_OTHER;
}

void LogAbortChainSameURLHistogram(int aborted_chain_size_same_url) {
  if (aborted_chain_size_same_url > 0) {
    UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeSameURL,
                         aborted_chain_size_same_url);
  }
}

void DispatchObserverTimingCallbacks(PageLoadMetricsObserver* observer,
                                     const PageLoadTiming& last_timing,
                                     const PageLoadTiming& new_timing,
                                     const PageLoadMetadata& last_metadata,
                                     const PageLoadExtraInfo& extra_info) {
  observer->OnTimingUpdate(new_timing, extra_info);
  if (!new_timing.dom_content_loaded_event_start.is_zero() &&
      last_timing.dom_content_loaded_event_start.is_zero())
    observer->OnDomContentLoadedEventStart(new_timing, extra_info);
  if (!new_timing.load_event_start.is_zero() &&
      last_timing.load_event_start.is_zero())
    observer->OnLoadEventStart(new_timing, extra_info);
  if (!new_timing.first_layout.is_zero() && last_timing.first_layout.is_zero())
    observer->OnFirstLayout(new_timing, extra_info);
  if (!new_timing.first_paint.is_zero() && last_timing.first_paint.is_zero())
    observer->OnFirstPaint(new_timing, extra_info);
  if (!new_timing.first_text_paint.is_zero() &&
      last_timing.first_text_paint.is_zero())
    observer->OnFirstTextPaint(new_timing, extra_info);
  if (!new_timing.first_image_paint.is_zero() &&
      last_timing.first_image_paint.is_zero())
    observer->OnFirstImagePaint(new_timing, extra_info);
  if (!new_timing.first_contentful_paint.is_zero() &&
      last_timing.first_contentful_paint.is_zero())
    observer->OnFirstContentfulPaint(new_timing, extra_info);
  if (!new_timing.parse_start.is_zero() && last_timing.parse_start.is_zero())
    observer->OnParseStart(new_timing, extra_info);
  if (!new_timing.parse_stop.is_zero() && last_timing.parse_stop.is_zero())
    observer->OnParseStop(new_timing, extra_info);
  if (extra_info.metadata.behavior_flags != last_metadata.behavior_flags)
    observer->OnLoadingBehaviorObserved(extra_info);
}

}  // namespace

PageLoadTracker::PageLoadTracker(
    bool in_foreground,
    PageLoadMetricsEmbedderInterface* embedder_interface,
    const GURL& currently_committed_url,
    content::NavigationHandle* navigation_handle,
    int aborted_chain_size,
    int aborted_chain_size_same_url)
    : renderer_tracked_(false),
      navigation_start_(navigation_handle->NavigationStart()),
      url_(navigation_handle->GetURL()),
      abort_type_(ABORT_NONE),
      started_in_foreground_(in_foreground),
      aborted_chain_size_(aborted_chain_size),
      aborted_chain_size_same_url_(aborted_chain_size_same_url),
      embedder_interface_(embedder_interface) {
  DCHECK(!navigation_handle->HasCommitted());
  embedder_interface_->RegisterObservers(this);
  for (const auto& observer : observers_) {
    observer->OnStart(navigation_handle, currently_committed_url,
                      started_in_foreground_);
  }
}

PageLoadTracker::~PageLoadTracker() {
  const PageLoadExtraInfo info = ComputePageLoadExtraInfo();

  if (info.time_to_commit && renderer_tracked() && timing_.IsEmpty()) {
    RecordInternalError(ERR_NO_IPCS_RECEIVED);
  }
  // Recall that trackers that are given ABORT_UNKNOWN_NAVIGATION have their
  // chain length added to the next navigation. Take care not to double count
  // them. Also do not double count committed loads, which call this already.
  if (commit_time_.is_null() && abort_type_ != ABORT_UNKNOWN_NAVIGATION)
    LogAbortChainHistograms(nullptr);

  for (const auto& observer : observers_) {
    observer->OnComplete(timing_, info);
  }
}

void PageLoadTracker::LogAbortChainHistograms(
    content::NavigationHandle* final_navigation) {
  if (aborted_chain_size_ == 0)
    return;
  // Note that this could be broken out by this navigation's abort type, if more
  // granularity is needed. Add one to the chain size to count the current
  // navigation. In the other cases, the current navigation is the final
  // navigation (which commits).
  if (!final_navigation) {
    UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeNoCommit,
                         aborted_chain_size_ + 1);
    LogAbortChainSameURLHistogram(aborted_chain_size_same_url_ + 1);
    return;
  }

  // The following is only executed for committing trackers.
  DCHECK(!commit_time_.is_null());

  // Note that histograms could be separated out by this commit's transition
  // type, but for simplicity they will all be bucketed together.
  LogAbortChainSameURLHistogram(aborted_chain_size_same_url_);

  ui::PageTransition committed_transition =
      final_navigation->GetPageTransition();
  switch (AbortTypeForPageTransition(committed_transition)) {
    case ABORT_RELOAD:
      UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeReload,
                           aborted_chain_size_);
      return;
    case ABORT_FORWARD_BACK:
      UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeForwardBack,
                           aborted_chain_size_);
      return;
    case ABORT_NEW_NAVIGATION:
      UMA_HISTOGRAM_COUNTS(internal::kAbortChainSizeNewNavigation,
                           aborted_chain_size_);
      return;
    default:
      NOTREACHED()
          << "LogAbortChainHistograms received unexpected ui::PageTransition: "
          << committed_transition;
      return;
  }
}

void PageLoadTracker::WebContentsHidden() {
  // Only log the first time we background in a given page load.
  if (background_time_.is_null()) {
    // Make sure we either started in the foreground and haven't been
    // foregrounded yet, or started in the background and have already been
    // foregrounded.
    DCHECK_EQ(started_in_foreground_, foreground_time_.is_null());
    background_time_ = base::TimeTicks::Now();
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&background_time_);
  }

  for (const auto& observer : observers_)
    observer->OnHidden();
}

void PageLoadTracker::WebContentsShown() {
  // Only log the first time we foreground in a given page load.
  if (foreground_time_.is_null()) {
    // Make sure we either started in the background and haven't been
    // backgrounded yet, or started in the foreground and have already been
    // backgrounded.
    DCHECK_NE(started_in_foreground_, background_time_.is_null());
    foreground_time_ = base::TimeTicks::Now();
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&foreground_time_);
  }

  for (const auto& observer : observers_)
    observer->OnShown();
}

void PageLoadTracker::Commit(content::NavigationHandle* navigation_handle) {
  // TODO(bmcquade): To improve accuracy, consider adding commit time to
  // NavigationHandle. Taking a timestamp here should be close enough for now.
  commit_time_ = base::TimeTicks::Now();
  ClampBrowserTimestampIfInterProcessTimeTickSkew(&commit_time_);
  url_ = navigation_handle->GetURL();
  for (const auto& observer : observers_) {
    observer->OnCommit(navigation_handle);
  }
  LogAbortChainHistograms(navigation_handle);
}

void PageLoadTracker::FailedProvisionalLoad(
    content::NavigationHandle* navigation_handle) {
  for (const auto& observer : observers_) {
    observer->OnFailedProvisionalLoad(navigation_handle);
  }
}

void PageLoadTracker::Redirect(content::NavigationHandle* navigation_handle) {
  for (const auto& observer : observers_) {
    observer->OnRedirect(navigation_handle);
  }
}

void PageLoadTracker::OnInputEvent(const blink::WebInputEvent& event) {
  for (const auto& observer : observers_) {
    observer->OnUserInput(event);
  }
}

void PageLoadTracker::NotifyClientRedirectTo(
    const PageLoadTracker& destination) {
  base::TimeDelta redirect_delay_after_paint;
  if (!timing_.first_paint.is_zero()) {
    base::TimeTicks first_paint_time = navigation_start() + timing_.first_paint;
    if (destination.navigation_start() > first_paint_time)
      redirect_delay_after_paint =
          destination.navigation_start() - first_paint_time;
  }
  PAGE_LOAD_HISTOGRAM(internal::kClientRedirectDelayAfterPaint,
                      redirect_delay_after_paint);
}

bool PageLoadTracker::UpdateTiming(const PageLoadTiming& new_timing,
                                   const PageLoadMetadata& new_metadata) {
  // Throw away IPCs that are not relevant to the current navigation.
  // Two timing structures cannot refer to the same navigation if they indicate
  // that a navigation started at different times, so a new timing struct with a
  // different start time from an earlier struct is considered invalid.
  bool valid_timing_descendent =
      timing_.navigation_start.is_null() ||
      timing_.navigation_start == new_timing.navigation_start;
  // Ensure flags sent previously are still present in the new metadata fields.
  bool valid_behavior_descendent =
      (metadata_.behavior_flags & new_metadata.behavior_flags) ==
      metadata_.behavior_flags;
  if (IsValidPageLoadTiming(new_timing) && valid_timing_descendent &&
      valid_behavior_descendent) {
    // There are some subtle ordering constraints here. GetPageLoadMetricsInfo()
    // must be called before DispatchObserverTimingCallbacks, but its
    // implementation depends on the state of metadata_, so we need to update
    // metadata_ before calling GetPageLoadMetricsInfo. Thus, we make a copy of
    // timing here, update timing_ and metadata_, and then proceed to dispatch
    // the observer timing callbacks.
    const PageLoadTiming last_timing = timing_;
    timing_ = new_timing;

    const PageLoadMetadata last_metadata = metadata_;
    metadata_ = new_metadata;
    const PageLoadExtraInfo info = ComputePageLoadExtraInfo();
    for (const auto& observer : observers_) {
      DispatchObserverTimingCallbacks(observer.get(), last_timing, new_timing,
                                      last_metadata, info);
    }
    return true;
  }
  return false;
}

void PageLoadTracker::set_renderer_tracked(bool renderer_tracked) {
  renderer_tracked_ = renderer_tracked;
}

void PageLoadTracker::AddObserver(
    std::unique_ptr<PageLoadMetricsObserver> observer) {
  observers_.push_back(std::move(observer));
}

void PageLoadTracker::ClampBrowserTimestampIfInterProcessTimeTickSkew(
    base::TimeTicks* event_time) {
  DCHECK(event_time != nullptr);
  // Windows 10 GCE bot non-deterministically failed because TimeTicks::Now()
  // called in the browser process e.g. commit_time was less than
  // navigation_start_ that was populated in the renderer process because the
  // clock was not system-wide monotonic.
  // Note that navigation_start_ can also be set in the browser process in
  // some cases and in those cases event_time should never be <
  // navigation_start_. If it is due to a code error and it gets clamped in this
  // function, on high resolution systems it should lead to a dcheck failure.

  // TODO (shivanisha) Currently IsHighResolution is the best way to check
  // if the clock is system-wide monotonic. However IsHighResolution
  // does a broader check to see if the clock in use is high resolution
  // which also implies it is system-wide monotonic (on Windows).
  if (base::TimeTicks::IsHighResolution()) {
    DCHECK(event_time->is_null() || *event_time >= navigation_start_);
    return;
  }

  if (!event_time->is_null() && *event_time < navigation_start_) {
    RecordInternalError(ERR_INTER_PROCESS_TIME_TICK_SKEW);
    *event_time = navigation_start_;
  }
}

PageLoadExtraInfo PageLoadTracker::ComputePageLoadExtraInfo() {
  base::Optional<base::TimeDelta> first_background_time;
  base::Optional<base::TimeDelta> first_foreground_time;
  base::Optional<base::TimeDelta> time_to_abort;
  base::Optional<base::TimeDelta> time_to_commit;

  if (!background_time_.is_null()) {
    DCHECK_GE(background_time_, navigation_start_);
    first_background_time = background_time_ - navigation_start_;
  }

  if (!foreground_time_.is_null()) {
    DCHECK_GE(foreground_time_, navigation_start_);
    first_foreground_time = foreground_time_ - navigation_start_;
  }

  if (abort_type_ != ABORT_NONE) {
    DCHECK_GE(abort_time_, navigation_start_);
    time_to_abort = abort_time_ - navigation_start_;
  } else {
    DCHECK(abort_time_.is_null());
  }

  if (!commit_time_.is_null()) {
    DCHECK_GE(commit_time_, navigation_start_);
    time_to_commit = commit_time_ - navigation_start_;
  }

  return PageLoadExtraInfo(
      first_background_time, first_foreground_time, started_in_foreground_,
      commit_time_.is_null() ? GURL() : url_, time_to_commit, abort_type_,
      time_to_abort, metadata_);
}

void PageLoadTracker::NotifyAbort(UserAbortType abort_type,
                                  base::TimeTicks timestamp,
                                  bool is_certainly_browser_timestamp) {
  DCHECK_NE(abort_type, ABORT_NONE);
  // Use UpdateAbort to update an already notified PageLoadTracker.
  if (abort_type_ != ABORT_NONE)
    return;

  UpdateAbortInternal(abort_type, timestamp, is_certainly_browser_timestamp);
}

void PageLoadTracker::UpdateAbort(UserAbortType abort_type,
                                  base::TimeTicks timestamp,
                                  bool is_certainly_browser_timestamp) {
  DCHECK_NE(abort_type, ABORT_NONE);
  DCHECK_NE(abort_type, ABORT_OTHER);
  DCHECK_EQ(abort_type_, ABORT_OTHER);

  // For some aborts (e.g. navigations), the initiated timestamp can be earlier
  // than the timestamp that aborted the load. Taking the minimum gives the
  // closest user initiated time known.
  UpdateAbortInternal(abort_type, std::min(abort_time_, timestamp),
                      is_certainly_browser_timestamp);
}

bool PageLoadTracker::IsLikelyProvisionalAbort(
    base::TimeTicks abort_cause_time) {
  // Note that |abort_cause_time - abort_time| can be negative.
  return abort_type_ == ABORT_OTHER &&
         (abort_cause_time - abort_time_).InMilliseconds() < 100;
}

bool PageLoadTracker::MatchesOriginalNavigation(
    content::NavigationHandle* navigation_handle) {
  // Neither navigation should have committed.
  DCHECK(!navigation_handle->HasCommitted());
  DCHECK(commit_time_.is_null());
  return navigation_handle->GetURL() == url_;
}

void PageLoadTracker::UpdateAbortInternal(UserAbortType abort_type,
                                          base::TimeTicks timestamp,
                                          bool is_certainly_browser_timestamp) {
  // When a provisional navigation commits, that navigation's start time is
  // interpreted as the abort time for other provisional loads in the tab.
  // However, this only makes sense if the committed load started after the
  // aborted provisional loads started. Thus we ignore cases where the committed
  // load started before the aborted provisional load, as this would result in
  // recording a negative time-to-abort. The real issue here is that we have to
  // infer the cause of aborts. It would be better if the navigation code could
  // instead report the actual cause of an aborted navigation. See crbug/571647
  // for details.
  if (timestamp < navigation_start_) {
    RecordInternalError(ERR_ABORT_BEFORE_NAVIGATION_START);
    abort_type_ = ABORT_NONE;
    abort_time_ = base::TimeTicks();
    return;
  }
  abort_type_ = abort_type;
  abort_time_ = timestamp;

  if (is_certainly_browser_timestamp) {
    ClampBrowserTimestampIfInterProcessTimeTickSkew(&abort_time_);
  }
}

// static
MetricsWebContentsObserver::MetricsWebContentsObserver(
    content::WebContents* web_contents,
    std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface)
    : content::WebContentsObserver(web_contents),
      in_foreground_(false),
      embedder_interface_(std::move(embedder_interface)),
      has_navigated_(false) {
  RegisterInputEventObserver(web_contents->GetRenderViewHost());
}

MetricsWebContentsObserver* MetricsWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface) {
  DCHECK(web_contents);

  MetricsWebContentsObserver* metrics = FromWebContents(web_contents);
  if (!metrics) {
    metrics = new MetricsWebContentsObserver(web_contents,
                                             std::move(embedder_interface));
    web_contents->SetUserData(UserDataKey(), metrics);
  }
  return metrics;
}

MetricsWebContentsObserver::~MetricsWebContentsObserver() {
  NotifyAbortAllLoads(ABORT_CLOSE);
}

void MetricsWebContentsObserver::RegisterInputEventObserver(
    content::RenderViewHost* host) {
  if (host != nullptr)
    host->GetWidget()->AddInputEventObserver(this);
}

void MetricsWebContentsObserver::UnregisterInputEventObserver(
    content::RenderViewHost* host) {
  if (host != nullptr)
    host->GetWidget()->RemoveInputEventObserver(this);
}

void MetricsWebContentsObserver::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  UnregisterInputEventObserver(old_host);
  RegisterInputEventObserver(new_host);
}

bool MetricsWebContentsObserver::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(MetricsWebContentsObserver, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(PageLoadMetricsMsg_TimingUpdated, OnTimingUpdated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MetricsWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;
  if (embedder_interface_->IsPrerendering(web_contents()))
    return;
  if (navigation_handle->GetURL().spec().compare(url::kAboutBlankURL) == 0)
    return;

  std::unique_ptr<PageLoadTracker> last_aborted =
      NotifyAbortedProvisionalLoadsNewNavigation(navigation_handle);

  int chain_size_same_url = 0;
  int chain_size = 0;
  if (last_aborted) {
    if (last_aborted->MatchesOriginalNavigation(navigation_handle)) {
      chain_size_same_url = last_aborted->aborted_chain_size_same_url() + 1;
    } else if (last_aborted->aborted_chain_size_same_url() > 0) {
      LogAbortChainSameURLHistogram(
          last_aborted->aborted_chain_size_same_url());
    }
    chain_size = last_aborted->aborted_chain_size() + 1;
  }

  // Pass in the last committed url to the PageLoadTracker. If the MWCO has
  // never observed a committed load, use the last committed url from this
  // WebContent's opener. This is more accurate than using referrers due to
  // referrer sanitizing and origin referrers. Note that this could potentially
  // be inaccurate if the opener has since navigated.
  content::WebContents* opener = web_contents()->GetOpener();
  const GURL& opener_url =
      !has_navigated_ && opener
          ? web_contents()->GetOpener()->GetLastCommittedURL()
          : GURL::EmptyGURL();
  const GURL& currently_committed_url =
      committed_load_ ? committed_load_->committed_url() : opener_url;
  has_navigated_ = true;

  // We can have two provisional loads in some cases. E.g. a same-site
  // navigation can have a concurrent cross-process navigation started
  // from the omnibox.
  DCHECK_GT(2ul, provisional_loads_.size());
  // Passing raw pointers to observers_ and embedder_interface_ is safe because
  // the MetricsWebContentsObserver owns them both list and they are torn down
  // after the PageLoadTracker. The PageLoadTracker does not hold on to
  // committed_load_ or navigation_handle beyond the scope of the constructor.
  provisional_loads_.insert(std::make_pair(
      navigation_handle,
      base::WrapUnique(new PageLoadTracker(
          in_foreground_, embedder_interface_.get(), currently_committed_url,
          navigation_handle, chain_size, chain_size_same_url))));
}

const PageLoadExtraInfo
MetricsWebContentsObserver::GetPageLoadExtraInfoForCommittedLoad() {
  DCHECK(committed_load_);
  return committed_load_->ComputePageLoadExtraInfo();
}

void MetricsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;

  std::unique_ptr<PageLoadTracker> finished_nav(
      std::move(provisional_loads_[navigation_handle]));
  provisional_loads_.erase(navigation_handle);

  // There's a chance a navigation could have started before we were added to a
  // tab. Bail out early if this is the case.
  if (!finished_nav)
    return;

  // Handle a pre-commit error here. Navigations that result in an error page
  // will be ignored. Note that downloads/204s will result in HasCommitted()
  // returning false.
  // TODO(csharrison): Track changes to NavigationHandle for signals when this
  // is the case (HTTP response headers).
  if (!navigation_handle->HasCommitted()) {
    finished_nav->FailedProvisionalLoad(navigation_handle);

    net::Error error = navigation_handle->GetNetErrorCode();

    // net::OK: This case occurs when the NavigationHandle finishes and reports
    // !HasCommitted(), but reports no net::Error. This should not occur
    // pre-PlzNavigate, but afterwards it should represent the navigation
    // stopped by the user before it was ready to commit.
    // net::ERR_ABORTED: An aborted provisional load has error net::ERR_ABORTED.
    // Note that this can come from some non user-initiated errors, such as
    // downloads, or 204 responses. See crbug.com/542369.
    if ((error == net::OK) || (error == net::ERR_ABORTED)) {
      finished_nav->NotifyAbort(ABORT_OTHER, base::TimeTicks::Now(), true);
      aborted_provisional_loads_.push_back(std::move(finished_nav));
    }

    return;
  }

  // Don't treat a same-page nav as a new page load.
  if (navigation_handle->IsSamePage())
    return;

  if (!navigation_handle->HasUserGesture() &&
      (navigation_handle->GetPageTransition() &
       ui::PAGE_TRANSITION_CLIENT_REDIRECT) != 0 &&
      committed_load_)
    committed_load_->NotifyClientRedirectTo(*finished_nav);

  // Notify other loads that they may have been aborted by this committed load.
  // Note that by using the committed navigation start as the abort cause, we
  // lose data on provisional loads that were aborted by other provisional
  // loads. Those will either be listed as ABORT_OTHER or as being aborted by
  // this load.
  // is_certainly_browser_timestamp is set to false because NavigationStart()
  // could be set in either the renderer or browser process.
  NotifyAbortAllLoadsWithTimestamp(
      AbortTypeForPageTransition(navigation_handle->GetPageTransition()),
      navigation_handle->NavigationStart(), false);

  committed_load_ = std::move(finished_nav);
  aborted_provisional_loads_.clear();

  const GURL& browser_url = web_contents()->GetLastCommittedURL();
  const std::string& mime_type = web_contents()->GetContentsMimeType();
  DCHECK(!browser_url.is_empty());
  DCHECK(!mime_type.empty());
  committed_load_->set_renderer_tracked(
      IsRelevantNavigation(navigation_handle, browser_url, mime_type));

  committed_load_->Commit(navigation_handle);
}

void MetricsWebContentsObserver::NavigationStopped() {
  NotifyAbortAllLoads(ABORT_STOP);
}

void MetricsWebContentsObserver::OnInputEvent(
    const blink::WebInputEvent& event) {
  // Ignore browser navigation or reload which comes with type Undefined.
  if (event.type == blink::WebInputEvent::Type::Undefined)
    return;

  if (!committed_load_) {
    RecordInternalError(ERR_USER_INPUT_WITH_NO_RELEVANT_LOAD);
    return;
  }

  committed_load_->OnInputEvent(event);
}

void MetricsWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;
  auto it = provisional_loads_.find(navigation_handle);
  if (it == provisional_loads_.end())
    return;
  it->second->Redirect(navigation_handle);
}

void MetricsWebContentsObserver::WasShown() {
  if (in_foreground_)
    return;
  in_foreground_ = true;
  if (committed_load_)
    committed_load_->WebContentsShown();
  for (const auto& kv : provisional_loads_) {
    kv.second->WebContentsShown();
  }
}

void MetricsWebContentsObserver::WasHidden() {
  if (!in_foreground_)
    return;
  in_foreground_ = false;
  if (committed_load_)
    committed_load_->WebContentsHidden();
  for (const auto& kv : provisional_loads_) {
    kv.second->WebContentsHidden();
  }
}

// This will occur when the process for the main RenderFrameHost exits, either
// normally or from a crash. We eagerly log data from the last committed load if
// we have one. Don't notify aborts here because this is probably not user
// initiated. If it is (e.g. browser shutdown), other code paths will take care
// of notifying.
void MetricsWebContentsObserver::RenderProcessGone(
    base::TerminationStatus status) {
  // Other code paths will be run for normal renderer shutdown. Note that we
  // sometimes get the STILL_RUNNING value on fast shutdown.
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_STILL_RUNNING) {
    return;
  }

  // If this is a crash, eagerly log the aborted provisional loads and the
  // committed load. |provisional_loads_| don't need to be destroyed here
  // because their lifetime is tied to the NavigationHandle.
  committed_load_.reset();
  aborted_provisional_loads_.clear();
}

void MetricsWebContentsObserver::NotifyAbortAllLoads(UserAbortType abort_type) {
  NotifyAbortAllLoadsWithTimestamp(abort_type, base::TimeTicks::Now(), true);
}

void MetricsWebContentsObserver::NotifyAbortAllLoadsWithTimestamp(
    UserAbortType abort_type,
    base::TimeTicks timestamp,
    bool is_certainly_browser_timestamp) {
  if (committed_load_)
    committed_load_->NotifyAbort(abort_type, timestamp,
                                 is_certainly_browser_timestamp);
  for (const auto& kv : provisional_loads_) {
    kv.second->NotifyAbort(abort_type, timestamp,
                           is_certainly_browser_timestamp);
  }
  for (const auto& tracker : aborted_provisional_loads_) {
    if (tracker->IsLikelyProvisionalAbort(timestamp))
      tracker->UpdateAbort(abort_type, timestamp,
                           is_certainly_browser_timestamp);
  }
  aborted_provisional_loads_.clear();
}

std::unique_ptr<PageLoadTracker>
MetricsWebContentsObserver::NotifyAbortedProvisionalLoadsNewNavigation(
    content::NavigationHandle* new_navigation) {
  // If there are multiple aborted loads that can be attributed to this one,
  // just count the latest one for simplicity. Other loads will fall into the
  // OTHER bucket, though there shouldn't be very many.
  if (aborted_provisional_loads_.size() == 0)
    return nullptr;
  if (aborted_provisional_loads_.size() > 1)
    RecordInternalError(ERR_NAVIGATION_SIGNALS_MULIPLE_ABORTED_LOADS);

  std::unique_ptr<PageLoadTracker> last_aborted_load =
      std::move(aborted_provisional_loads_.back());
  aborted_provisional_loads_.pop_back();

  base::TimeTicks timestamp = new_navigation->NavigationStart();
  if (last_aborted_load->IsLikelyProvisionalAbort(timestamp))
    last_aborted_load->UpdateAbort(ABORT_UNKNOWN_NAVIGATION, timestamp, false);

  aborted_provisional_loads_.clear();
  return last_aborted_load;
}

void MetricsWebContentsObserver::OnTimingUpdated(
    content::RenderFrameHost* render_frame_host,
    const PageLoadTiming& timing,
    const PageLoadMetadata& metadata) {
  bool error = false;
  if (!committed_load_ || !committed_load_->renderer_tracked()) {
    RecordInternalError(ERR_IPC_WITH_NO_RELEVANT_LOAD);
    error = true;
  }

  // We may receive notifications from frames that have been navigated away
  // from. We simply ignore them.
  if (render_frame_host != web_contents()->GetMainFrame()) {
    RecordInternalError(ERR_IPC_FROM_WRONG_FRAME);
    error = true;
  }

  // For urls like chrome://newtab, the renderer and browser disagree,
  // so we have to double check that the renderer isn't sending data from a
  // bad url like https://www.google.com/_/chrome/newtab.
  if (!web_contents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    RecordInternalError(ERR_IPC_FROM_BAD_URL_SCHEME);
    error = true;
  }

  if (error)
    return;

  if (!committed_load_->UpdateTiming(timing, metadata)) {
    // If the page load tracker cannot update its timing, something is wrong
    // with the IPC (it's from another load, or it's invalid in some other way).
    // We expect this to be a rare occurrence.
    RecordInternalError(ERR_BAD_TIMING_IPC);
  }
}

}  // namespace page_load_metrics
