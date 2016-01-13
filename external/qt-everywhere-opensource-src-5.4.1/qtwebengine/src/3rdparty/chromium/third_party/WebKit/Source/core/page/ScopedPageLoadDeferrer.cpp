/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/page/ScopedPageLoadDeferrer.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Page.h"
#include "wtf/HashSet.h"

namespace WebCore {

ScopedPageLoadDeferrer::ScopedPageLoadDeferrer(Page* exclusion)
{
    const HashSet<Page*>& pages = Page::ordinaryPages();

    HashSet<Page*>::const_iterator end = pages.end();
    for (HashSet<Page*>::const_iterator it = pages.begin(); it != end; ++it) {
        Page* page = *it;
        if (page == exclusion || page->defersLoading())
            continue;

        if (page->mainFrame()->isLocalFrame()) {
            m_deferredFrames.append(page->deprecatedLocalMainFrame());

            // Ensure that we notify the client if the initial empty document is accessed before
            // showing anything modal, to prevent spoofs while the modal window or sheet is visible.
            page->deprecatedLocalMainFrame()->loader().notifyIfInitialDocumentAccessed();
        }

        // This code is not logically part of load deferring, but we do not want JS code executed
        // beneath modal windows or sheets, which is exactly when ScopedPageLoadDeferrer is used.
        for (Frame* frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (frame->isLocalFrame())
                toLocalFrame(frame)->document()->suspendScheduledTasks();
        }
    }

    size_t count = m_deferredFrames.size();
    for (size_t i = 0; i < count; ++i) {
        if (Page* page = m_deferredFrames[i]->page())
            page->setDefersLoading(true);
    }
}

ScopedPageLoadDeferrer::~ScopedPageLoadDeferrer()
{
    for (size_t i = 0; i < m_deferredFrames.size(); ++i) {
        if (Page* page = m_deferredFrames[i]->page()) {
            page->setDefersLoading(false);

            for (Frame* frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
                if (frame->isLocalFrame())
                    toLocalFrame(frame)->document()->resumeScheduledTasks();
            }
        }
    }
}

} // namespace WebCore
