// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "url/gurl.h"

namespace page_load_metrics {

// This enum represents how a page load ends. If the action occurs before the
// page load finishes (or reaches some point like first paint), then we consider
// the load to be aborted.
enum UserAbortType {
  // Represents no abort.
  ABORT_NONE,

  // If the user presses reload or shift-reload.
  ABORT_RELOAD,

  // The user presses the back/forward button.
  ABORT_FORWARD_BACK,

  // If the navigation is replaced by a new navigation. This includes link
  // clicks, typing in the omnibox (not a reload), and form submissions.
  ABORT_NEW_NAVIGATION,

  // If the user presses the stop X button.
  ABORT_STOP,

  // If the navigation is aborted by closing the tab or browser.
  ABORT_CLOSE,

  // We don't know why the navigation aborted. This is the value we assign to an
  // aborted load if the only signal we get is a provisional load finishing
  // without committing, either without error or with net::ERR_ABORTED.
  ABORT_OTHER,

  // The load aborted due to another navigation, but it isn't clear what type of
  // navigation it was.
  ABORT_UNKNOWN_NAVIGATION,

  // Add values before this final count.
  ABORT_LAST_ENTRY
};

struct PageLoadExtraInfo {
  PageLoadExtraInfo(
      const base::Optional<base::TimeDelta>& first_background_time,
      const base::Optional<base::TimeDelta>& first_foreground_time,
      bool started_in_foreground,
      const GURL& committed_url,
      const base::Optional<base::TimeDelta>& time_to_commit,
      UserAbortType abort_type,
      const base::Optional<base::TimeDelta>& time_to_abort,
      const PageLoadMetadata& metadata);

  PageLoadExtraInfo(const PageLoadExtraInfo& other);

  ~PageLoadExtraInfo();

  // The first time that the page was backgrounded since the navigation started.
  const base::Optional<base::TimeDelta> first_background_time;

  // The first time that the page was foregrounded since the navigation started.
  const base::Optional<base::TimeDelta> first_foreground_time;

  // True if the page load started in the foreground.
  const bool started_in_foreground;

  // Committed URL. If the page load did not commit, |committed_url| will be
  // empty.
  const GURL committed_url;

  // Time from navigation start until commit.
  const base::Optional<base::TimeDelta> time_to_commit;

  // The abort time and time to abort for this page load. If the page was not
  // aborted, |abort_type| will be |ABORT_NONE|.
  const UserAbortType abort_type;
  const base::Optional<base::TimeDelta> time_to_abort;

  // Extra information supplied to the page load metrics system from the
  // renderer.
  const PageLoadMetadata metadata;
};

// Interface for PageLoadMetrics observers. All instances of this class are
// owned by the PageLoadTracker tracking a page load. They will be deleted after
// calling OnComplete.
class PageLoadMetricsObserver {
 public:
  virtual ~PageLoadMetricsObserver() {}

  // The page load started, with the given navigation handle. Note that OnStart
  // is called for same-page navigations. Implementers of OnStart that only want
  // to process non-same-page navigations should also check to see that the page
  // load committed via OnCommit or committed_url in
  // PageLoadExtraInfo. currently_committed_url contains the URL of the
  // committed page load at the time the navigation for navigation_handle was
  // initiated, or the empty URL if there was no committed page load at the time
  // the navigation was initiated.
  virtual void OnStart(content::NavigationHandle* navigation_handle,
                       const GURL& currently_committed_url,
                       bool started_in_foreground) {}

  // OnRedirect is triggered when a page load redirects to another URL.
  // The navigation handle holds relevant data for the navigation, but will
  // be destroyed soon after this call. Don't hold a reference to it. This can
  // be called multiple times.
  virtual void OnRedirect(content::NavigationHandle* navigation_handle) {}

  // OnCommit is triggered when a page load commits, i.e. when we receive the
  // first data for the request. The navigation handle holds relevant data for
  // the navigation, but will be destroyed soon after this call. Don't hold a
  // reference to it.
  // Note that this does not get called for same page navigations.
  virtual void OnCommit(content::NavigationHandle* navigation_handle) {}

  // OnFailedProvisionalLoad is triggered when a provisional load failed and did
  // not commit. Note that provisional loads that result in downloads or 204s
  // are aborted by the system, and thus considered failed provisional loads.
  virtual void OnFailedProvisionalLoad(
      content::NavigationHandle* navigation_handle) {}

  // OnTimingUpdate is triggered when an updated PageLoadTiming is
  // available. This method may be called multiple times over the course of the
  // page load. Note that this is currently an experimental API which may be
  // removed in the future. Please email loading-dev@chromium.org if you intend
  // to override this method.
  virtual void OnTimingUpdate(const PageLoadTiming& timing,
                              const PageLoadExtraInfo& extra_info) {}

  // OnHidden is triggered when a page leaves the foreground. It does not fire
  // when a foreground page is permanently closed; for that, listen to
  // OnComplete instead.
  virtual void OnHidden() {}

  // OnShown is triggered when a page is brought to the foreground. It does not
  // fire when the page first loads; for that, listen for OnStart instead.
  virtual void OnShown() {}

  // OnComplete is triggered when we are ready to record metrics for this page
  // load. This will happen some time after commit. The PageLoadTiming struct
  // contains timing data and the PageLoadExtraInfo struct contains other useful
  // data collected over the course of the page load. If the load did not
  // receive any timing information, |timing.IsEmpty()| will be true.
  // After this call, the object will be deleted.
  virtual void OnComplete(const PageLoadTiming& timing,
                          const PageLoadExtraInfo& extra_info) {}

  // OnUserInput is triggered when a new user input is passed in to
  // web_contents. Contains a TimeDelta from navigation start.
  virtual void OnUserInput(const blink::WebInputEvent& event) {}

  // The following methods are invoked at most once, when the timing for the
  // associated event first becomes available.
  virtual void OnDomContentLoadedEventStart(
      const PageLoadTiming& timing,
      const PageLoadExtraInfo& extra_info) {}
  virtual void OnLoadEventStart(const PageLoadTiming& timing,
                                const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstLayout(const PageLoadTiming& timing,
                             const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstPaint(const PageLoadTiming& timing,
                            const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstTextPaint(const PageLoadTiming& timing,
                                const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstImagePaint(const PageLoadTiming& timing,
                                 const PageLoadExtraInfo& extra_info) {}
  virtual void OnFirstContentfulPaint(const PageLoadTiming& timing,
                                      const PageLoadExtraInfo& extra_info) {}
  virtual void OnParseStart(const PageLoadTiming& timing,
                            const PageLoadExtraInfo& extra_info) {}
  virtual void OnParseStop(const PageLoadTiming& timing,
                           const PageLoadExtraInfo& extra_info) {}

  // Observer method to be invoked when there is a change in PageLoadMetadata's
  // behavior_flags.
  virtual void OnLoadingBehaviorObserved(
      const page_load_metrics::PageLoadExtraInfo& extra_info) {}
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_PAGE_LOAD_METRICS_OBSERVER_H_
