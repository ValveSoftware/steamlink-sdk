// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/metrics_render_frame_observer.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/renderer/page_timing_metrics_sender.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPerformance.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

base::TimeDelta ClampDelta(double event, double start) {
  if (event - start < 0)
    event = start;
  return base::Time::FromDoubleT(event) - base::Time::FromDoubleT(start);
}

}  //  namespace

MetricsRenderFrameObserver::MetricsRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

MetricsRenderFrameObserver::~MetricsRenderFrameObserver() {}

void MetricsRenderFrameObserver::DidChangePerformanceTiming() {
  SendMetrics();
}

void MetricsRenderFrameObserver::DidObserveLoadingBehavior(
    blink::WebLoadingBehaviorFlag behavior) {
  if (page_timing_metrics_sender_)
    page_timing_metrics_sender_->DidObserveLoadingBehavior(behavior);
}

void MetricsRenderFrameObserver::DidCommitProvisionalLoad(
    bool is_new_navigation,
    bool is_same_page_navigation) {
  // Same-page navigations (e.g. an in-document navigation from a fragment
  // link) aren't full page loads, since they don't go to network to load the
  // main HTML resource. DidStartProvisionalLoad doesn't get invoked for same
  // page navigations, so we may still have an active
  // page_timing_metrics_sender_ at this point.
  if (is_same_page_navigation)
    return;

  // Make sure to release the sender for a previous navigation, if we have one.
  page_timing_metrics_sender_.reset();

  // We only create a PageTimingMetricsSender if the page meets the criteria for
  // sending and recording metrics. Once page_timing_metrics_sender_ is
  // non-null, we will send metrics for the current page at some later time, as
  // those metrics become available.
  if (ShouldSendMetrics()) {
    PageLoadTiming timing(GetTiming());
    DCHECK(!timing.navigation_start.is_null());
    page_timing_metrics_sender_.reset(
        new PageTimingMetricsSender(this, routing_id(), CreateTimer(), timing));
  }
}

void MetricsRenderFrameObserver::SendMetrics() {
  if (!page_timing_metrics_sender_)
    return;
  if (HasNoRenderFrame())
    return;
  PageLoadTiming timing(GetTiming());
  page_timing_metrics_sender_->Send(timing);
}

bool MetricsRenderFrameObserver::ShouldSendMetrics() const {
  if (HasNoRenderFrame())
    return false;
  const blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // We only generate historgrams for main frames.
  if (frame->parent())
    return false;

  const blink::WebDocument& document = frame->document();
  // Ignore non-HTTP schemes (e.g. chrome://).
  const GURL& url = document.url();
  if (!url.SchemeIsHTTPOrHTTPS())
    return false;

  const blink::WebURLResponse& url_response = frame->dataSource()->response();

  // Ignore non-HTML documents (e.g. SVG). Note that images are treated by
  // Blink as HTML documents, so to exclude images, we must perform
  // additional mime type checking below.
  if (!document.isHTMLDocument() && !document.isXHTMLDocument())
    return false;

  // Ignore non-HTML mime types (e.g. images).
  std::string mime_type = url_response.mimeType().utf8();
  if (mime_type != "text/html" && mime_type != "application/xhtml+xml")
    return false;

  return true;
}

PageLoadTiming MetricsRenderFrameObserver::GetTiming() const {
  const blink::WebPerformance& perf =
      render_frame()->GetWebFrame()->performance();

  PageLoadTiming timing;
  double start = perf.navigationStart();
  timing.navigation_start = base::Time::FromDoubleT(start);
  timing.response_start = ClampDelta(perf.responseStart(), start);
  timing.dom_loading = ClampDelta(perf.domLoading(), start);
  timing.dom_content_loaded_event_start =
      ClampDelta(perf.domContentLoadedEventStart(), start);
  timing.load_event_start = ClampDelta(perf.loadEventStart(), start);
  timing.first_layout = ClampDelta(perf.firstLayout(), start);
  timing.first_paint = ClampDelta(perf.firstPaint(), start);
  timing.first_text_paint = ClampDelta(perf.firstTextPaint(), start);
  timing.first_image_paint = ClampDelta(perf.firstImagePaint(), start);
  timing.first_contentful_paint =
      ClampDelta(perf.firstContentfulPaint(), start);
  timing.parse_start = ClampDelta(perf.parseStart(), start);
  timing.parse_stop = ClampDelta(perf.parseStop(), start);
  timing.parse_blocked_on_script_load_duration =
      base::TimeDelta::FromSecondsD(perf.parseBlockedOnScriptLoadDuration());
  timing.parse_blocked_on_script_load_from_document_write_duration =
      base::TimeDelta::FromSecondsD(
          perf.parseBlockedOnScriptLoadFromDocumentWriteDuration());
  return timing;
}

std::unique_ptr<base::Timer> MetricsRenderFrameObserver::CreateTimer() const {
  return base::WrapUnique(new base::OneShotTimer);
}

bool MetricsRenderFrameObserver::HasNoRenderFrame() const {
  bool no_frame = !render_frame() || !render_frame()->GetWebFrame();
  DCHECK(!no_frame);
  return no_frame;
}

void MetricsRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace page_load_metrics
