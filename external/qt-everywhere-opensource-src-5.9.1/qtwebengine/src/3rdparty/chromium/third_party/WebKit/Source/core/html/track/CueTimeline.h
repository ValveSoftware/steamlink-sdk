// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CueTimeline_h
#define CueTimeline_h

#include "core/html/track/TextTrackCue.h"
#include "core/html/track/vtt/VTTCue.h"
#include "platform/PODIntervalTree.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class HTMLMediaElement;
class TextTrackCueList;

// TODO(Oilpan): This needs to be PODIntervalTree<double, Member<TextTrackCue>>.
// However, it is not easy to move PODIntervalTree to the heap (for a
// C++-template reason) so we leave it as a raw pointer at the moment. This is
// safe because CueTimeline and TextTrackCue are guaranteed to die at the same
// time when the owner HTMLMediaElement dies. Thus the raw TextTrackCue* cannot
// become stale pointers.
typedef PODIntervalTree<double, TextTrackCue*> CueIntervalTree;
typedef CueIntervalTree::IntervalType CueInterval;
typedef Vector<CueInterval> CueList;

// This class manages the timeline and rendering updates of cues associated
// with TextTracks. Owned by a HTMLMediaElement.
class CueTimeline final : public GarbageCollectedFinalized<CueTimeline> {
 public:
  CueTimeline(HTMLMediaElement&);

  void addCues(TextTrack*, const TextTrackCueList*);
  void addCue(TextTrack*, TextTrackCue*);
  void removeCues(TextTrack*, const TextTrackCueList*);
  void removeCue(TextTrack*, TextTrackCue*);

  void hideCues(TextTrack*, const TextTrackCueList*);

  void updateActiveCues(double);

  bool ignoreUpdateRequests() const { return m_ignoreUpdate > 0; }
  void beginIgnoringUpdateRequests();
  void endIgnoringUpdateRequests();

  const CueList& currentlyActiveCues() const { return m_currentlyActiveCues; }

  DECLARE_TRACE();

 private:
  HTMLMediaElement& mediaElement() const { return *m_mediaElement; }

  void addCueInternal(TextTrackCue*);
  void removeCueInternal(TextTrackCue*);

  Member<HTMLMediaElement> m_mediaElement;

  CueIntervalTree m_cueTree;

  CueList m_currentlyActiveCues;
  double m_lastUpdateTime;

  int m_ignoreUpdate;
};

class TrackDisplayUpdateScope {
  STACK_ALLOCATED();

 public:
  TrackDisplayUpdateScope(CueTimeline& cueTimeline) {
    m_cueTimeline = &cueTimeline;
    m_cueTimeline->beginIgnoringUpdateRequests();
  }
  ~TrackDisplayUpdateScope() {
    DCHECK(m_cueTimeline);
    m_cueTimeline->endIgnoringUpdateRequests();
  }

 private:
  Member<CueTimeline> m_cueTimeline;
};

#ifndef NDEBUG
// Template specializations required by PodIntervalTree in debug mode.
template <>
struct ValueToString<double> {
  static String toString(const double value) { return String::number(value); }
};

template <>
struct ValueToString<TextTrackCue*> {
  static String toString(TextTrackCue* const& cue) { return cue->toString(); }
};
#endif

}  // namespace blink

#endif  // CueTimeline_h
