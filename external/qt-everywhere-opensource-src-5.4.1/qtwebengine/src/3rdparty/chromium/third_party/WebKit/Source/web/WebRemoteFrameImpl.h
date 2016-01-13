// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrameImpl_h
#define WebRemoteFrameImpl_h

#include "public/web/WebRemoteFrame.h"
#include "web/RemoteFrameClient.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"

namespace WebCore {
class FrameOwner;
class Page;
class RemoteFrame;
}

namespace blink {

class WebRemoteFrameImpl : public WebRemoteFrame, public RefCounted<WebRemoteFrameImpl> {
public:
    WebRemoteFrameImpl();
    virtual ~WebRemoteFrameImpl();

    // WebRemoteFrame methods.
    virtual bool isWebLocalFrame() const OVERRIDE;
    virtual WebLocalFrame* toWebLocalFrame() OVERRIDE;
    virtual bool isWebRemoteFrame() const OVERRIDE;
    virtual WebRemoteFrame* toWebRemoteFrame() OVERRIDE;
    virtual void close() OVERRIDE;
    virtual WebString uniqueName() const OVERRIDE;
    virtual WebString assignedName() const OVERRIDE;
    virtual void setName(const WebString&) OVERRIDE;
    virtual WebVector<WebIconURL> iconURLs(int iconTypesMask) const OVERRIDE;
    virtual void setIsRemote(bool) OVERRIDE;
    virtual void setRemoteWebLayer(WebLayer*) OVERRIDE;
    virtual void setPermissionClient(WebPermissionClient*) OVERRIDE;
    virtual void setSharedWorkerRepositoryClient(WebSharedWorkerRepositoryClient*) OVERRIDE;
    virtual void setCanHaveScrollbars(bool) OVERRIDE;
    virtual WebSize scrollOffset() const OVERRIDE;
    virtual void setScrollOffset(const WebSize&) OVERRIDE;
    virtual WebSize minimumScrollOffset() const OVERRIDE;
    virtual WebSize maximumScrollOffset() const OVERRIDE;
    virtual WebSize contentsSize() const OVERRIDE;
    virtual bool hasVisibleContent() const OVERRIDE;
    virtual WebRect visibleContentRect() const OVERRIDE;
    virtual bool hasHorizontalScrollbar() const OVERRIDE;
    virtual bool hasVerticalScrollbar() const OVERRIDE;
    virtual WebView* view() const OVERRIDE;
    virtual void removeChild(WebFrame*) OVERRIDE;
    virtual WebDocument document() const OVERRIDE;
    virtual WebPerformance performance() const OVERRIDE;
    virtual bool dispatchBeforeUnloadEvent() OVERRIDE;
    virtual void dispatchUnloadEvent() OVERRIDE;
    virtual NPObject* windowObject() const OVERRIDE;
    virtual void bindToWindowObject(const WebString& name, NPObject*) OVERRIDE;
    virtual void bindToWindowObject(const WebString& name, NPObject*, void*) OVERRIDE;
    virtual void executeScript(const WebScriptSource&) OVERRIDE;
    virtual void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sources, unsigned numSources,
        int extensionGroup) OVERRIDE;
    virtual void setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin&) OVERRIDE;
    virtual void setIsolatedWorldContentSecurityPolicy(int worldID, const WebString&) OVERRIDE;
    virtual void addMessageToConsole(const WebConsoleMessage&) OVERRIDE;
    virtual void collectGarbage() OVERRIDE;
    virtual bool checkIfRunInsecureContent(const WebURL&) const OVERRIDE;
    virtual v8::Handle<v8::Value> executeScriptAndReturnValue(
        const WebScriptSource&) OVERRIDE;
    virtual void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sourcesIn, unsigned numSources,
        int extensionGroup, WebVector<v8::Local<v8::Value> >* results) OVERRIDE;
    virtual v8::Handle<v8::Value> callFunctionEvenIfScriptDisabled(
        v8::Handle<v8::Function>,
        v8::Handle<v8::Value>,
        int argc,
        v8::Handle<v8::Value> argv[]) OVERRIDE;
    virtual v8::Local<v8::Context> mainWorldScriptContext() const OVERRIDE;
    virtual void reload(bool ignoreCache) OVERRIDE;
    virtual void reloadWithOverrideURL(const WebURL& overrideUrl, bool ignoreCache) OVERRIDE;
    virtual void loadRequest(const WebURLRequest&) OVERRIDE;
    virtual void loadHistoryItem(const WebHistoryItem&, WebHistoryLoadType, WebURLRequest::CachePolicy) OVERRIDE;
    virtual void loadData(
        const WebData&, const WebString& mimeType, const WebString& textEncoding,
        const WebURL& baseURL, const WebURL& unreachableURL, bool replace) OVERRIDE;
    virtual void loadHTMLString(
        const WebData& html, const WebURL& baseURL, const WebURL& unreachableURL,
        bool replace) OVERRIDE;
    virtual bool isLoading() const OVERRIDE;
    virtual void stopLoading() OVERRIDE;
    virtual WebDataSource* provisionalDataSource() const OVERRIDE;
    virtual WebDataSource* dataSource() const OVERRIDE;
    virtual void enableViewSourceMode(bool enable) OVERRIDE;
    virtual bool isViewSourceModeEnabled() const OVERRIDE;
    virtual void setReferrerForRequest(WebURLRequest&, const WebURL& referrer) OVERRIDE;
    virtual void dispatchWillSendRequest(WebURLRequest&) OVERRIDE;
    virtual WebURLLoader* createAssociatedURLLoader(const WebURLLoaderOptions&) OVERRIDE;
    virtual unsigned unloadListenerCount() const OVERRIDE;
    virtual void replaceSelection(const WebString&) OVERRIDE;
    virtual void insertText(const WebString&) OVERRIDE;
    virtual void setMarkedText(const WebString&, unsigned location, unsigned length) OVERRIDE;
    virtual void unmarkText() OVERRIDE;
    virtual bool hasMarkedText() const OVERRIDE;
    virtual WebRange markedRange() const OVERRIDE;
    virtual bool firstRectForCharacterRange(unsigned location, unsigned length, WebRect&) const OVERRIDE;
    virtual size_t characterIndexForPoint(const WebPoint&) const OVERRIDE;
    virtual bool executeCommand(const WebString&, const WebNode& = WebNode()) OVERRIDE;
    virtual bool executeCommand(const WebString&, const WebString& value, const WebNode& = WebNode()) OVERRIDE;
    virtual bool isCommandEnabled(const WebString&) const OVERRIDE;
    virtual void enableContinuousSpellChecking(bool) OVERRIDE;
    virtual bool isContinuousSpellCheckingEnabled() const OVERRIDE;
    virtual void requestTextChecking(const WebElement&) OVERRIDE;
    virtual void replaceMisspelledRange(const WebString&) OVERRIDE;
    virtual void removeSpellingMarkers() OVERRIDE;
    virtual bool hasSelection() const OVERRIDE;
    virtual WebRange selectionRange() const OVERRIDE;
    virtual WebString selectionAsText() const OVERRIDE;
    virtual WebString selectionAsMarkup() const OVERRIDE;
    virtual bool selectWordAroundCaret() OVERRIDE;
    virtual void selectRange(const WebPoint& base, const WebPoint& extent) OVERRIDE;
    virtual void selectRange(const WebRange&) OVERRIDE;
    virtual void moveRangeSelection(const WebPoint& base, const WebPoint& extent) OVERRIDE;
    virtual void moveCaretSelection(const WebPoint&) OVERRIDE;
    virtual bool setEditableSelectionOffsets(int start, int end) OVERRIDE;
    virtual bool setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines) OVERRIDE;
    virtual void extendSelectionAndDelete(int before, int after) OVERRIDE;
    virtual void setCaretVisible(bool) OVERRIDE;
    virtual int printBegin(const WebPrintParams&, const WebNode& constrainToNode) OVERRIDE;
    virtual float printPage(int pageToPrint, WebCanvas*) OVERRIDE;
    virtual float getPrintPageShrink(int page) OVERRIDE;
    virtual void printEnd() OVERRIDE;
    virtual bool isPrintScalingDisabledForPlugin(const WebNode&) OVERRIDE;
    virtual bool hasCustomPageSizeStyle(int pageIndex) OVERRIDE;
    virtual bool isPageBoxVisible(int pageIndex) OVERRIDE;
    virtual void pageSizeAndMarginsInPixels(
        int pageIndex,
        WebSize& pageSize,
        int& marginTop,
        int& marginRight,
        int& marginBottom,
        int& marginLeft) OVERRIDE;
    virtual WebString pageProperty(const WebString& propertyName, int pageIndex) OVERRIDE;
    virtual void printPagesWithBoundaries(WebCanvas*, const WebSize&) OVERRIDE;
    virtual bool find(
        int identifier, const WebString& searchText, const WebFindOptions&,
        bool wrapWithinFrame, WebRect* selectionRect) OVERRIDE;
    virtual void stopFinding(bool clearSelection) OVERRIDE;
    virtual void scopeStringMatches(
        int identifier, const WebString& searchText, const WebFindOptions&,
        bool reset) OVERRIDE;
    virtual void cancelPendingScopingEffort() OVERRIDE;
    virtual void increaseMatchCount(int count, int identifier) OVERRIDE;
    virtual void resetMatchCount() OVERRIDE;
    virtual int findMatchMarkersVersion() const OVERRIDE;
    virtual WebFloatRect activeFindMatchRect() OVERRIDE;
    virtual void findMatchRects(WebVector<WebFloatRect>&) OVERRIDE;
    virtual int selectNearestFindMatch(const WebFloatPoint&, WebRect* selectionRect) OVERRIDE;
    virtual void setTickmarks(const WebVector<WebRect>&) OVERRIDE;

    virtual void sendOrientationChangeEvent() OVERRIDE;

    virtual void dispatchMessageEventWithOriginCheck(
        const WebSecurityOrigin& intendedTargetOrigin,
        const WebDOMEvent&) OVERRIDE;

    virtual WebString contentAsText(size_t maxChars) const OVERRIDE;
    virtual WebString contentAsMarkup() const OVERRIDE;
    virtual WebString renderTreeAsText(RenderAsTextControls toShow = RenderAsTextNormal) const OVERRIDE;
    virtual WebString markerTextForListItem(const WebElement&) const OVERRIDE;
    virtual WebRect selectionBoundsRect() const OVERRIDE;

    virtual bool selectionStartHasSpellingMarkerFor(int from, int length) const OVERRIDE;
    virtual WebString layerTreeAsText(bool showDebugInfo = false) const OVERRIDE;

    virtual WebLocalFrame* createLocalChild(const WebString& name, WebFrameClient*) OVERRIDE;
    virtual WebRemoteFrame* createRemoteChild(const WebString& name, WebFrameClient*) OVERRIDE;

    void initializeAsMainFrame(WebCore::Page*);

    void setWebCoreFrame(PassRefPtr<WebCore::RemoteFrame>);
    WebCore::RemoteFrame* frame() const { return m_frame.get(); }

    static WebRemoteFrameImpl* fromFrame(WebCore::RemoteFrame&);

private:
    RemoteFrameClient m_frameClient;
    RefPtr<WebCore::RemoteFrame> m_frame;

    HashMap<WebFrame*, OwnPtr<WebCore::FrameOwner> > m_ownersForChildren;
};

DEFINE_TYPE_CASTS(WebRemoteFrameImpl, WebFrame, frame, frame->isWebRemoteFrame(), frame.isWebRemoteFrame());

} // namespace blink

#endif // WebRemoteFrameImpl_h
