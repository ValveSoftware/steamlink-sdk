// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/FirstMeaningfulPaintDetector.h"

#include "core/css/FontFaceSet.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/paint/PaintTiming.h"
#include "platform/tracing/TraceEvent.h"

namespace blink {

namespace {

// Web fonts that laid out more than this number of characters block First
// Meaningful Paint.
const int kBlankCharactersThreshold = 200;

// FirstMeaningfulPaintDetector stops observing layouts and reports First
// Meaningful Paint when this duration passed from last network activity.
const double kSecondsWithoutNetworkActivityThreshold = 0.5;

}  // namespace

FirstMeaningfulPaintDetector& FirstMeaningfulPaintDetector::from(
    Document& document) {
  return PaintTiming::from(document).firstMeaningfulPaintDetector();
}

FirstMeaningfulPaintDetector::FirstMeaningfulPaintDetector(
    PaintTiming* paintTiming)
    : m_paintTiming(paintTiming),
      m_networkStableTimer(
          this,
          &FirstMeaningfulPaintDetector::networkStableTimerFired) {}

Document* FirstMeaningfulPaintDetector::document() {
  return m_paintTiming->document();
}

// Computes "layout significance" (http://goo.gl/rytlPL) of a layout operation.
// Significance of a layout is the number of layout objects newly added to the
// layout tree, weighted by page height (before and after the layout).
// A paint after the most significance layout during page load is reported as
// First Meaningful Paint.
void FirstMeaningfulPaintDetector::markNextPaintAsMeaningfulIfNeeded(
    const LayoutObjectCounter& counter,
    int contentsHeightBeforeLayout,
    int contentsHeightAfterLayout,
    int visibleHeight) {
  if (m_state == Reported)
    return;

  unsigned delta = counter.count() - m_prevLayoutObjectCount;
  m_prevLayoutObjectCount = counter.count();

  if (visibleHeight == 0)
    return;

  double ratioBefore = std::max(
      1.0, static_cast<double>(contentsHeightBeforeLayout) / visibleHeight);
  double ratioAfter = std::max(
      1.0, static_cast<double>(contentsHeightAfterLayout) / visibleHeight);
  double significance = delta / ((ratioBefore + ratioAfter) / 2);

  // If the page has many blank characters, the significance value is
  // accumulated until the text become visible.
  int approximateBlankCharacterCount =
      FontFaceSet::approximateBlankCharacterCount(*document());
  if (approximateBlankCharacterCount > kBlankCharactersThreshold) {
    m_accumulatedSignificanceWhileHavingBlankText += significance;
  } else {
    significance += m_accumulatedSignificanceWhileHavingBlankText;
    m_accumulatedSignificanceWhileHavingBlankText = 0;
    if (significance > m_maxSignificanceSoFar) {
      m_state = NextPaintIsMeaningful;
      m_maxSignificanceSoFar = significance;
    }
  }
}

void FirstMeaningfulPaintDetector::notifyPaint() {
  if (m_state != NextPaintIsMeaningful)
    return;

  // Skip document background-only paints.
  if (m_paintTiming->firstPaint() == 0.0)
    return;

  m_provisionalFirstMeaningfulPaint = monotonicallyIncreasingTime();
  m_state = NextPaintIsNotMeaningful;

  TRACE_EVENT_MARK_WITH_TIMESTAMP1(
      "loading", "firstMeaningfulPaintCandidate",
      TraceEvent::toTraceTimestamp(m_provisionalFirstMeaningfulPaint), "frame",
      document()->frame());
}

void FirstMeaningfulPaintDetector::checkNetworkStable() {
  DCHECK(document());
  if (m_state == Reported || document()->fetcher()->hasPendingRequest())
    return;

  m_networkStableTimer.startOneShot(kSecondsWithoutNetworkActivityThreshold,
                                    BLINK_FROM_HERE);
}

void FirstMeaningfulPaintDetector::networkStableTimerFired(TimerBase*) {
  if (m_state == Reported || !document() ||
      document()->fetcher()->hasPendingRequest())
    return;

  if (m_provisionalFirstMeaningfulPaint)
    m_paintTiming->setFirstMeaningfulPaint(m_provisionalFirstMeaningfulPaint);
  m_state = Reported;
}

DEFINE_TRACE(FirstMeaningfulPaintDetector) {
  visitor->trace(m_paintTiming);
}

}  // namespace blink
