// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/process/kill.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics_messages.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

const char kDefaultTestUrl[] = "https://google.com/";
const char kDefaultTestUrlAnchor[] = "https://google.com/#samepage";
const char kDefaultTestUrl2[] = "https://whatever.com/";

// Simple PageLoadMetricsObserver that copies observed PageLoadTimings into the
// provided std::vector, so they can be analyzed by unit tests.
class TestPageLoadMetricsObserver : public PageLoadMetricsObserver {
 public:
  explicit TestPageLoadMetricsObserver(
      std::vector<PageLoadTiming>* updated_timings,
      std::vector<PageLoadTiming>* complete_timings,
      std::vector<GURL>* observed_committed_urls)
      : updated_timings_(updated_timings),
        complete_timings_(complete_timings),
        observed_committed_urls_(observed_committed_urls) {}

  void OnStart(content::NavigationHandle* navigation_handle,
               const GURL& currently_committed_url,
               bool started_in_foreground) override {
    observed_committed_urls_->push_back(currently_committed_url);
  }

  void OnTimingUpdate(const PageLoadTiming& timing,
                      const PageLoadExtraInfo& extra_info) override {
    updated_timings_->push_back(timing);
  }

  void OnComplete(const PageLoadTiming& timing,
                  const PageLoadExtraInfo& extra_info) override {
    complete_timings_->push_back(timing);
  }

 private:
  std::vector<PageLoadTiming>* const updated_timings_;
  std::vector<PageLoadTiming>* const complete_timings_;
  std::vector<GURL>* const observed_committed_urls_;
};

class TestPageLoadMetricsEmbedderInterface
    : public PageLoadMetricsEmbedderInterface {
 public:
  TestPageLoadMetricsEmbedderInterface() : is_prerendering_(false) {}

  bool IsPrerendering(content::WebContents* web_contents) override {
    return is_prerendering_;
  }
  void set_is_prerendering(bool is_prerendering) {
    is_prerendering_ = is_prerendering;
  }
  void RegisterObservers(PageLoadTracker* tracker) override {
    tracker->AddObserver(base::WrapUnique(new TestPageLoadMetricsObserver(
        &updated_timings_, &complete_timings_, &observed_committed_urls_)));
  }
  const std::vector<PageLoadTiming>& updated_timings() const {
    return updated_timings_;
  }
  const std::vector<PageLoadTiming>& complete_timings() const {
    return complete_timings_;
  }

  // currently_committed_urls passed to OnStart().
  const std::vector<GURL>& observed_committed_urls_from_on_start() const {
    return observed_committed_urls_;
  }

 private:
  std::vector<PageLoadTiming> updated_timings_;
  std::vector<PageLoadTiming> complete_timings_;
  std::vector<GURL> observed_committed_urls_;
  bool is_prerendering_;
};

}  //  namespace

class MetricsWebContentsObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  MetricsWebContentsObserverTest() : num_errors_(0) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    AttachObserver();
  }

  void SimulateTimingUpdate(const PageLoadTiming& timing) {
    SimulateTimingUpdate(timing, web_contents()->GetMainFrame());
  }

  void SimulateTimingUpdate(const PageLoadTiming& timing,
                            content::RenderFrameHost* render_frame_host) {
    ASSERT_TRUE(observer_->OnMessageReceived(
        PageLoadMetricsMsg_TimingUpdated(observer_->routing_id(), timing,
                                         PageLoadMetadata()),
        render_frame_host));
  }

  void AttachObserver() {
    embedder_interface_ = new TestPageLoadMetricsEmbedderInterface();
    observer_.reset(new MetricsWebContentsObserver(
        web_contents(), base::WrapUnique(embedder_interface_)));
    observer_->WasShown();
  }

  void CheckErrorEvent(InternalErrorLoadEvent error, int count) {
    histogram_tester_.ExpectBucketCount(internal::kErrorEvents, error, count);
    num_errors_ += count;
  }

  void CheckTotalErrorEvents() {
    histogram_tester_.ExpectTotalCount(internal::kErrorEvents, num_errors_);
  }

  void CheckNoErrorEvents() {
    histogram_tester_.ExpectTotalCount(internal::kErrorEvents, 0);
  }

  void AssertNoNonEmptyTimingReported() {
    ASSERT_FALSE(embedder_interface_->complete_timings().empty());
    for (const auto& timing : embedder_interface_->complete_timings()) {
      ASSERT_TRUE(timing.IsEmpty());
    }
  }

  void AssertNonEmptyTimingsReported(size_t expected_non_empty_timings) {
    ASSERT_GE(embedder_interface_->complete_timings().size(),
              expected_non_empty_timings);
    size_t actual_non_empty_timings = 0;
    for (const auto& timing : embedder_interface_->complete_timings()) {
      if (!timing.IsEmpty()) {
        ++actual_non_empty_timings;
      }
    }
    ASSERT_EQ(expected_non_empty_timings, actual_non_empty_timings);
    ASSERT_GE(embedder_interface_->updated_timings().size(),
              actual_non_empty_timings);
  }

  int CountCompleteTimingReported() {
    return embedder_interface_->complete_timings().size();
  }
  int CountUpdatedTimingReported() {
    return embedder_interface_->updated_timings().size();
  }

  const std::vector<GURL>& observed_committed_urls_from_on_start() const {
    return embedder_interface_->observed_committed_urls_from_on_start();
  }

 protected:
  base::HistogramTester histogram_tester_;
  TestPageLoadMetricsEmbedderInterface* embedder_interface_;
  std::unique_ptr<MetricsWebContentsObserver> observer_;

 private:
  int num_errors_;

  DISALLOW_COPY_AND_ASSIGN(MetricsWebContentsObserverTest);
};

TEST_F(MetricsWebContentsObserverTest, SuccessfulMainFrameNavigation) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);
  timing.response_start = base::TimeDelta::FromMilliseconds(2);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  ASSERT_TRUE(observed_committed_urls_from_on_start().empty());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(1u, observed_committed_urls_from_on_start().size());
  ASSERT_TRUE(observed_committed_urls_from_on_start().at(0).is_empty());

  ASSERT_EQ(0, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  AssertNonEmptyTimingsReported(1);
  ASSERT_EQ(2u, observed_committed_urls_from_on_start().size());
  ASSERT_EQ(kDefaultTestUrl,
            observed_committed_urls_from_on_start().at(1).spec());
  ASSERT_EQ(1, CountUpdatedTimingReported());

  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, NotInMainFrame) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(subframe);
  subframe_tester->SimulateNavigationStart(GURL(kDefaultTestUrl2));
  subframe_tester->SimulateNavigationCommit(GURL(kDefaultTestUrl2));
  SimulateTimingUpdate(timing, subframe);
  subframe_tester->SimulateNavigationStop();

  // Navigate again to see if the timing updated for a subframe message.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  ASSERT_EQ(0, CountUpdatedTimingReported());
  AssertNoNonEmptyTimingReported();
  CheckErrorEvent(ERR_IPC_FROM_WRONG_FRAME, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, SamePageNoTrigger) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(0, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrlAnchor));
  // A same page navigation shouldn't trigger logging UMA for the original.
  ASSERT_EQ(1, CountUpdatedTimingReported());
  AssertNoNonEmptyTimingReported();
  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DontLogPrerender) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  embedder_interface_->set_is_prerendering(true);

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  SimulateTimingUpdate(timing);
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DontLogIrrelevantNavigation) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(10);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());

  GURL about_blank_url = GURL("about:blank");
  web_contents_tester->NavigateAndCommit(about_blank_url);
  SimulateTimingUpdate(timing);
  ASSERT_EQ(0, CountUpdatedTimingReported());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());

  CheckErrorEvent(ERR_IPC_FROM_BAD_URL_SCHEME, 1);
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, NotInMainError) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(1);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");

  content::RenderFrameHostTester* subframe_tester =
      content::RenderFrameHostTester::For(subframe);
  subframe_tester->SimulateNavigationStart(GURL(kDefaultTestUrl2));
  subframe_tester->SimulateNavigationCommit(GURL(kDefaultTestUrl2));
  SimulateTimingUpdate(timing, subframe);
  CheckErrorEvent(ERR_IPC_FROM_WRONG_FRAME, 1);
  CheckTotalErrorEvents();
  ASSERT_EQ(0, CountUpdatedTimingReported());
  ASSERT_EQ(0, CountCompleteTimingReported());
}

TEST_F(MetricsWebContentsObserverTest, BadIPC) {
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(10);
  PageLoadTiming timing2;
  timing2.navigation_start = base::Time::FromDoubleT(100);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));

  SimulateTimingUpdate(timing);
  ASSERT_EQ(1, CountUpdatedTimingReported());
  SimulateTimingUpdate(timing2);
  ASSERT_EQ(1, CountUpdatedTimingReported());

  CheckErrorEvent(ERR_BAD_TIMING_IPC, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, ObservePartialNavigation) {
  // Delete the observer for this test, add it once the navigation has started.
  observer_.reset();
  PageLoadTiming timing;
  timing.navigation_start = base::Time::FromDoubleT(10);

  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());

  // Start the navigation, then start observing the web contents. This used to
  // crash us. Make sure we bail out and don't log histograms.
  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  AttachObserver();
  rfh_tester->SimulateNavigationCommit(GURL(kDefaultTestUrl));

  SimulateTimingUpdate(timing);

  // Navigate again to force histogram logging.
  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  ASSERT_EQ(0, CountCompleteTimingReported());
  ASSERT_EQ(0, CountUpdatedTimingReported());
  CheckErrorEvent(ERR_IPC_WITH_NO_RELEVANT_LOAD, 1);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, DontLogAbortChains) {
  NavigateAndCommit(GURL(kDefaultTestUrl));
  NavigateAndCommit(GURL(kDefaultTestUrl2));
  NavigateAndCommit(GURL(kDefaultTestUrl));
  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNewNavigation, 0);
  CheckErrorEvent(ERR_NO_IPCS_RECEIVED, 2);
  CheckTotalErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, LogAbortChains) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  // Start and abort three loads before one finally commits.
  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl2));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl2), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl2));
  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNewNavigation, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeNewNavigation, 3,
                                      1);
  CheckNoErrorEvents();
}

TEST_F(MetricsWebContentsObserverTest, LogAbortChainsSameURL) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  // Start and abort three loads before one finally commits.
  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->NavigateAndCommit(GURL(kDefaultTestUrl));
  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNewNavigation, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeNewNavigation, 3,
                                      1);
  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeSameURL, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeSameURL, 3, 1);
}

TEST_F(MetricsWebContentsObserverTest, LogAbortChainsNoCommit) {
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents());
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  // Start and abort three loads before one finally commits.
  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl2));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl2), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents_tester->StartNavigation(GURL(kDefaultTestUrl));
  rfh_tester->SimulateNavigationError(GURL(kDefaultTestUrl), net::ERR_ABORTED);
  rfh_tester->SimulateNavigationStop();

  web_contents()->Stop();

  histogram_tester_.ExpectTotalCount(internal::kAbortChainSizeNoCommit, 1);
  histogram_tester_.ExpectBucketCount(internal::kAbortChainSizeNoCommit, 3,
                                      1);
}

}  // namespace page_load_metrics
