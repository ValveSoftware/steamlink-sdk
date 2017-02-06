// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/OpenedFrameTracker.h"

#include "platform/heap/Handle.h"
#include "public/web/WebFrame.h"

namespace blink {

OpenedFrameTracker::OpenedFrameTracker()
{
}

OpenedFrameTracker::~OpenedFrameTracker()
{
}

bool OpenedFrameTracker::isEmpty() const
{
    return m_openedFrames.isEmpty();
}

void OpenedFrameTracker::add(WebFrame* frame)
{
    m_openedFrames.add(frame);
}

void OpenedFrameTracker::remove(WebFrame* frame)
{
    m_openedFrames.remove(frame);
}

void OpenedFrameTracker::transferTo(WebFrame* opener)
{
    // Copy the set of opened frames, since changing the owner will mutate this set.
    HashSet<WebFrame*> frames(m_openedFrames);
    for (WebFrame* frame : frames)
        frame->setOpener(opener);
}

template <typename VisitorDispatcher>
ALWAYS_INLINE void OpenedFrameTracker::traceFramesImpl(VisitorDispatcher visitor)
{
    HashSet<WebFrame*>::iterator end = m_openedFrames.end();
    for (HashSet<WebFrame*>::iterator it = m_openedFrames.begin(); it != end; ++it)
        WebFrame::traceFrame(visitor, *it);
}

void OpenedFrameTracker::traceFrames(Visitor* visitor) { traceFramesImpl(visitor); }
void OpenedFrameTracker::traceFrames(InlinedGlobalMarkingVisitor visitor) { traceFramesImpl(visitor); }

} // namespace blink
