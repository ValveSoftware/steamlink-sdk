// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FirstMeaningfulPaintDetector_h
#define FirstMeaningfulPaintDetector_h

#include "core/CoreExport.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class Document;
class PaintTiming;

// FirstMeaningfulPaintDetector observes layout operations during page load
// until network stable (2 seconds of no network activity), and computes the
// layout-based First Meaningful Paint.
// See https://goo.gl/vpaxv6 and http://goo.gl/TEiMi4 for more details.
class CORE_EXPORT FirstMeaningfulPaintDetector
    : public GarbageCollectedFinalized<FirstMeaningfulPaintDetector> {
  WTF_MAKE_NONCOPYABLE(FirstMeaningfulPaintDetector);

 public:
  // Used by FrameView to keep track of the number of layout objects created
  // in the frame.
  class LayoutObjectCounter {
   public:
    void reset() { m_count = 0; }
    void increment() { m_count++; }
    unsigned count() const { return m_count; }

   private:
    unsigned m_count = 0;
  };

  static FirstMeaningfulPaintDetector& from(Document&);

  FirstMeaningfulPaintDetector(PaintTiming*);
  virtual ~FirstMeaningfulPaintDetector() {}

  void markNextPaintAsMeaningfulIfNeeded(const LayoutObjectCounter&,
                                         int contentsHeightBeforeLayout,
                                         int contentsHeightAfterLayout,
                                         int visibleHeight);
  void notifyPaint();
  void checkNetworkStable();

  DECLARE_TRACE();

 private:
  friend class FirstMeaningfulPaintDetectorTest;

  Document* document();
  void networkStableTimerFired(TimerBase*);

  enum State { NextPaintIsNotMeaningful, NextPaintIsMeaningful, Reported };
  State m_state = NextPaintIsNotMeaningful;

  Member<PaintTiming> m_paintTiming;
  double m_provisionalFirstMeaningfulPaint = 0.0;
  double m_maxSignificanceSoFar = 0.0;
  double m_accumulatedSignificanceWhileHavingBlankText = 0.0;
  unsigned m_prevLayoutObjectCount = 0;
  Timer<FirstMeaningfulPaintDetector> m_networkStableTimer;
};

}  // namespace blink

#endif
