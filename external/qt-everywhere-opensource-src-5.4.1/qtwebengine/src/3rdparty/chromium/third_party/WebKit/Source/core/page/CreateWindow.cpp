/*
 * Copyright (C) 2006, 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/page/CreateWindow.h"

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/WindowFeatures.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace WebCore {

static LocalFrame* createWindow(LocalFrame& openerFrame, LocalFrame& lookupFrame, const FrameLoadRequest& request, const WindowFeatures& features, NavigationPolicy policy, ShouldSendReferrer shouldSendReferrer, bool& created)
{
    ASSERT(!features.dialog || request.frameName().isEmpty());

    if (!request.frameName().isEmpty() && request.frameName() != "_blank" && policy == NavigationPolicyIgnore) {
        if (LocalFrame* frame = lookupFrame.loader().findFrameForNavigation(request.frameName(), openerFrame.document())) {
            if (request.frameName() != "_self")
                frame->page()->focusController().setFocusedFrame(frame);
            created = false;
            return frame;
        }
    }

    // Sandboxed frames cannot open new auxiliary browsing contexts.
    if (openerFrame.document()->isSandboxed(SandboxPopups)) {
        // FIXME: This message should be moved off the console once a solution to https://bugs.webkit.org/show_bug.cgi?id=103274 exists.
        openerFrame.document()->addConsoleMessage(SecurityMessageSource, ErrorMessageLevel, "Blocked opening '" + request.resourceRequest().url().elidedString() + "' in a new window because the request was made in a sandboxed frame whose 'allow-popups' permission is not set.");
        return 0;
    }

    if (openerFrame.settings() && !openerFrame.settings()->supportsMultipleWindows()) {
        created = false;
        if (!openerFrame.tree().top()->isLocalFrame())
            return 0;
        return toLocalFrame(openerFrame.tree().top());
    }

    Page* oldPage = openerFrame.page();
    if (!oldPage)
        return 0;

    Page* page = oldPage->chrome().client().createWindow(&openerFrame, request, features, policy, shouldSendReferrer);
    if (!page || !page->mainFrame()->isLocalFrame())
        return 0;
    FrameHost* host = &page->frameHost();

    ASSERT(page->mainFrame());
    LocalFrame& frame = *page->deprecatedLocalMainFrame();

    if (request.frameName() != "_blank")
        frame.tree().setName(request.frameName());

    host->chrome().setWindowFeatures(features);

    // 'x' and 'y' specify the location of the window, while 'width' and 'height'
    // specify the size of the viewport. We can only resize the window, so adjust
    // for the difference between the window size and the viewport size.

    FloatRect windowRect = host->chrome().windowRect();
    FloatSize viewportSize = host->chrome().pageRect().size();

    if (features.xSet)
        windowRect.setX(features.x);
    if (features.ySet)
        windowRect.setY(features.y);
    if (features.widthSet)
        windowRect.setWidth(features.width + (windowRect.width() - viewportSize.width()));
    if (features.heightSet)
        windowRect.setHeight(features.height + (windowRect.height() - viewportSize.height()));

    // Ensure non-NaN values, minimum size as well as being within valid screen area.
    FloatRect newWindowRect = LocalDOMWindow::adjustWindowRect(frame, windowRect);

    host->chrome().setWindowRect(newWindowRect);
    host->chrome().show(policy);

    created = true;
    return &frame;
}

LocalFrame* createWindow(const String& urlString, const AtomicString& frameName, const WindowFeatures& windowFeatures,
    LocalDOMWindow& callingWindow, LocalFrame& firstFrame, LocalFrame& openerFrame, LocalDOMWindow::PrepareDialogFunction function, void* functionContext)
{
    LocalFrame* activeFrame = callingWindow.frame();
    ASSERT(activeFrame);

    KURL completedURL = urlString.isEmpty() ? KURL(ParsedURLString, emptyString()) : firstFrame.document()->completeURL(urlString);
    if (!completedURL.isEmpty() && !completedURL.isValid()) {
        // Don't expose client code to invalid URLs.
        callingWindow.printErrorMessage("Unable to open a window with invalid URL '" + completedURL.string() + "'.\n");
        return 0;
    }

    // For whatever reason, Firefox uses the first frame to determine the outgoingReferrer. We replicate that behavior here.
    Referrer referrer(SecurityPolicy::generateReferrerHeader(firstFrame.document()->referrerPolicy(), completedURL, firstFrame.document()->outgoingReferrer()), firstFrame.document()->referrerPolicy());

    ResourceRequest request(completedURL, referrer);
    FrameLoader::addHTTPOriginIfNeeded(request, AtomicString(firstFrame.document()->outgoingOrigin()));
    FrameLoadRequest frameRequest(callingWindow.document(), request, frameName);

    // We pass the opener frame for the lookupFrame in case the active frame is different from
    // the opener frame, and the name references a frame relative to the opener frame.
    bool created;
    LocalFrame* newFrame = createWindow(*activeFrame, openerFrame, frameRequest, windowFeatures, NavigationPolicyIgnore, MaybeSendReferrer, created);
    if (!newFrame)
        return 0;

    if (newFrame != &openerFrame && newFrame != openerFrame.tree().top())
        newFrame->loader().forceSandboxFlags(openerFrame.document()->sandboxFlags());

    newFrame->loader().setOpener(&openerFrame);

    if (newFrame->domWindow()->isInsecureScriptAccess(callingWindow, completedURL))
        return newFrame;

    if (function)
        function(newFrame->domWindow(), functionContext);

    if (created) {
        FrameLoadRequest request(callingWindow.document(), ResourceRequest(completedURL, referrer));
        newFrame->loader().load(request);
    } else if (!urlString.isEmpty()) {
        newFrame->navigationScheduler().scheduleLocationChange(callingWindow.document(), completedURL.string(), referrer, false);
    }
    return newFrame;
}

void createWindowForRequest(const FrameLoadRequest& request, LocalFrame& openerFrame, NavigationPolicy policy, ShouldSendReferrer shouldSendReferrer)
{
    if (openerFrame.document()->pageDismissalEventBeingDispatched() != Document::NoDismissal)
        return;

    if (openerFrame.document() && openerFrame.document()->isSandboxed(SandboxPopups))
        return;

    if (!LocalDOMWindow::allowPopUp(openerFrame))
        return;

    if (policy == NavigationPolicyCurrentTab)
        policy = NavigationPolicyNewForegroundTab;

    WindowFeatures features;
    bool created;
    LocalFrame* newFrame = createWindow(openerFrame, openerFrame, request, features, policy, shouldSendReferrer, created);
    if (!newFrame)
        return;
    if (shouldSendReferrer == MaybeSendReferrer) {
        newFrame->loader().setOpener(&openerFrame);
        newFrame->document()->setReferrerPolicy(openerFrame.document()->referrerPolicy());
    }
    FrameLoadRequest newRequest(0, request.resourceRequest());
    newRequest.setFormState(request.formState());
    newFrame->loader().load(newRequest);
}

} // namespace WebCore
