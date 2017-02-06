// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/V0CustomElementSyncMicrotaskQueue.h"

namespace blink {

void V0CustomElementSyncMicrotaskQueue::enqueue(V0CustomElementMicrotaskStep* step)
{
    m_queue.append(step);
}

void V0CustomElementSyncMicrotaskQueue::doDispatch()
{
    unsigned i;

    for (i = 0; i < m_queue.size(); ++i) {
        if (V0CustomElementMicrotaskStep::Processing == m_queue[i]->process())
            break;
    }

    m_queue.remove(0, i);
}

} // namespace blink
