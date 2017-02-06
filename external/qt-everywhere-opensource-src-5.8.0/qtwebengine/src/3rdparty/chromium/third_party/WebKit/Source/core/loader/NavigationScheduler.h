/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2009 Adam Barth. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NavigationScheduler_h
#define NavigationScheduler_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebScheduler.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

class CancellableTaskFactory;
class Document;
class FormSubmission;
class LocalFrame;
class ScheduledNavigation;

class CORE_EXPORT NavigationScheduler final : public GarbageCollectedFinalized<NavigationScheduler> {
    WTF_MAKE_NONCOPYABLE(NavigationScheduler);
public:
    static NavigationScheduler* create(LocalFrame* frame)
    {
        return new NavigationScheduler(frame);
    }

    ~NavigationScheduler();

    bool locationChangePending();
    bool isNavigationScheduledWithin(double intervalInSeconds) const;

    void scheduleRedirect(double delay, const String& url);
    void scheduleLocationChange(Document*, const String& url, bool replacesCurrentItem = true);
    void schedulePageBlock(Document*);
    void scheduleFormSubmission(Document*, FormSubmission*);
    void scheduleReload();

    void startTimer();
    void cancel();

    DECLARE_TRACE();

private:
    explicit NavigationScheduler(LocalFrame*);

    bool shouldScheduleReload() const;
    bool shouldScheduleNavigation(const String& url) const;

    void navigateTask();
    void schedule(ScheduledNavigation*);

    static bool mustReplaceCurrentItem(LocalFrame* targetFrame);

    Member<LocalFrame> m_frame;
    std::unique_ptr<CancellableTaskFactory> m_navigateTaskFactory;
    Member<ScheduledNavigation> m_redirect;
    WebScheduler::NavigatingFrameType m_frameType; // Exists because we can't deref m_frame in destructor.
};

class NavigationDisablerForBeforeUnload {
    WTF_MAKE_NONCOPYABLE(NavigationDisablerForBeforeUnload);
    STACK_ALLOCATED();
public:
    NavigationDisablerForBeforeUnload()
    {
        s_navigationDisableCount++;
    }
    ~NavigationDisablerForBeforeUnload()
    {
        ASSERT(s_navigationDisableCount);
        s_navigationDisableCount--;
    }
    static bool isNavigationAllowed() { return !s_navigationDisableCount; }

private:
    static unsigned s_navigationDisableCount;
};

class CORE_EXPORT NavigationCounterForUnload {
    WTF_MAKE_NONCOPYABLE(NavigationCounterForUnload);
    STACK_ALLOCATED();
public:
    NavigationCounterForUnload()
    {
        s_inUnloadHandler++;
    }
    ~NavigationCounterForUnload()
    {
        DCHECK(s_inUnloadHandler);
        s_inUnloadHandler--;
    }
    static bool inUnloadHandler() { return !!s_inUnloadHandler; }

private:
    static unsigned s_inUnloadHandler;
};

} // namespace blink

#endif // NavigationScheduler_h
