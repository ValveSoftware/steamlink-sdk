// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AutoplayUmaHelper_h
#define AutoplayUmaHelper_h

#include "core/events/EventListener.h"
#include "platform/heap/Handle.h"

namespace blink {

// These values are used for histograms. Do not reorder.
enum class AutoplaySource {
  // Autoplay comes from HTMLMediaElement `autoplay` attribute.
  Attribute = 0,
  // Autoplay comes from `play()` method.
  Method = 1,
  // This enum value must be last.
  NumberOfSources = 2,
};

// These values are used for histograms. Do not reorder.
enum class AutoplayUnmuteActionStatus {
  Failure = 0,
  Success = 1,
  NumberOfStatus = 2,
};

// These values are used for histograms. Do not reorder.
enum AutoplayBlockedReason {
  AutoplayBlockedReasonDataSaver = 0,
  AutoplayBlockedReasonSetting = 1,
  AutoplayBlockedReasonDataSaverAndSetting = 2,
  // Keey at the end.
  AutoplayBlockedReasonMax = 3
};

class Document;
class ElementVisibilityObserver;
class HTMLMediaElement;

class AutoplayUmaHelper final : public EventListener {
 public:
  static AutoplayUmaHelper* create(HTMLMediaElement*);

  ~AutoplayUmaHelper();

  bool operator==(const EventListener&) const override;

  void onAutoplayInitiated(AutoplaySource);

  void recordAutoplayUnmuteStatus(AutoplayUnmuteActionStatus);

  void didMoveToNewDocument(Document& oldDocument);

  bool isVisible() const { return m_isVisible; }

  bool hasSource() const { return m_source != AutoplaySource::NumberOfSources; }

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit AutoplayUmaHelper(HTMLMediaElement*);

  void handleEvent(ExecutionContext*, Event*) override;

  void handlePlayingEvent();
  void handlePauseEvent();
  void handleUnloadEvent();

  void maybeUnregisterUnloadListener();

  void maybeStartRecordingMutedVideoPlayMethodBecomeVisible();
  void maybeStopRecordingMutedVideoPlayMethodBecomeVisible(bool isVisible);

  void maybeStartRecordingMutedVideoOffscreenDuration();
  void maybeStopRecordingMutedVideoOffscreenDuration();

  void onVisibilityChangedForMutedVideoOffscreenDuration(bool isVisibile);
  void onVisibilityChangedForMutedVideoPlayMethodBecomeVisible(bool isVisible);

  bool shouldListenToUnloadEvent() const;

  // The autoplay source. Use AutoplaySource::NumberOfSources for invalid
  // source.
  AutoplaySource m_source;
  // The media element this UMA helper is attached to. |m_element| owns |this|.
  Member<HTMLMediaElement> m_element;

  // The observer is used to observe whether a muted video autoplaying by play()
  // method become visible at some point.
  // The UMA is pending for recording as long as this observer is non-null.
  Member<ElementVisibilityObserver> m_mutedVideoPlayMethodVisibilityObserver;

  // -----------------------------------------------------------------------
  // Variables used for recording the duration of autoplay muted video playing
  // offscreen.  The variables are valid when
  // |m_autoplayOffscrenVisibilityObserver| is non-null.
  // The recording stops whenever the playback pauses or the page is unloaded.

  // The starting time of autoplaying muted video.
  int64_t m_mutedVideoAutoplayOffscreenStartTimeMS;

  // The duration an autoplaying muted video has been in offscreen.
  int64_t m_mutedVideoAutoplayOffscreenDurationMS;

  // Whether an autoplaying muted video is visible.
  bool m_isVisible;

  // The observer is used to observer an autoplaying muted video changing it's
  // visibility, which is used for offscreen duration UMA.  The UMA is pending
  // for recording as long as this observer is non-null.
  Member<ElementVisibilityObserver>
      m_mutedVideoOffscreenDurationVisibilityObserver;
};

}  // namespace blink

#endif  // AutoplayUmaHelper_h
