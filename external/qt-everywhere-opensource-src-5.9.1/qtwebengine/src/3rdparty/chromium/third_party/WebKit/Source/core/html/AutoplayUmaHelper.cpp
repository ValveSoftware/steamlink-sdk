// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/AutoplayUmaHelper.h"

#include "core/dom/Document.h"
#include "core/dom/ElementVisibilityObserver.h"
#include "core/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMediaElement.h"
#include "platform/Histogram.h"
#include "wtf/CurrentTime.h"

namespace blink {

namespace {

const int32_t maxOffscreenDurationUmaMS = 60 * 60 * 1000;
const int32_t offscreenDurationUmaBucketCount = 50;

}  // namespace

AutoplayUmaHelper* AutoplayUmaHelper::create(HTMLMediaElement* element) {
  return new AutoplayUmaHelper(element);
}

AutoplayUmaHelper::AutoplayUmaHelper(HTMLMediaElement* element)
    : EventListener(CPPEventListenerType),
      m_source(AutoplaySource::NumberOfSources),
      m_element(element),
      m_mutedVideoPlayMethodVisibilityObserver(nullptr),
      m_mutedVideoAutoplayOffscreenStartTimeMS(0),
      m_mutedVideoAutoplayOffscreenDurationMS(0),
      m_isVisible(false),
      m_mutedVideoOffscreenDurationVisibilityObserver(nullptr) {}

AutoplayUmaHelper::~AutoplayUmaHelper() = default;

bool AutoplayUmaHelper::operator==(const EventListener& other) const {
  return this == &other;
}

void AutoplayUmaHelper::onAutoplayInitiated(AutoplaySource source) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, videoHistogram,
                      ("Media.Video.Autoplay",
                       static_cast<int>(AutoplaySource::NumberOfSources)));
  DEFINE_STATIC_LOCAL(EnumerationHistogram, mutedVideoHistogram,
                      ("Media.Video.Autoplay.Muted",
                       static_cast<int>(AutoplaySource::NumberOfSources)));
  DEFINE_STATIC_LOCAL(EnumerationHistogram, audioHistogram,
                      ("Media.Audio.Autoplay",
                       static_cast<int>(AutoplaySource::NumberOfSources)));
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, blockedMutedVideoHistogram,
      ("Media.Video.Autoplay.Muted.Blocked", AutoplayBlockedReasonMax));

  // Autoplay already initiated
  // TODO(zqzhang): how about having autoplay attribute and calling `play()` in
  // the script?
  if (hasSource())
    return;

  m_source = source;

  // Record the source.
  if (m_element->isHTMLVideoElement()) {
    videoHistogram.count(static_cast<int>(m_source));
    if (m_element->muted())
      mutedVideoHistogram.count(static_cast<int>(m_source));
  } else {
    audioHistogram.count(static_cast<int>(m_source));
  }

  // Record if it will be blocked by Data Saver or Autoplay setting.
  if (m_element->isHTMLVideoElement() && m_element->muted() &&
      RuntimeEnabledFeatures::autoplayMutedVideosEnabled()) {
    bool dataSaverEnabled =
        m_element->document().settings() &&
        m_element->document().settings()->dataSaverEnabled();
    bool blockedBySetting = !m_element->isAutoplayAllowedPerSettings();

    if (dataSaverEnabled && blockedBySetting) {
      blockedMutedVideoHistogram.count(
          AutoplayBlockedReasonDataSaverAndSetting);
    } else if (dataSaverEnabled) {
      blockedMutedVideoHistogram.count(AutoplayBlockedReasonDataSaver);
    } else if (blockedBySetting) {
      blockedMutedVideoHistogram.count(AutoplayBlockedReasonSetting);
    }
  }

  m_element->addEventListener(EventTypeNames::playing, this, false);
}

void AutoplayUmaHelper::recordAutoplayUnmuteStatus(
    AutoplayUnmuteActionStatus status) {
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, autoplayUnmuteHistogram,
      ("Media.Video.Autoplay.Muted.UnmuteAction",
       static_cast<int>(AutoplayUnmuteActionStatus::NumberOfStatus)));

  autoplayUnmuteHistogram.count(static_cast<int>(status));
}

void AutoplayUmaHelper::didMoveToNewDocument(Document& oldDocument) {
  if (!shouldListenToUnloadEvent())
    return;

  if (oldDocument.domWindow())
    oldDocument.domWindow()->removeEventListener(EventTypeNames::unload, this,
                                                 false);
  if (m_element->document().domWindow())
    m_element->document().domWindow()->addEventListener(EventTypeNames::unload,
                                                        this, false);
}

void AutoplayUmaHelper::onVisibilityChangedForMutedVideoPlayMethodBecomeVisible(
    bool isVisible) {
  if (!isVisible || !m_mutedVideoPlayMethodVisibilityObserver)
    return;

  maybeStopRecordingMutedVideoPlayMethodBecomeVisible(true);
}

void AutoplayUmaHelper::onVisibilityChangedForMutedVideoOffscreenDuration(
    bool isVisible) {
  if (isVisible == m_isVisible)
    return;

  if (isVisible)
    m_mutedVideoAutoplayOffscreenDurationMS +=
        static_cast<int64_t>(monotonicallyIncreasingTimeMS()) -
        m_mutedVideoAutoplayOffscreenStartTimeMS;
  else
    m_mutedVideoAutoplayOffscreenStartTimeMS =
        static_cast<int64_t>(monotonicallyIncreasingTimeMS());

  m_isVisible = isVisible;
}

void AutoplayUmaHelper::handleEvent(ExecutionContext* executionContext,
                                    Event* event) {
  if (event->type() == EventTypeNames::playing)
    handlePlayingEvent();
  else if (event->type() == EventTypeNames::pause)
    handlePauseEvent();
  else if (event->type() == EventTypeNames::unload)
    handleUnloadEvent();
  else
    NOTREACHED();
}

void AutoplayUmaHelper::handlePlayingEvent() {
  maybeStartRecordingMutedVideoPlayMethodBecomeVisible();
  maybeStartRecordingMutedVideoOffscreenDuration();

  m_element->removeEventListener(EventTypeNames::playing, this, false);
}

void AutoplayUmaHelper::handlePauseEvent() {
  maybeStopRecordingMutedVideoOffscreenDuration();
}

void AutoplayUmaHelper::handleUnloadEvent() {
  maybeStopRecordingMutedVideoPlayMethodBecomeVisible(false);
  maybeStopRecordingMutedVideoOffscreenDuration();
}

void AutoplayUmaHelper::maybeStartRecordingMutedVideoPlayMethodBecomeVisible() {
  if (m_source != AutoplaySource::Method || !m_element->isHTMLVideoElement() ||
      !m_element->muted())
    return;

  m_mutedVideoPlayMethodVisibilityObserver = new ElementVisibilityObserver(
      m_element,
      WTF::bind(&AutoplayUmaHelper::
                    onVisibilityChangedForMutedVideoPlayMethodBecomeVisible,
                wrapWeakPersistent(this)));
  m_mutedVideoPlayMethodVisibilityObserver->start();
  if (m_element->document().domWindow())
    m_element->document().domWindow()->addEventListener(EventTypeNames::unload,
                                                        this, false);
}

void AutoplayUmaHelper::maybeStopRecordingMutedVideoPlayMethodBecomeVisible(
    bool visible) {
  if (!m_mutedVideoPlayMethodVisibilityObserver)
    return;

  DEFINE_STATIC_LOCAL(BooleanHistogram, histogram,
                      ("Media.Video.Autoplay.Muted.PlayMethod.BecomesVisible"));

  histogram.count(visible);
  m_mutedVideoPlayMethodVisibilityObserver->stop();
  m_mutedVideoPlayMethodVisibilityObserver = nullptr;
  maybeUnregisterUnloadListener();
}

void AutoplayUmaHelper::maybeStartRecordingMutedVideoOffscreenDuration() {
  if (!m_element->isHTMLVideoElement() || !m_element->muted())
    return;

  // Start recording muted video playing offscreen duration.
  m_mutedVideoAutoplayOffscreenStartTimeMS =
      static_cast<int64_t>(monotonicallyIncreasingTimeMS());
  m_isVisible = false;
  m_mutedVideoOffscreenDurationVisibilityObserver =
      new ElementVisibilityObserver(
          m_element,
          WTF::bind(&AutoplayUmaHelper::
                        onVisibilityChangedForMutedVideoOffscreenDuration,
                    wrapWeakPersistent(this)));
  m_mutedVideoOffscreenDurationVisibilityObserver->start();
  m_element->addEventListener(EventTypeNames::pause, this, false);
  if (m_element->document().domWindow())
    m_element->document().domWindow()->addEventListener(EventTypeNames::unload,
                                                        this, false);
}

void AutoplayUmaHelper::maybeStopRecordingMutedVideoOffscreenDuration() {
  if (!m_mutedVideoOffscreenDurationVisibilityObserver)
    return;

  if (!m_isVisible)
    m_mutedVideoAutoplayOffscreenDurationMS +=
        static_cast<int64_t>(monotonicallyIncreasingTimeMS()) -
        m_mutedVideoAutoplayOffscreenStartTimeMS;

  // Since histograms uses int32_t, the duration needs to be limited to
  // std::numeric_limits<int32_t>::max().
  int32_t boundedTime = static_cast<int32_t>(
      std::min<int64_t>(m_mutedVideoAutoplayOffscreenDurationMS,
                        std::numeric_limits<int32_t>::max()));

  if (m_source == AutoplaySource::Attribute) {
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, durationHistogram,
        ("Media.Video.Autoplay.Muted.Attribute.OffscreenDuration", 1,
         maxOffscreenDurationUmaMS, offscreenDurationUmaBucketCount));
    durationHistogram.count(boundedTime);
  } else {
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, durationHistogram,
        ("Media.Video.Autoplay.Muted.PlayMethod.OffscreenDuration", 1,
         maxOffscreenDurationUmaMS, offscreenDurationUmaBucketCount));
    durationHistogram.count(boundedTime);
  }
  m_mutedVideoOffscreenDurationVisibilityObserver->stop();
  m_mutedVideoOffscreenDurationVisibilityObserver = nullptr;
  m_mutedVideoAutoplayOffscreenDurationMS = 0;
  m_element->removeEventListener(EventTypeNames::pause, this, false);
  maybeUnregisterUnloadListener();
}

void AutoplayUmaHelper::maybeUnregisterUnloadListener() {
  if (!shouldListenToUnloadEvent() && m_element->document().domWindow())
    m_element->document().domWindow()->removeEventListener(
        EventTypeNames::unload, this, false);
}

bool AutoplayUmaHelper::shouldListenToUnloadEvent() const {
  return m_mutedVideoPlayMethodVisibilityObserver ||
         m_mutedVideoOffscreenDurationVisibilityObserver;
}

DEFINE_TRACE(AutoplayUmaHelper) {
  EventListener::trace(visitor);
  visitor->trace(m_element);
  visitor->trace(m_mutedVideoPlayMethodVisibilityObserver);
  visitor->trace(m_mutedVideoOffscreenDurationVisibilityObserver);
}

}  // namespace blink
