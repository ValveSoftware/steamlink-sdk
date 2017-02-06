// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintTiming_h
#define PaintTiming_h

#include "core/dom/Document.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LocalFrame;

// PaintTiming is responsible for tracking paint-related timings for a given
// document.
class PaintTiming final : public GarbageCollectedFinalized<PaintTiming>, public Supplement<Document> {
    WTF_MAKE_NONCOPYABLE(PaintTiming);
    USING_GARBAGE_COLLECTED_MIXIN(PaintTiming);
public:
    virtual ~PaintTiming() { }

    static PaintTiming& from(Document&);

    // mark*() methods record the time for the given paint event, record a trace
    // event, and notify that paint timing has changed. These methods do nothing
    // (early return) if a time has already been recorded for the given paint
    // event.
    void markFirstPaint();

    // markFirstTextPaint, markFirstImagePaint, and markFirstContentfulPaint
    // will also record first paint if first paint hasn't been recorded yet.
    void markFirstContentfulPaint();

    // markFirstTextPaint and markFirstImagePaint will also record first
    // contentful paint if first contentful paint hasn't been recorded yet.
    void markFirstTextPaint();
    void markFirstImagePaint();

    // The getters below return monotonically-increasing seconds, or zero if the
    // given paint event has not yet occurred. See the comments for
    // monotonicallyIncreasingTime in wtf/CurrentTime.h for additional details.

    // firstPaint returns the first time that anything was painted for the
    // current document.
    double firstPaint() const { return m_firstPaint; }

    // firstContentfulPaint returns the first time that 'contentful' content was
    // painted. For instance, the first time that text or image content was
    // painted.
    double firstContentfulPaint() const { return m_firstContentfulPaint; }

    // firstTextPaint returns the first time that text content was painted.
    double firstTextPaint() const { return m_firstTextPaint; }

    // firstImagePaint returns the first time that image content was painted.
    double firstImagePaint() const { return m_firstImagePaint; }

    DECLARE_VIRTUAL_TRACE();

private:
    explicit PaintTiming(Document&);
    LocalFrame* frame() const;
    void notifyPaintTimingChanged();

    // set*() set the timing for the given paint event to the given timestamp
    // and record a trace event if the value is currently zero, but do not
    // notify that paint timing changed. These methods can be invoked from other
    // mark*() or set*() methods to make sure that first paint is marked as part
    // of marking first contentful paint, or that first contentful paint is
    // marked as part of marking first text/image paint, for example.
    void setFirstPaint(double stamp);

    // setFirstContentfulPaint will also set first paint time if first paint
    // time has not yet been recorded.
    void setFirstContentfulPaint(double stamp);

    double m_firstPaint = 0.0;
    double m_firstTextPaint = 0.0;
    double m_firstImagePaint = 0.0;
    double m_firstContentfulPaint = 0.0;

    Member<Document> m_document;
};

} // namespace blink

#endif
