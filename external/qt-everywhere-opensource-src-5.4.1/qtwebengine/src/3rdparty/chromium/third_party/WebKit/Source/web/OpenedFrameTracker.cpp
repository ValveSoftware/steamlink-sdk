// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/OpenedFrameTracker.h"

#include "public/web/WebFrame.h"

namespace blink {

OpenedFrameTracker::OpenedFrameTracker()
{
}

OpenedFrameTracker::~OpenedFrameTracker()
{
    updateOpener(0);
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

void OpenedFrameTracker::updateOpener(WebFrame* frame)
{
    HashSet<WebFrame*>::iterator end = m_openedFrames.end();
    for (HashSet<WebFrame*>::iterator it = m_openedFrames.begin(); it != end; ++it)
        (*it)->m_opener = frame;
}

} // namespace blink
