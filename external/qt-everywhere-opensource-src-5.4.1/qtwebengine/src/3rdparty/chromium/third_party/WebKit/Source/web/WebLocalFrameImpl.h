/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebLocalFrameImpl_h
#define WebLocalFrameImpl_h

#include "core/frame/LocalFrame.h"
#include "platform/geometry/FloatRect.h"
#include "public/platform/WebFileSystemType.h"
#include "public/web/WebLocalFrame.h"
#include "web/FrameLoaderClientImpl.h"
#include "web/NotificationPresenterImpl.h"
#include "web/UserMediaClientImpl.h"
#include "wtf/Compiler.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {
class GraphicsContext;
class HTMLInputElement;
class HistoryItem;
class IntSize;
class KURL;
class Node;
class Range;
class SubstituteData;
struct FrameLoadRequest;
struct WindowFeatures;
}

namespace blink {
class ChromePrintContext;
class GeolocationClientProxy;
class SharedWorkerRepositoryClientImpl;
class TextFinder;
class WebDataSourceImpl;
class WebInputElement;
class WebFrameClient;
class WebPerformance;
class WebPlugin;
class WebPluginContainerImpl;
class WebView;
class WebViewImpl;
struct WebPrintParams;

template <typename T> class WebVector;

// Implementation of WebFrame, note that this is a reference counted object.
class WebLocalFrameImpl FINAL
    : public WebLocalFrame
    , public RefCounted<WebLocalFrameImpl> {
public:
    // WebFrame methods:
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
    virtual void setOpener(WebFrame*) OVERRIDE;
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

    // WebLocalFrame methods:
    virtual void addStyleSheetByURL(const WebString& url) OVERRIDE;

    void willDetachParent();

    static WebLocalFrameImpl* create(WebFrameClient*);
    virtual ~WebLocalFrameImpl();

    // Called by the WebViewImpl to initialize the main frame for the page.
    void initializeAsMainFrame(WebCore::Page*);

    PassRefPtr<WebCore::LocalFrame> createChildFrame(
        const WebCore::FrameLoadRequest&, WebCore::HTMLFrameOwnerElement*);

    void didChangeContentsSize(const WebCore::IntSize&);

    void createFrameView();

    static WebLocalFrameImpl* fromFrame(WebCore::LocalFrame*);
    static WebLocalFrameImpl* fromFrame(WebCore::LocalFrame&);
    static WebLocalFrameImpl* fromFrameOwnerElement(WebCore::Element*);

    // If the frame hosts a PluginDocument, this method returns the WebPluginContainerImpl
    // that hosts the plugin.
    static WebPluginContainerImpl* pluginContainerFromFrame(WebCore::LocalFrame*);

    // If the frame hosts a PluginDocument, this method returns the WebPluginContainerImpl
    // that hosts the plugin. If the provided node is a plugin, then it runs its
    // WebPluginContainerImpl.
    static WebPluginContainerImpl* pluginContainerFromNode(WebCore::LocalFrame*, const WebNode&);

    WebViewImpl* viewImpl() const;

    WebCore::FrameView* frameView() const { return frame() ? frame()->view() : 0; }

    // Getters for the impls corresponding to Get(Provisional)DataSource. They
    // may return 0 if there is no corresponding data source.
    WebDataSourceImpl* dataSourceImpl() const;
    WebDataSourceImpl* provisionalDataSourceImpl() const;

    // Returns which frame has an active match. This function should only be
    // called on the main frame, as it is the only frame keeping track. Returned
    // value can be 0 if no frame has an active match.
    WebLocalFrameImpl* activeMatchFrame() const;

    // Returns the active match in the current frame. Could be a null range if
    // the local frame has no active match.
    WebCore::Range* activeMatch() const;

    // When a Find operation ends, we want to set the selection to what was active
    // and set focus to the first focusable node we find (starting with the first
    // node in the matched range and going up the inheritance chain). If we find
    // nothing to focus we focus the first focusable node in the range. This
    // allows us to set focus to a link (when we find text inside a link), which
    // allows us to navigate by pressing Enter after closing the Find box.
    void setFindEndstateFocusAndSelection();

    void didFail(const WebCore::ResourceError&, bool wasProvisional);

    // Sets whether the WebLocalFrameImpl allows its document to be scrolled.
    // If the parameter is true, allow the document to be scrolled.
    // Otherwise, disallow scrolling.
    virtual void setCanHaveScrollbars(bool) OVERRIDE;

    WebCore::LocalFrame* frame() const { return m_frame.get(); }
    WebFrameClient* client() const { return m_client; }
    void setClient(WebFrameClient* client) { m_client = client; }

    WebPermissionClient* permissionClient() { return m_permissionClient; }
    SharedWorkerRepositoryClientImpl* sharedWorkerRepositoryClient() const { return m_sharedWorkerRepositoryClient.get(); }

    void setInputEventsTransformForEmulation(const WebCore::IntSize&, float);

    static void selectWordAroundPosition(WebCore::LocalFrame*, WebCore::VisiblePosition);

    // Returns the text finder object if it already exists.
    // Otherwise creates it and then returns.
    TextFinder& ensureTextFinder();

    // Invalidates vertical scrollbar only.
    void invalidateScrollbar() const;

    // Invalidates both content area and the scrollbar.
    void invalidateAll() const;

    PassRefPtr<WebCore::LocalFrame> initializeAsChildFrame(WebCore::FrameHost*, WebCore::FrameOwner*, const AtomicString& name, const AtomicString& fallbackName);

    // Returns a hit-tested VisiblePosition for the given point
    WebCore::VisiblePosition visiblePositionForWindowPoint(const WebPoint&);

private:
    friend class FrameLoaderClientImpl;

    explicit WebLocalFrameImpl(WebFrameClient*);

    // Sets the local WebCore frame and registers destruction observers.
    void setWebCoreFrame(PassRefPtr<WebCore::LocalFrame>);

    void loadJavaScriptURL(const WebCore::KURL&);

    WebPlugin* focusedPluginIfInputMethodSupported();

    FrameLoaderClientImpl m_frameLoaderClientImpl;

    // The embedder retains a reference to the WebCore LocalFrame while it is active in the DOM. This
    // reference is released when the frame is removed from the DOM or the entire page is closed.
    // FIXME: These will need to change to WebFrame when we introduce WebFrameProxy.
    RefPtr<WebCore::LocalFrame> m_frame;

    // Indicate whether the current LocalFrame is local or remote. Remote frames are
    // rendered in a different process from their parent frames.
    bool m_isRemote;

    WebFrameClient* m_client;
    WebPermissionClient* m_permissionClient;
    OwnPtr<SharedWorkerRepositoryClientImpl> m_sharedWorkerRepositoryClient;

    // Will be initialized after first call to find() or scopeStringMatches().
    OwnPtr<TextFinder> m_textFinder;

    // Valid between calls to BeginPrint() and EndPrint(). Containts the print
    // information. Is used by PrintPage().
    OwnPtr<ChromePrintContext> m_printContext;

    // Stores the additional input events offset and scale when device metrics emulation is enabled.
    WebCore::IntSize m_inputEventsOffsetForEmulation;
    float m_inputEventsScaleFactorForEmulation;

    UserMediaClientImpl m_userMediaClientImpl;

    OwnPtr<GeolocationClientProxy> m_geolocationClientProxy;
};

DEFINE_TYPE_CASTS(WebLocalFrameImpl, WebFrame, frame, frame->isWebLocalFrame(), frame.isWebLocalFrame());

} // namespace blink

#endif
