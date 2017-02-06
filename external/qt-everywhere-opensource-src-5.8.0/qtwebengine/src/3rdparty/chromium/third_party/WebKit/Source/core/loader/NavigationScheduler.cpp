/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "core/loader/NavigationScheduler.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/events/Event.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLFormElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FormSubmission.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/FrameLoaderStateMachine.h"
#include "core/page/Page.h"
#include "platform/Histogram.h"
#include "platform/SharedBuffer.h"
#include "platform/UserGestureIndicator.h"
#include "platform/scheduler/CancellableTaskFactory.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebScheduler.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

namespace {

// Add new scheduled navigation types before ScheduledLastEntry
enum ScheduledNavigationType {
    ScheduledReload,
    ScheduledFormSubmission,
    ScheduledURLNavigation,
    ScheduledRedirect,
    ScheduledLocationChange,
    ScheduledPageBlock,

    ScheduledLastEntry
};

// If the current frame has a provisional document loader, a scheduled
// navigation might abort that load. Log those occurrences until
// crbug.com/557430 is resolved.
void maybeLogScheduledNavigationClobber(ScheduledNavigationType type, LocalFrame* frame, const FrameLoadRequest& request, UserGestureIndicator* gestureIndicator)
{
    if (!frame->loader().provisionalDocumentLoader())
        return;
    // Include enumeration values userGesture variants.
    DEFINE_STATIC_LOCAL(EnumerationHistogram, scheduledNavigationClobberHistogram, ("Navigation.Scheduled.MaybeCausedAbort", ScheduledNavigationType::ScheduledLastEntry * 2));

    UserGestureToken* gestureToken = gestureIndicator->currentToken();
    int value = gestureToken->hasGestures() ? type + ScheduledLastEntry : type;
    scheduledNavigationClobberHistogram.count(value);

    DEFINE_STATIC_LOCAL(CustomCountHistogram, scheduledClobberAbortTimeHistogram, ("Navigation.Scheduled.MaybeCausedAbort.Time", 1, 10000, 50));
    double navigationStart = frame->loader().provisionalDocumentLoader()->timing().navigationStart();
    if (navigationStart)
        scheduledClobberAbortTimeHistogram.count(monotonicallyIncreasingTime() - navigationStart);
}

} // namespace

unsigned NavigationDisablerForBeforeUnload::s_navigationDisableCount = 0;
unsigned NavigationCounterForUnload::s_inUnloadHandler = 0;

class ScheduledNavigation : public GarbageCollectedFinalized<ScheduledNavigation> {
    WTF_MAKE_NONCOPYABLE(ScheduledNavigation);
public:
    ScheduledNavigation(double delay, Document* originDocument, bool replacesCurrentItem, bool isLocationChange)
        : m_delay(delay)
        , m_originDocument(originDocument)
        , m_replacesCurrentItem(replacesCurrentItem)
        , m_isLocationChange(isLocationChange)
        , m_wasUserGesture(UserGestureIndicator::processingUserGesture())
    {
        if (m_wasUserGesture)
            m_userGestureToken = UserGestureIndicator::currentToken();
    }
    virtual ~ScheduledNavigation() { }

    virtual void fire(LocalFrame*) = 0;

    virtual bool shouldStartTimer(LocalFrame*) { return true; }

    double delay() const { return m_delay; }
    Document* originDocument() const { return m_originDocument.get(); }
    bool replacesCurrentItem() const { return m_replacesCurrentItem; }
    bool isLocationChange() const { return m_isLocationChange; }
    std::unique_ptr<UserGestureIndicator> createUserGestureIndicator()
    {
        if (m_wasUserGesture &&  m_userGestureToken)
            return wrapUnique(new UserGestureIndicator(m_userGestureToken));
        return wrapUnique(new UserGestureIndicator(DefinitelyNotProcessingUserGesture));
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_originDocument);
    }

protected:
    void clearUserGesture() { m_wasUserGesture = false; }

private:
    double m_delay;
    Member<Document> m_originDocument;
    bool m_replacesCurrentItem;
    bool m_isLocationChange;
    bool m_wasUserGesture;
    RefPtr<UserGestureToken> m_userGestureToken;
};

class ScheduledURLNavigation : public ScheduledNavigation {
protected:
    ScheduledURLNavigation(double delay, Document* originDocument, const String& url, bool replacesCurrentItem, bool isLocationChange)
        : ScheduledNavigation(delay, originDocument, replacesCurrentItem, isLocationChange)
        , m_url(url)
        , m_shouldCheckMainWorldContentSecurityPolicy(CheckContentSecurityPolicy)
    {
        if (ContentSecurityPolicy::shouldBypassMainWorld(originDocument))
            m_shouldCheckMainWorldContentSecurityPolicy = DoNotCheckContentSecurityPolicy;
    }

    void fire(LocalFrame* frame) override
    {
        std::unique_ptr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        FrameLoadRequest request(originDocument(), m_url, "_self", m_shouldCheckMainWorldContentSecurityPolicy);
        request.setReplacesCurrentItem(replacesCurrentItem());
        request.setClientRedirect(ClientRedirectPolicy::ClientRedirect);

        ScheduledNavigationType type = isLocationChange() ? ScheduledNavigationType::ScheduledLocationChange : ScheduledNavigationType::ScheduledURLNavigation;
        maybeLogScheduledNavigationClobber(type, frame, request, gestureIndicator.get());
        frame->loader().load(request);
    }

    String url() const { return m_url; }

private:
    String m_url;
    ContentSecurityPolicyDisposition m_shouldCheckMainWorldContentSecurityPolicy;
};

class ScheduledRedirect final : public ScheduledURLNavigation {
public:
    static ScheduledRedirect* create(double delay, Document* originDocument, const String& url, bool replacesCurrentItem)
    {
        return new ScheduledRedirect(delay, originDocument, url, replacesCurrentItem);
    }

    bool shouldStartTimer(LocalFrame* frame) override { return frame->document()->loadEventFinished(); }

    void fire(LocalFrame* frame) override
    {
        std::unique_ptr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        FrameLoadRequest request(originDocument(), url(), "_self");
        request.setReplacesCurrentItem(replacesCurrentItem());
        if (equalIgnoringFragmentIdentifier(frame->document()->url(), request.resourceRequest().url()))
            request.resourceRequest().setCachePolicy(WebCachePolicy::ValidatingCacheData);
        request.setClientRedirect(ClientRedirectPolicy::ClientRedirect);
        maybeLogScheduledNavigationClobber(ScheduledNavigationType::ScheduledRedirect, frame, request, gestureIndicator.get());
        frame->loader().load(request);
    }

private:
    ScheduledRedirect(double delay, Document* originDocument, const String& url, bool replacesCurrentItem)
        : ScheduledURLNavigation(delay, originDocument, url, replacesCurrentItem, false)
    {
        clearUserGesture();
    }
};

class ScheduledLocationChange final : public ScheduledURLNavigation {
public:
    static ScheduledLocationChange* create(Document* originDocument, const String& url, bool replacesCurrentItem)
    {
        return new ScheduledLocationChange(originDocument, url, replacesCurrentItem);
    }

private:
    ScheduledLocationChange(Document* originDocument, const String& url, bool replacesCurrentItem)
        : ScheduledURLNavigation(0.0, originDocument, url, replacesCurrentItem, !protocolIsJavaScript(url)) { }
};

class ScheduledReload final : public ScheduledNavigation {
public:
    static ScheduledReload* create()
    {
        return new ScheduledReload;
    }

    void fire(LocalFrame* frame) override
    {
        std::unique_ptr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        ResourceRequest resourceRequest = frame->loader().resourceRequestForReload(FrameLoadTypeReload, KURL(), ClientRedirectPolicy::ClientRedirect);
        if (resourceRequest.isNull())
            return;
        FrameLoadRequest request = FrameLoadRequest(nullptr, resourceRequest);
        request.setClientRedirect(ClientRedirectPolicy::ClientRedirect);
        maybeLogScheduledNavigationClobber(ScheduledNavigationType::ScheduledReload, frame, request, gestureIndicator.get());
        frame->loader().load(request, FrameLoadTypeReload);
    }

private:
    ScheduledReload()
        : ScheduledNavigation(0.0, nullptr, true, true)
    {
    }
};

class ScheduledPageBlock final : public ScheduledURLNavigation {
public:
    static ScheduledPageBlock* create(Document* originDocument, const String& url)
    {
        return new ScheduledPageBlock(originDocument, url);
    }

    void fire(LocalFrame* frame) override
    {
        std::unique_ptr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        SubstituteData substituteData(SharedBuffer::create(), "text/plain", "UTF-8", KURL(), ForceSynchronousLoad);
        FrameLoadRequest request(originDocument(), url(), substituteData);
        request.setReplacesCurrentItem(true);
        request.setClientRedirect(ClientRedirectPolicy::ClientRedirect);
        maybeLogScheduledNavigationClobber(ScheduledNavigationType::ScheduledPageBlock, frame, request, gestureIndicator.get());
        frame->loader().load(request);
    }
private:
    ScheduledPageBlock(Document* originDocument, const String& url)
        : ScheduledURLNavigation(0.0, originDocument, url, true, true)
    {
    }

};

class ScheduledFormSubmission final : public ScheduledNavigation {
public:
    static ScheduledFormSubmission* create(Document* document, FormSubmission* submission, bool replacesCurrentItem)
    {
        return new ScheduledFormSubmission(document, submission, replacesCurrentItem);
    }

    void fire(LocalFrame* frame) override
    {
        std::unique_ptr<UserGestureIndicator> gestureIndicator = createUserGestureIndicator();
        FrameLoadRequest frameRequest = m_submission->createFrameLoadRequest(originDocument());
        frameRequest.setReplacesCurrentItem(replacesCurrentItem());
        maybeLogScheduledNavigationClobber(ScheduledNavigationType::ScheduledFormSubmission, frame, frameRequest, gestureIndicator.get());
        frame->loader().load(frameRequest);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_submission);
        ScheduledNavigation::trace(visitor);
    }

private:
    ScheduledFormSubmission(Document* document, FormSubmission* submission, bool replacesCurrentItem)
        : ScheduledNavigation(0, document, replacesCurrentItem, true)
        , m_submission(submission)
    {
        ASSERT(m_submission->form());
    }

    Member<FormSubmission> m_submission;
};

NavigationScheduler::NavigationScheduler(LocalFrame* frame)
    : m_frame(frame)
    , m_navigateTaskFactory(CancellableTaskFactory::create(this, &NavigationScheduler::navigateTask))
    , m_frameType(m_frame->isMainFrame() ? WebScheduler::NavigatingFrameType::kMainFrame : WebScheduler::NavigatingFrameType::kChildFrame)
{
}

NavigationScheduler::~NavigationScheduler()
{
    if (m_navigateTaskFactory->isPending())
        Platform::current()->currentThread()->scheduler()->removePendingNavigation(m_frameType);
}

bool NavigationScheduler::locationChangePending()
{
    return m_redirect && m_redirect->isLocationChange();
}

bool NavigationScheduler::isNavigationScheduledWithin(double interval) const
{
    return m_redirect && m_redirect->delay() <= interval;
}

// TODO(dcheng): There are really two different load blocking concepts at work
// here and they have been incorrectly tangled together.
//
// 1. NavigationDisablerForBeforeUnload is for blocking navigation scheduling
//    during a beforeunload event. Scheduled navigations during beforeunload
//    would make it possible to get trapped in an endless loop of beforeunload
//    dialogs.
//
//    Checking Frame::isNavigationAllowed() doesn't make sense in this context:
//    NavigationScheduler is always cleared when a new load commits, so it's
//    impossible for a scheduled navigation to clobber a navigation that just
//    committed.
//
// 2. FrameNavigationDisabler / LocalFrame::isNavigationAllowed() are intended
//    to prevent Documents from being reattached during destruction, since it
//    can cause bugs with security origin confusion. This is primarily intended
//    to block /synchronous/ navigations during things lke Document::detach().
inline bool NavigationScheduler::shouldScheduleReload() const
{
    return m_frame->page() && m_frame->isNavigationAllowed() && NavigationDisablerForBeforeUnload::isNavigationAllowed();
}

inline bool NavigationScheduler::shouldScheduleNavigation(const String& url) const
{
    return m_frame->page() && m_frame->isNavigationAllowed() && (protocolIsJavaScript(url) || NavigationDisablerForBeforeUnload::isNavigationAllowed());
}

void NavigationScheduler::scheduleRedirect(double delay, const String& url)
{
    if (!shouldScheduleNavigation(url))
        return;
    if (delay < 0 || delay > INT_MAX / 1000)
        return;
    if (url.isEmpty())
        return;

    // We want a new back/forward list item if the refresh timeout is > 1 second.
    if (!m_redirect || delay <= m_redirect->delay())
        schedule(ScheduledRedirect::create(delay, m_frame->document(), url, delay <= 1));
}

bool NavigationScheduler::mustReplaceCurrentItem(LocalFrame* targetFrame)
{
    // Non-user navigation before the page has finished firing onload should not create a new back/forward item.
    // See https://webkit.org/b/42861 for the original motivation for this.
    if (!targetFrame->document()->loadEventFinished() && !UserGestureIndicator::utilizeUserGesture())
        return true;

    // Navigation of a subframe during loading of an ancestor frame does not create a new back/forward item.
    // The definition of "during load" is any time before all handlers for the load event have been run.
    // See https://bugs.webkit.org/show_bug.cgi?id=14957 for the original motivation for this.
    Frame* parentFrame = targetFrame->tree().parent();
    return parentFrame && parentFrame->isLocalFrame() && !toLocalFrame(parentFrame)->loader().allAncestorsAreComplete();
}

void NavigationScheduler::scheduleLocationChange(Document* originDocument, const String& url, bool replacesCurrentItem)
{
    if (!shouldScheduleNavigation(url))
        return;

    replacesCurrentItem = replacesCurrentItem || mustReplaceCurrentItem(m_frame);

    // If the URL we're going to navigate to is the same as the current one, except for the
    // fragment part, we don't need to schedule the location change. We'll skip this
    // optimization for cross-origin navigations to minimize the navigator's ability to
    // execute timing attacks.
    if (originDocument->getSecurityOrigin()->canAccess(m_frame->document()->getSecurityOrigin())) {
        KURL parsedURL(ParsedURLString, url);
        if (parsedURL.hasFragmentIdentifier() && equalIgnoringFragmentIdentifier(m_frame->document()->url(), parsedURL)) {
            if (NavigationCounterForUnload::inUnloadHandler())
                Deprecation::countDeprecation(m_frame, UseCounter::UnloadHandler_Navigation);

            FrameLoadRequest request(originDocument, m_frame->document()->completeURL(url), "_self");
            request.setReplacesCurrentItem(replacesCurrentItem);
            if (replacesCurrentItem)
                request.setClientRedirect(ClientRedirectPolicy::ClientRedirect);
            m_frame->loader().load(request);
            return;
        }
    }

    schedule(ScheduledLocationChange::create(originDocument, url, replacesCurrentItem));
}

void NavigationScheduler::schedulePageBlock(Document* originDocument)
{
    ASSERT(m_frame->page());
    const KURL& url = m_frame->document()->url();
    schedule(ScheduledPageBlock::create(originDocument, url));
}

void NavigationScheduler::scheduleFormSubmission(Document* document, FormSubmission* submission)
{
    ASSERT(m_frame->page());
    schedule(ScheduledFormSubmission::create(document, submission, mustReplaceCurrentItem(m_frame)));
}

void NavigationScheduler::scheduleReload()
{
    if (!shouldScheduleReload())
        return;
    if (m_frame->document()->url().isEmpty())
        return;
    schedule(ScheduledReload::create());
}

void NavigationScheduler::navigateTask()
{
    Platform::current()->currentThread()->scheduler()->removePendingNavigation(m_frameType);

    if (!m_frame->page())
        return;
    if (m_frame->page()->defersLoading()) {
        InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
        return;
    }

    ScheduledNavigation* redirect(m_redirect.release());
    redirect->fire(m_frame);
    InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
}

void NavigationScheduler::schedule(ScheduledNavigation* redirect)
{
    ASSERT(m_frame->page());

    // In a back/forward navigation, we sometimes restore history state to iframes, even though the state was generated
    // dynamically and JS will try to put something different in the iframe. In this case, we will load stale things
    // and/or confuse the JS when it shortly thereafter tries to schedule a location change. Let the JS have its way.
    // FIXME: This check seems out of place.
    if (!m_frame->loader().stateMachine()->committedFirstRealDocumentLoad() && m_frame->loader().provisionalDocumentLoader()) {
        m_frame->loader().stopAllLoaders();
        if (!m_frame->host())
            return;
    }

    cancel();
    m_redirect = redirect;
    startTimer();
}

void NavigationScheduler::startTimer()
{
    if (!m_redirect)
        return;

    ASSERT(m_frame->page());
    if (m_navigateTaskFactory->isPending())
        return;
    if (!m_redirect->shouldStartTimer(m_frame))
        return;

    WebScheduler* scheduler = Platform::current()->currentThread()->scheduler();
    scheduler->addPendingNavigation(m_frameType);
    scheduler->loadingTaskRunner()->postDelayedTask(
        BLINK_FROM_HERE, m_navigateTaskFactory->cancelAndCreate(), m_redirect->delay() * 1000.0);

    InspectorInstrumentation::frameScheduledNavigation(m_frame, m_redirect->delay());
}

void NavigationScheduler::cancel()
{
    if (m_navigateTaskFactory->isPending()) {
        Platform::current()->currentThread()->scheduler()->removePendingNavigation(m_frameType);
        InspectorInstrumentation::frameClearedScheduledNavigation(m_frame);
    }
    m_navigateTaskFactory->cancel();
    m_redirect.clear();
}

DEFINE_TRACE(NavigationScheduler)
{
    visitor->trace(m_frame);
    visitor->trace(m_redirect);
}

} // namespace blink
