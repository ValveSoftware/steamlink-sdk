/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
 * Copyright (C) 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#ifndef EmptyClients_h
#define EmptyClients_h

#include "core/editing/UndoStep.h"
#include "core/inspector/InspectorClient.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/BackForwardClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/ContextMenuClient.h"
#include "core/page/DragClient.h"
#include "core/page/EditorClient.h"
#include "core/page/FocusType.h"
#include "core/page/Page.h"
#include "core/page/SpellCheckerClient.h"
#include "core/page/StorageClient.h"
#include "platform/DragImage.h"
#include "platform/geometry/FloatRect.h"
#include "platform/network/ResourceError.h"
#include "platform/text/TextCheckerClient.h"
#include "public/platform/WebScreenInfo.h"
#include "wtf/Forward.h"
#include <v8.h>

/*
 This file holds empty Client stubs for use by WebCore.
 Viewless element needs to create a dummy Page->LocalFrame->FrameView tree for use in parsing or executing JavaScript.
 This tree depends heavily on Clients (usually provided by WebKit classes).

 This file was first created for SVGImage as it had no way to access the current Page (nor should it,
 since Images are not tied to a page).
 See http://bugs.webkit.org/show_bug.cgi?id=5971 for the original discussion about this file.

 Ideally, whenever you change a Client class, you should add a stub here.
 Brittle, yes.  Unfortunate, yes.  Hopefully temporary.
*/

namespace WebCore {

class EmptyChromeClient : public ChromeClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~EmptyChromeClient() { }
    virtual void chromeDestroyed() OVERRIDE { }

    virtual void* webView() const OVERRIDE { return 0; }
    virtual void setWindowRect(const FloatRect&) OVERRIDE { }
    virtual FloatRect windowRect() OVERRIDE { return FloatRect(); }

    virtual FloatRect pageRect() OVERRIDE { return FloatRect(); }

    virtual void focus() OVERRIDE { }

    virtual bool canTakeFocus(FocusType) OVERRIDE { return false; }
    virtual void takeFocus(FocusType) OVERRIDE { }

    virtual void focusedNodeChanged(Node*) OVERRIDE { }
    virtual void focusedFrameChanged(LocalFrame*) OVERRIDE { }
    virtual Page* createWindow(LocalFrame*, const FrameLoadRequest&, const WindowFeatures&, NavigationPolicy, ShouldSendReferrer) OVERRIDE { return 0; }
    virtual void show(NavigationPolicy) OVERRIDE { }

    virtual bool canRunModal() OVERRIDE { return false; }
    virtual void runModal() OVERRIDE { }

    virtual void setToolbarsVisible(bool) OVERRIDE { }
    virtual bool toolbarsVisible() OVERRIDE { return false; }

    virtual void setStatusbarVisible(bool) OVERRIDE { }
    virtual bool statusbarVisible() OVERRIDE { return false; }

    virtual void setScrollbarsVisible(bool) OVERRIDE { }
    virtual bool scrollbarsVisible() OVERRIDE { return false; }

    virtual void setMenubarVisible(bool) OVERRIDE { }
    virtual bool menubarVisible() OVERRIDE { return false; }

    virtual void setResizable(bool) OVERRIDE { }

    virtual bool shouldReportDetailedMessageForSource(const String&) OVERRIDE { return false; }
    virtual void addMessageToConsole(LocalFrame*, MessageSource, MessageLevel, const String&, unsigned, const String&, const String&) OVERRIDE { }

    virtual bool canRunBeforeUnloadConfirmPanel() OVERRIDE { return false; }
    virtual bool runBeforeUnloadConfirmPanel(const String&, LocalFrame*) OVERRIDE { return true; }

    virtual void closeWindowSoon() OVERRIDE { }

    virtual void runJavaScriptAlert(LocalFrame*, const String&) OVERRIDE { }
    virtual bool runJavaScriptConfirm(LocalFrame*, const String&) OVERRIDE { return false; }
    virtual bool runJavaScriptPrompt(LocalFrame*, const String&, const String&, String&) OVERRIDE { return false; }

    virtual bool hasOpenedPopup() const OVERRIDE { return false; }
    virtual PassRefPtr<PopupMenu> createPopupMenu(LocalFrame&, PopupMenuClient*) const OVERRIDE;
    virtual void setPagePopupDriver(PagePopupDriver*) OVERRIDE { }
    virtual void resetPagePopupDriver() OVERRIDE { }

    virtual void setStatusbarText(const String&) OVERRIDE { }

    virtual bool tabsToLinks() OVERRIDE { return false; }

    virtual IntRect windowResizerRect() const OVERRIDE { return IntRect(); }

    virtual void invalidateContentsAndRootView(const IntRect&) OVERRIDE { }
    virtual void invalidateContentsForSlowScroll(const IntRect&) OVERRIDE { }
    virtual void scroll(const IntSize&, const IntRect&, const IntRect&) OVERRIDE { }
    virtual void scheduleAnimation() OVERRIDE { }

    virtual IntRect rootViewToScreen(const IntRect& r) const OVERRIDE { return r; }
    virtual blink::WebScreenInfo screenInfo() const OVERRIDE { return blink::WebScreenInfo(); }
    virtual void contentsSizeChanged(LocalFrame*, const IntSize&) const OVERRIDE { }

    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned) OVERRIDE { }

    virtual void setToolTip(const String&, TextDirection) OVERRIDE { }

    virtual void print(LocalFrame*) OVERRIDE { }

    virtual void enumerateChosenDirectory(FileChooser*) OVERRIDE { }

    virtual PassOwnPtr<ColorChooser> createColorChooser(LocalFrame*, ColorChooserClient*, const Color&) OVERRIDE;
    virtual PassRefPtrWillBeRawPtr<DateTimeChooser> openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&) OVERRIDE;
    virtual void openTextDataListChooser(HTMLInputElement&) OVERRIDE;

    virtual void runOpenPanel(LocalFrame*, PassRefPtr<FileChooser>) OVERRIDE;

    virtual void setCursor(const Cursor&) OVERRIDE { }

    virtual void attachRootGraphicsLayer(GraphicsLayer*) OVERRIDE { }

    virtual void needTouchEvents(bool) OVERRIDE { }
    virtual void setTouchAction(TouchAction touchAction) OVERRIDE { };

    virtual void didAssociateFormControls(const WillBeHeapVector<RefPtrWillBeMember<Element> >&) OVERRIDE { }

    virtual void annotatedRegionsChanged() OVERRIDE { }
    virtual bool paintCustomOverhangArea(GraphicsContext*, const IntRect&, const IntRect&, const IntRect&) OVERRIDE { return false; }
    virtual String acceptLanguages() OVERRIDE;
};

class EmptyFrameLoaderClient FINAL : public FrameLoaderClient {
    WTF_MAKE_NONCOPYABLE(EmptyFrameLoaderClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyFrameLoaderClient() { }
    virtual ~EmptyFrameLoaderClient() {  }

    virtual bool hasWebView() const OVERRIDE { return true; } // mainly for assertions

    virtual Frame* opener() const OVERRIDE { return 0; }
    virtual void setOpener(Frame*) OVERRIDE { }

    virtual Frame* parent() const OVERRIDE { return 0; }
    virtual Frame* top() const OVERRIDE { return 0; }
    virtual Frame* previousSibling() const OVERRIDE { return 0; }
    virtual Frame* nextSibling() const OVERRIDE { return 0; }
    virtual Frame* firstChild() const OVERRIDE { return 0; }
    virtual Frame* lastChild() const OVERRIDE { return 0; }
    virtual void detachedFromParent() OVERRIDE { }

    virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, const ResourceResponse&) OVERRIDE { }
    virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse&) OVERRIDE { }
    virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long) OVERRIDE { }
    virtual void dispatchDidLoadResourceFromMemoryCache(const ResourceRequest&, const ResourceResponse&) OVERRIDE { }

    virtual void dispatchDidHandleOnloadEvents() OVERRIDE { }
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() OVERRIDE { }
    virtual void dispatchWillClose() OVERRIDE { }
    virtual void dispatchDidStartProvisionalLoad() OVERRIDE { }
    virtual void dispatchDidReceiveTitle(const String&) OVERRIDE { }
    virtual void dispatchDidChangeIcons(IconType) OVERRIDE { }
    virtual void dispatchDidCommitLoad(LocalFrame*, HistoryItem*, HistoryCommitType) OVERRIDE { }
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&) OVERRIDE { }
    virtual void dispatchDidFailLoad(const ResourceError&) OVERRIDE { }
    virtual void dispatchDidFinishDocumentLoad() OVERRIDE { }
    virtual void dispatchDidFinishLoad() OVERRIDE { }
    virtual void dispatchDidFirstVisuallyNonEmptyLayout() OVERRIDE { }
    virtual void dispatchDidChangeThemeColor() OVERRIDE { };

    virtual NavigationPolicy decidePolicyForNavigation(const ResourceRequest&, DocumentLoader*, NavigationPolicy) OVERRIDE;

    virtual void dispatchWillSendSubmitEvent(HTMLFormElement*) OVERRIDE;
    virtual void dispatchWillSubmitForm(HTMLFormElement*) OVERRIDE;

    virtual void didStartLoading(LoadStartType) OVERRIDE { }
    virtual void progressEstimateChanged(double) OVERRIDE { }
    virtual void didStopLoading() OVERRIDE { }

    virtual void loadURLExternally(const ResourceRequest&, NavigationPolicy, const String& = String()) OVERRIDE { }

    virtual PassRefPtr<DocumentLoader> createDocumentLoader(LocalFrame*, const ResourceRequest&, const SubstituteData&) OVERRIDE;

    virtual String userAgent(const KURL&) OVERRIDE { return ""; }

    virtual String doNotTrackValue() OVERRIDE { return String(); }

    virtual void transitionToCommittedForNewPage() OVERRIDE { }

    virtual bool navigateBackForward(int offset) const OVERRIDE { return false; }
    virtual void didDisplayInsecureContent() OVERRIDE { }
    virtual void didRunInsecureContent(SecurityOrigin*, const KURL&) OVERRIDE { }
    virtual void didDetectXSS(const KURL&, bool) OVERRIDE { }
    virtual void didDispatchPingLoader(const KURL&) OVERRIDE { }
    virtual void selectorMatchChanged(const Vector<String>&, const Vector<String>&) OVERRIDE { }
    virtual PassRefPtr<LocalFrame> createFrame(const KURL&, const AtomicString&, const Referrer&, HTMLFrameOwnerElement*) OVERRIDE;
    virtual PassRefPtr<Widget> createPlugin(HTMLPlugInElement*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool, DetachedPluginPolicy) OVERRIDE;
    virtual bool canCreatePluginWithoutRenderer(const String& mimeType) const OVERRIDE { return false; }
    virtual PassRefPtr<Widget> createJavaAppletWidget(HTMLAppletElement*, const KURL&, const Vector<String>&, const Vector<String>&) OVERRIDE;

    virtual ObjectContentType objectContentType(const KURL&, const String&, bool) OVERRIDE { return ObjectContentType(); }

    virtual void dispatchDidClearWindowObjectInMainWorld() OVERRIDE { }
    virtual void documentElementAvailable() OVERRIDE { }

    virtual void didCreateScriptContext(v8::Handle<v8::Context>, int extensionGroup, int worldId) OVERRIDE { }
    virtual void willReleaseScriptContext(v8::Handle<v8::Context>, int worldId) OVERRIDE { }
    virtual bool allowScriptExtension(const String& extensionName, int extensionGroup, int worldId) OVERRIDE { return false; }

    virtual blink::WebCookieJar* cookieJar() const OVERRIDE { return 0; }

    virtual void didRequestAutocomplete(HTMLFormElement*) OVERRIDE;

    virtual PassOwnPtr<blink::WebServiceWorkerProvider> createServiceWorkerProvider() OVERRIDE;
    virtual PassOwnPtr<blink::WebApplicationCacheHost> createApplicationCacheHost(blink::WebApplicationCacheHostClient*) OVERRIDE;
};

class EmptyTextCheckerClient FINAL : public TextCheckerClient {
public:
    virtual bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const OVERRIDE { return true; }
    virtual void checkSpellingOfString(const String&, int*, int*) OVERRIDE { }
    virtual String getAutoCorrectSuggestionForMisspelledWord(const String&) OVERRIDE { return String(); }
    virtual void checkGrammarOfString(const String&, Vector<GrammarDetail>&, int*, int*) OVERRIDE { }
    virtual void requestCheckingOfString(PassRefPtr<TextCheckingRequest>) OVERRIDE;
};

class EmptySpellCheckerClient FINAL : public SpellCheckerClient {
    WTF_MAKE_NONCOPYABLE(EmptySpellCheckerClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptySpellCheckerClient() { }
    virtual ~EmptySpellCheckerClient() { }

    virtual bool isContinuousSpellCheckingEnabled() OVERRIDE { return false; }
    virtual void toggleContinuousSpellChecking() OVERRIDE { }
    virtual bool isGrammarCheckingEnabled() OVERRIDE { return false; }

    virtual TextCheckerClient& textChecker() OVERRIDE { return m_textCheckerClient; }

    virtual void updateSpellingUIWithMisspelledWord(const String&) OVERRIDE { }
    virtual void showSpellingUI(bool) OVERRIDE { }
    virtual bool spellingUIIsShowing() OVERRIDE { return false; }

private:
    EmptyTextCheckerClient m_textCheckerClient;
};

class EmptyEditorClient FINAL : public EditorClient {
    WTF_MAKE_NONCOPYABLE(EmptyEditorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyEditorClient() { }
    virtual ~EmptyEditorClient() { }

    virtual void respondToChangedContents() OVERRIDE { }
    virtual void respondToChangedSelection(LocalFrame*, SelectionType) OVERRIDE { }

    virtual bool canCopyCut(LocalFrame*, bool defaultValue) const OVERRIDE { return defaultValue; }
    virtual bool canPaste(LocalFrame*, bool defaultValue) const OVERRIDE { return defaultValue; }

    virtual bool handleKeyboardEvent() OVERRIDE { return false; }
};

class EmptyContextMenuClient FINAL : public ContextMenuClient {
    WTF_MAKE_NONCOPYABLE(EmptyContextMenuClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyContextMenuClient() { }
    virtual ~EmptyContextMenuClient() {  }
    virtual void showContextMenu(const ContextMenu*) OVERRIDE { }
    virtual void clearContextMenu() OVERRIDE { }
};

class EmptyDragClient FINAL : public DragClient {
    WTF_MAKE_NONCOPYABLE(EmptyDragClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyDragClient() { }
    virtual ~EmptyDragClient() {}
    virtual DragDestinationAction actionMaskForDrag(DragData*) OVERRIDE { return DragDestinationActionNone; }
    virtual void startDrag(DragImage*, const IntPoint&, const IntPoint&, Clipboard*, LocalFrame*, bool) OVERRIDE { }
};

class EmptyInspectorClient FINAL : public InspectorClient {
    WTF_MAKE_NONCOPYABLE(EmptyInspectorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyInspectorClient() { }
    virtual ~EmptyInspectorClient() { }

    virtual void highlight() OVERRIDE { }
    virtual void hideHighlight() OVERRIDE { }
};

class EmptyBackForwardClient FINAL : public BackForwardClient {
public:
    virtual int backListCount() OVERRIDE { return 0; }
    virtual int forwardListCount() OVERRIDE { return 0; }
    virtual int backForwardListCount() OVERRIDE { return 0; }
};

class EmptyStorageClient FINAL : public StorageClient {
public:
    virtual PassOwnPtr<StorageNamespace> createSessionStorageNamespace() OVERRIDE;
    virtual bool canAccessStorage(LocalFrame*, StorageType) const OVERRIDE { return false; }
};

void fillWithEmptyClients(Page::PageClients&);

}

#endif // EmptyClients_h
