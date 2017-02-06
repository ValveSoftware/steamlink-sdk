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

// How ownership works
// -------------------
//
// Big oh represents a refcounted relationship: owner O--- ownee
//
// WebView (for the toplevel frame only)
//    O
//    |           WebFrame
//    |              O
//    |              |
//   Page O------- LocalFrame (m_mainFrame) O-------O FrameView
//                   ||
//                   ||
//               FrameLoader
//
// FrameLoader and LocalFrame are formerly one object that was split apart because
// it got too big. They basically have the same lifetime, hence the double line.
//
// From the perspective of the embedder, WebFrame is simply an object that it
// allocates by calling WebFrame::create() and must be freed by calling close().
// Internally, WebFrame is actually refcounted and it holds a reference to its
// corresponding LocalFrame in blink.
//
// Oilpan: the middle objects + Page in the above diagram are Oilpan heap allocated,
// WebView and FrameView are currently not. In terms of ownership and control, the
// relationships stays the same, but the references from the off-heap WebView to the
// on-heap Page is handled by a Persistent<>, not a RefPtr<>. Similarly, the mutual
// strong references between the on-heap LocalFrame and the off-heap FrameView
// is through a RefPtr (from LocalFrame to FrameView), and a Persistent refers
// to the LocalFrame in the other direction.
//
// From the embedder's point of view, the use of Oilpan brings no changes. close()
// must still be used to signal that the embedder is through with the WebFrame.
// Calling it will bring about the release and finalization of the frame object,
// and everything underneath.
//
// How frames are destroyed
// ------------------------
//
// The main frame is never destroyed and is re-used. The FrameLoader is re-used
// and a reference to the main frame is kept by the Page.
//
// When frame content is replaced, all subframes are destroyed. This happens
// in Frame::detachChildren for each subframe in a pre-order depth-first
// traversal. Note that child node order may not match DOM node order!
// detachChildren() (virtually) calls Frame::detach(), which again calls
// FrameLoaderClient::detached(). This triggers WebFrame to clear its reference to
// LocalFrame. FrameLoaderClient::detached() also notifies the embedder via WebFrameClient
// that the frame is detached. Most embedders will invoke close() on the WebFrame
// at this point, triggering its deletion unless something else is still retaining a reference.
//
// The client is expected to be set whenever the WebLocalFrameImpl is attached to
// the DOM.

#include "web/WebLocalFrameImpl.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/IconURL.h"
#include "core/dom/MessagePort.h"
#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/SuspendableTask.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/SubstituteData.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/PluginDocument.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/HistoryItem.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/NavigationScheduler.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/PrintContext.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/TransformRecorder.h"
#include "core/style/StyleInheritedData.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "modules/app_banner/AppBannerController.h"
#include "modules/audio_output_devices/AudioOutputDeviceClient.h"
#include "modules/bluetooth/BluetoothSupplement.h"
#include "modules/installedapp/InstalledAppController.h"
#include "modules/notifications/NotificationPermissionClient.h"
#include "modules/permissions/PermissionController.h"
#include "modules/presentation/PresentationController.h"
#include "modules/push_messaging/PushController.h"
#include "modules/screen_orientation/ScreenOrientationController.h"
#include "modules/vr/VRController.h"
#include "modules/wake_lock/ScreenWakeLock.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "platform/UserGestureIndicator.h"
#include "platform/clipboard/ClipboardUtilities.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/DisplayItemCacheSkipper.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/heap/Handle.h"
#include "platform/network/ResourceRequest.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/ServiceRegistry.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebSuspendableTask.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebVector.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebConsoleMessage.h"
#include "public/web/WebDOMEvent.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFindOptions.h"
#include "public/web/WebFormElement.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebFrameOwnerProperties.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebIconURL.h"
#include "public/web/WebInputElement.h"
#include "public/web/WebKit.h"
#include "public/web/WebNode.h"
#include "public/web/WebPerformance.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebPrintParams.h"
#include "public/web/WebPrintPresetOptions.h"
#include "public/web/WebRange.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSerializedScriptValue.h"
#include "public/web/WebTreeScopeType.h"
#include "skia/ext/platform_canvas.h"
#include "web/AssociatedURLLoader.h"
#include "web/AudioOutputDeviceClientImpl.h"
#include "web/CompositionUnderlineVectorBuilder.h"
#include "web/FindInPageCoordinates.h"
#include "web/IndexedDBClientImpl.h"
#include "web/LocalFileSystemClient.h"
#include "web/MIDIClientProxy.h"
#include "web/NavigatorContentUtilsClientImpl.h"
#include "web/NotificationPermissionClientImpl.h"
#include "web/RemoteFrameOwner.h"
#include "web/SharedWorkerRepositoryClientImpl.h"
#include "web/SuspendableScriptExecutor.h"
#include "web/TextFinder.h"
#include "web/WebDataSourceImpl.h"
#include "web/WebDevToolsAgentImpl.h"
#include "web/WebFrameWidgetImpl.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/CurrentTime.h"
#include "wtf/HashMap.h"
#include "wtf/PtrUtil.h"
#include <algorithm>
#include <memory>

namespace blink {

static int frameCount = 0;

static HeapVector<ScriptSourceCode> createSourcesVector(const WebScriptSource* sourcesIn, unsigned numSources)
{
    HeapVector<ScriptSourceCode> sources;
    sources.append(sourcesIn, numSources);
    return sources;
}

WebPluginContainerImpl* WebLocalFrameImpl::pluginContainerFromFrame(LocalFrame* frame)
{
    if (!frame)
        return 0;
    if (!frame->document() || !frame->document()->isPluginDocument())
        return 0;
    PluginDocument* pluginDocument = toPluginDocument(frame->document());
    return toWebPluginContainerImpl(pluginDocument->pluginWidget());
}

WebPluginContainerImpl* WebLocalFrameImpl::currentPluginContainer(LocalFrame* frame, Node* node)
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame);
    if (pluginContainer)
        return pluginContainer;

    if (!node) {
        DCHECK(frame->document());
        node = frame->document()->focusedElement();
    }
    return toWebPluginContainerImpl(WebNode::pluginContainerFromNode(node));
}

// Simple class to override some of PrintContext behavior. Some of the methods
// made virtual so that they can be overridden by ChromePluginPrintContext.
class ChromePrintContext : public PrintContext {
    WTF_MAKE_NONCOPYABLE(ChromePrintContext);
public:
    ChromePrintContext(LocalFrame* frame)
        : PrintContext(frame)
        , m_printedPageWidth(0)
    {
    }

    ~ChromePrintContext() override {}

    virtual void begin(float width, float height)
    {
        DCHECK(!m_printedPageWidth);
        m_printedPageWidth = width;
        PrintContext::begin(m_printedPageWidth, height);
    }

    virtual float getPageShrink(int pageNumber) const
    {
        IntRect pageRect = m_pageRects[pageNumber];
        return m_printedPageWidth / pageRect.width();
    }

    float spoolSinglePage(WebCanvas* canvas, int pageNumber)
    {
        dispatchEventsForPrintingOnAllFrames();
        if (!frame()->document() || frame()->document()->layoutViewItem().isNull())
            return 0;

        frame()->view()->updateAllLifecyclePhasesExceptPaint();
        if (!frame()->document() || frame()->document()->layoutViewItem().isNull())
            return 0;

        IntRect pageRect = m_pageRects[pageNumber];
        SkPictureBuilder pictureBuilder(pageRect, &skia::GetMetaData(*canvas));
        pictureBuilder.context().setPrinting(true);

        float scale = spoolPage(pictureBuilder, pageNumber);
        pictureBuilder.endRecording()->playback(canvas);
        return scale;
    }

    void spoolAllPagesWithBoundaries(WebCanvas* canvas, const FloatSize& pageSizeInPixels)
    {
        dispatchEventsForPrintingOnAllFrames();
        if (!frame()->document() || frame()->document()->layoutViewItem().isNull())
            return;

        frame()->view()->updateAllLifecyclePhasesExceptPaint();
        if (!frame()->document() || frame()->document()->layoutViewItem().isNull())
            return;

        float pageHeight;
        computePageRects(FloatRect(FloatPoint(0, 0), pageSizeInPixels), 0, 0, 1, pageHeight);

        const float pageWidth = pageSizeInPixels.width();
        size_t numPages = pageRects().size();
        int totalHeight = numPages * (pageSizeInPixels.height() + 1) - 1;
        IntRect allPagesRect(0, 0, pageWidth, totalHeight);

        SkPictureBuilder pictureBuilder(allPagesRect, &skia::GetMetaData(*canvas));
        pictureBuilder.context().setPrinting(true);

        {
            GraphicsContext& context = pictureBuilder.context();
            DisplayItemCacheSkipper skipper(context);

            // Fill the whole background by white.
            {
                DrawingRecorder backgroundRecorder(context, pictureBuilder, DisplayItem::PrintedContentBackground, allPagesRect);
                context.fillRect(FloatRect(0, 0, pageWidth, totalHeight), Color::white);
            }


            int currentHeight = 0;
            for (size_t pageIndex = 0; pageIndex < numPages; pageIndex++) {
                // Draw a line for a page boundary if this isn't the first page.
                if (pageIndex > 0) {
                    DrawingRecorder lineBoundaryRecorder(context, pictureBuilder, DisplayItem::PrintedContentLineBoundary, allPagesRect);
                    context.save();
                    context.setStrokeColor(Color(0, 0, 255));
                    context.setFillColor(Color(0, 0, 255));
                    context.drawLine(IntPoint(0, currentHeight), IntPoint(pageWidth, currentHeight));
                    context.restore();
                }

                AffineTransform transform;
                transform.translate(0, currentHeight);
#if OS(WIN) || OS(MACOSX)
                // Account for the disabling of scaling in spoolPage. In the context
                // of spoolAllPagesWithBoundaries the scale HAS NOT been pre-applied.
                float scale = getPageShrink(pageIndex);
                transform.scale(scale, scale);
#endif
                TransformRecorder transformRecorder(context, pictureBuilder, transform);
                spoolPage(pictureBuilder, pageIndex);

                currentHeight += pageSizeInPixels.height() + 1;
            }
        }
        pictureBuilder.endRecording()->playback(canvas);
    }

protected:
    // Spools the printed page, a subrect of frame(). Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead. Returns the scale to be applied.
    // On Linux, we don't have the problem with NativeTheme, hence we let WebKit
    // do the scaling and ignore the return value.
    virtual float spoolPage(SkPictureBuilder& pictureBuilder, int pageNumber)
    {
        IntRect pageRect = m_pageRects[pageNumber];
        float scale = m_printedPageWidth / pageRect.width();
        GraphicsContext& context = pictureBuilder.context();

        AffineTransform transform;
#if OS(POSIX) && !OS(MACOSX)
        transform.scale(scale);
#endif
        transform.translate(static_cast<float>(-pageRect.x()), static_cast<float>(-pageRect.y()));
        TransformRecorder transformRecorder(context, pictureBuilder, transform);

        ClipRecorder clipRecorder(context, pictureBuilder, DisplayItem::ClipPrintedPage, pageRect);

        frame()->view()->paintContents(context, GlobalPaintNormalPhase, pageRect);

        {
            DrawingRecorder lineBoundaryRecorder(context, pictureBuilder, DisplayItem::PrintedContentDestinationLocations, pageRect);
            outputLinkedDestinations(context, pageRect);
        }

        return scale;
    }

private:
    void dispatchEventsForPrintingOnAllFrames()
    {
        HeapVector<Member<Document>> documents;
        for (Frame* currentFrame = frame(); currentFrame; currentFrame = currentFrame->tree().traverseNext(frame())) {
            if (currentFrame->isLocalFrame())
                documents.append(toLocalFrame(currentFrame)->document());
        }

        for (auto& doc : documents)
            doc->dispatchEventsForPrinting();
    }

    // Set when printing.
    float m_printedPageWidth;
};

// Simple class to override some of PrintContext behavior. This is used when
// the frame hosts a plugin that supports custom printing. In this case, we
// want to delegate all printing related calls to the plugin.
class ChromePluginPrintContext final : public ChromePrintContext {
public:
    ChromePluginPrintContext(LocalFrame* frame, WebPluginContainerImpl* plugin, const WebPrintParams& printParams)
        : ChromePrintContext(frame), m_plugin(plugin), m_printParams(printParams)
    {
    }

    ~ChromePluginPrintContext() override {}

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_plugin);
        ChromePrintContext::trace(visitor);
    }

    void begin(float width, float height) override
    {
    }

    void end() override
    {
        m_plugin->printEnd();
    }

    float getPageShrink(int pageNumber) const override
    {
        // We don't shrink the page (maybe we should ask the widget ??)
        return 1.0;
    }

    void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight) override
    {
        m_printParams.printContentArea = IntRect(printRect);
        m_pageRects.fill(IntRect(printRect), m_plugin->printBegin(m_printParams));
    }

    void computePageRectsWithPageSize(const FloatSize& pageSizeInPixels) override
    {
        NOTREACHED();
    }

protected:
    // Spools the printed page, a subrect of frame(). Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead. Returns the scale to be applied.
    float spoolPage(SkPictureBuilder& pictureBuilder, int pageNumber) override
    {
        IntRect pageRect = m_pageRects[pageNumber];
        m_plugin->printPage(pageNumber, pictureBuilder.context(), pageRect);

        return 1.0;
    }

private:
    // Set when printing.
    Member<WebPluginContainerImpl> m_plugin;
    WebPrintParams m_printParams;
};

static WebDataSource* DataSourceForDocLoader(DocumentLoader* loader)
{
    return loader ? WebDataSourceImpl::fromDocumentLoader(loader) : 0;
}

// WebSuspendableTaskWrapper --------------------------------------------------

class WebSuspendableTaskWrapper: public SuspendableTask {
public:
    static std::unique_ptr<WebSuspendableTaskWrapper> create(std::unique_ptr<WebSuspendableTask> task)
    {
        return wrapUnique(new WebSuspendableTaskWrapper(std::move(task)));
    }

    void run() override
    {
        m_task->run();
    }

    void contextDestroyed() override
    {
        m_task->contextDestroyed();
    }

private:
    explicit WebSuspendableTaskWrapper(std::unique_ptr<WebSuspendableTask> task)
        : m_task(std::move(task))
    {
    }

    std::unique_ptr<WebSuspendableTask> m_task;
};

// WebFrame -------------------------------------------------------------------

int WebFrame::instanceCount()
{
    return frameCount;
}

WebLocalFrame* WebLocalFrame::frameForCurrentContext()
{
    v8::Local<v8::Context> context = v8::Isolate::GetCurrent()->GetCurrentContext();
    if (context.IsEmpty())
        return 0;
    return frameForContext(context);
}

WebLocalFrame* WebLocalFrame::frameForContext(v8::Local<v8::Context> context)
{
    return WebLocalFrameImpl::fromFrame(toLocalFrame(toFrameIfNotDetached(context)));
}

WebLocalFrame* WebLocalFrame::fromFrameOwnerElement(const WebElement& element)
{
    return WebLocalFrameImpl::fromFrameOwnerElement(element);
}

bool WebLocalFrameImpl::isWebLocalFrame() const
{
    return true;
}

WebLocalFrame* WebLocalFrameImpl::toWebLocalFrame()
{
    return this;
}

bool WebLocalFrameImpl::isWebRemoteFrame() const
{
    return false;
}

WebRemoteFrame* WebLocalFrameImpl::toWebRemoteFrame()
{
    NOTREACHED();
    return 0;
}

void WebLocalFrameImpl::close()
{
    m_client = nullptr;

    if (m_devToolsAgent)
        m_devToolsAgent.clear();

    m_selfKeepAlive.clear();
}

WebString WebLocalFrameImpl::uniqueName() const
{
    return frame()->tree().uniqueName();
}

WebString WebLocalFrameImpl::assignedName() const
{
    return frame()->tree().name();
}

void WebLocalFrameImpl::setName(const WebString& name)
{
    frame()->tree().setName(name);
}

WebVector<WebIconURL> WebLocalFrameImpl::iconURLs(int iconTypesMask) const
{
    // The URL to the icon may be in the header. As such, only
    // ask the loader for the icon if it's finished loading.
    if (frame()->document()->loadEventFinished())
        return frame()->document()->iconURLs(iconTypesMask);
    return WebVector<WebIconURL>();
}

void WebLocalFrameImpl::setRemoteWebLayer(WebLayer* webLayer)
{
    NOTREACHED();
}

void WebLocalFrameImpl::setContentSettingsClient(WebContentSettingsClient* contentSettingsClient)
{
    m_contentSettingsClient = contentSettingsClient;
}

void WebLocalFrameImpl::setSharedWorkerRepositoryClient(WebSharedWorkerRepositoryClient* client)
{
    m_sharedWorkerRepositoryClient = SharedWorkerRepositoryClientImpl::create(client);
}

ScrollableArea* WebLocalFrameImpl::layoutViewportScrollableArea() const
{
    if (FrameView* view = frameView())
        return view->layoutViewportScrollableArea();
    return nullptr;
}

bool WebLocalFrameImpl::isFocused() const
{
    if (!viewImpl() || !viewImpl()->page())
        return false;

    return this == WebFrame::fromFrame(viewImpl()->page()->focusController().focusedFrame());
}

WebSize WebLocalFrameImpl::scrollOffset() const
{
    if (ScrollableArea* scrollableArea = layoutViewportScrollableArea())
        return toIntSize(scrollableArea->scrollPosition());
    return WebSize();
}

void WebLocalFrameImpl::setScrollOffset(const WebSize& offset)
{
    if (ScrollableArea* scrollableArea = layoutViewportScrollableArea())
        scrollableArea->setScrollPosition(IntPoint(offset.width, offset.height), ProgrammaticScroll);
}

WebSize WebLocalFrameImpl::contentsSize() const
{
    if (FrameView* view = frameView())
        return view->contentsSize();
    return WebSize();
}

bool WebLocalFrameImpl::hasVisibleContent() const
{
    if (LayoutPart* layoutObject = frame()->ownerLayoutObject()) {
        if (layoutObject->style()->visibility() != VISIBLE)
            return false;
    }

    if (FrameView* view = frameView())
        return view->visibleWidth() > 0 && view->visibleHeight() > 0;
    return false;
}

WebRect WebLocalFrameImpl::visibleContentRect() const
{
    if (FrameView* view = frameView())
        return view->visibleContentRect();
    return WebRect();
}

bool WebLocalFrameImpl::hasHorizontalScrollbar() const
{
    return frame() && frame()->view() && frame()->view()->horizontalScrollbar();
}

bool WebLocalFrameImpl::hasVerticalScrollbar() const
{
    return frame() && frame()->view() && frame()->view()->verticalScrollbar();
}

WebView* WebLocalFrameImpl::view() const
{
    return viewImpl();
}

WebDocument WebLocalFrameImpl::document() const
{
    if (!frame() || !frame()->document())
        return WebDocument();
    return WebDocument(frame()->document());
}

WebPerformance WebLocalFrameImpl::performance() const
{
    if (!frame())
        return WebPerformance();
    return WebPerformance(DOMWindowPerformance::performance(*(frame()->domWindow())));
}

void WebLocalFrameImpl::dispatchUnloadEvent()
{
    if (!frame())
        return;
    SubframeLoadingDisabler disabler(frame()->document());
    frame()->loader().dispatchUnloadEvent();
}

void WebLocalFrameImpl::executeScript(const WebScriptSource& source)
{
    DCHECK(frame());
    TextPosition position(OrdinalNumber::fromOneBasedInt(source.startLine), OrdinalNumber::first());
    v8::HandleScope handleScope(toIsolate(frame()));
    frame()->script().executeScriptInMainWorld(ScriptSourceCode(source.code, source.url, position));
}

void WebLocalFrameImpl::executeScriptInIsolatedWorld(int worldID, const WebScriptSource* sourcesIn, unsigned numSources, int extensionGroup)
{
    DCHECK(frame());
    CHECK_GT(worldID, 0);
    CHECK_LT(worldID, EmbedderWorldIdLimit);

    HeapVector<ScriptSourceCode> sources = createSourcesVector(sourcesIn, numSources);
    v8::HandleScope handleScope(toIsolate(frame()));
    frame()->script().executeScriptInIsolatedWorld(worldID, sources, extensionGroup, 0);
}

void WebLocalFrameImpl::setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin& securityOrigin)
{
    DCHECK(frame());
    DOMWrapperWorld::setIsolatedWorldSecurityOrigin(worldID, securityOrigin.get());
}

void WebLocalFrameImpl::setIsolatedWorldContentSecurityPolicy(int worldID, const WebString& policy)
{
    DCHECK(frame());
    DOMWrapperWorld::setIsolatedWorldContentSecurityPolicy(worldID, policy);
}

void WebLocalFrameImpl::setIsolatedWorldHumanReadableName(int worldID, const WebString& humanReadableName)
{
    DCHECK(frame());
    DOMWrapperWorld::setIsolatedWorldHumanReadableName(worldID, humanReadableName);
}

void WebLocalFrameImpl::addMessageToConsole(const WebConsoleMessage& message)
{
    DCHECK(frame());

    MessageLevel webCoreMessageLevel = LogMessageLevel;
    switch (message.level) {
    case WebConsoleMessage::LevelDebug:
        webCoreMessageLevel = DebugMessageLevel;
        break;
    case WebConsoleMessage::LevelLog:
        webCoreMessageLevel = LogMessageLevel;
        break;
    case WebConsoleMessage::LevelWarning:
        webCoreMessageLevel = WarningMessageLevel;
        break;
    case WebConsoleMessage::LevelError:
        webCoreMessageLevel = ErrorMessageLevel;
        break;
    // Unsupported values.
    case WebConsoleMessage::LevelInfo:
    case WebConsoleMessage::LevelRevokedError:
        break;
    }

    frame()->document()->addConsoleMessage(ConsoleMessage::create(OtherMessageSource, webCoreMessageLevel, message.text, SourceLocation::create(message.url, message.lineNumber, message.columnNumber, nullptr)));
}

void WebLocalFrameImpl::collectGarbage()
{
    if (!frame())
        return;
    if (!frame()->settings()->scriptEnabled())
        return;
    V8GCController::collectGarbage(v8::Isolate::GetCurrent());
}

v8::Local<v8::Value> WebLocalFrameImpl::executeScriptAndReturnValue(const WebScriptSource& source)
{
    DCHECK(frame());

    TextPosition position(OrdinalNumber::fromOneBasedInt(source.startLine), OrdinalNumber::first());
    return frame()->script().executeScriptInMainWorldAndReturnValue(ScriptSourceCode(source.code, source.url, position));
}

void WebLocalFrameImpl::requestExecuteScriptAndReturnValue(const WebScriptSource& source, bool userGesture, WebScriptExecutionCallback* callback)
{
    DCHECK(frame());

    SuspendableScriptExecutor::createAndRun(frame(), 0, createSourcesVector(&source, 1), 0, userGesture, callback);
}

void WebLocalFrameImpl::executeScriptInIsolatedWorld(int worldID, const WebScriptSource* sourcesIn, unsigned numSources, int extensionGroup, WebVector<v8::Local<v8::Value>>* results)
{
    DCHECK(frame());
    CHECK_GT(worldID, 0);
    CHECK_LT(worldID, EmbedderWorldIdLimit);

    HeapVector<ScriptSourceCode> sources = createSourcesVector(sourcesIn, numSources);

    if (results) {
        Vector<v8::Local<v8::Value>> scriptResults;
        frame()->script().executeScriptInIsolatedWorld(worldID, sources, extensionGroup, &scriptResults);
        WebVector<v8::Local<v8::Value>> v8Results(scriptResults.size());
        for (unsigned i = 0; i < scriptResults.size(); i++)
            v8Results[i] = v8::Local<v8::Value>::New(toIsolate(frame()), scriptResults[i]);
        results->swap(v8Results);
    } else {
        v8::HandleScope handleScope(toIsolate(frame()));
        frame()->script().executeScriptInIsolatedWorld(worldID, sources, extensionGroup, 0);
    }
}

void WebLocalFrameImpl::requestExecuteScriptInIsolatedWorld(int worldID, const WebScriptSource* sourcesIn, unsigned numSources, int extensionGroup, bool userGesture, WebScriptExecutionCallback* callback)
{
    DCHECK(frame());
    CHECK_GT(worldID, 0);
    CHECK_LT(worldID, EmbedderWorldIdLimit);

    SuspendableScriptExecutor::createAndRun(frame(), worldID, createSourcesVector(sourcesIn, numSources), extensionGroup, userGesture, callback);
}

// TODO(bashi): Consider returning MaybeLocal.
v8::Local<v8::Value> WebLocalFrameImpl::callFunctionEvenIfScriptDisabled(v8::Local<v8::Function> function, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> argv[])
{
    DCHECK(frame());
    v8::Local<v8::Value> result;
    if (!frame()->script().callFunction(function, receiver, argc, static_cast<v8::Local<v8::Value>*>(argv)).ToLocal(&result))
        return v8::Local<v8::Value>();
    return result;
}

v8::Local<v8::Context> WebLocalFrameImpl::mainWorldScriptContext() const
{
    ScriptState* scriptState = ScriptState::forMainWorld(frame());
    DCHECK(scriptState);
    return scriptState->context();
}

v8::Local<v8::Context> WebLocalFrameImpl::isolatedWorldScriptContext(int worldID, int extensionGroup) const
{
    RefPtr<DOMWrapperWorld> world = DOMWrapperWorld::ensureIsolatedWorld(toIsolate(frame()), worldID, extensionGroup);
    ScriptState* scriptState = ScriptState::forWorld(frame(), *world.get());
    ASSERT(scriptState->contextIsValid());
    return scriptState->context();
}

bool WebFrame::scriptCanAccess(WebFrame* target)
{
    return BindingSecurity::shouldAllowAccessToFrame(mainThreadIsolate(), currentDOMWindow(mainThreadIsolate()), target->toImplBase()->frame(), DoNotReportSecurityError);
}

void WebLocalFrameImpl::reload(WebFrameLoadType loadType)
{
    // TODO(clamy): Remove this function once RenderFrame calls load for all
    // requests.
    reloadWithOverrideURL(KURL(), loadType);
}

void WebLocalFrameImpl::reloadWithOverrideURL(const WebURL& overrideUrl, WebFrameLoadType loadType)
{
    // TODO(clamy): Remove this function once RenderFrame calls load for all
    // requests.
    DCHECK(frame());
    DCHECK(loadType == WebFrameLoadType::Reload || loadType == WebFrameLoadType::ReloadBypassingCache);
    WebURLRequest request = requestForReload(loadType, overrideUrl);
    if (request.isNull())
        return;
    load(request, loadType, WebHistoryItem(), WebHistoryDifferentDocumentLoad, false);
}

void WebLocalFrameImpl::reloadImage(const WebNode& webNode)
{
    const Node* node = webNode.constUnwrap<Node>();
    if (isHTMLImageElement(*node)) {
        const HTMLImageElement& imageElement = toHTMLImageElement(*node);
        imageElement.forceReload();
    }
}

void WebLocalFrameImpl::reloadLoFiImages()
{
    frame()->document()->fetcher()->reloadLoFiImages();
}

void WebLocalFrameImpl::loadRequest(const WebURLRequest& request)
{
    // TODO(clamy): Remove this function once RenderFrame calls load for all
    // requests.
    load(request, WebFrameLoadType::Standard, WebHistoryItem(), WebHistoryDifferentDocumentLoad, false);
}

void WebLocalFrameImpl::loadHTMLString(const WebData& data, const WebURL& baseURL, const WebURL& unreachableURL, bool replace)
{
    DCHECK(frame());
    loadData(data, WebString::fromUTF8("text/html"), WebString::fromUTF8("UTF-8"), baseURL, unreachableURL, replace,
        WebFrameLoadType::Standard, WebHistoryItem(), WebHistoryDifferentDocumentLoad, false);
}

void WebLocalFrameImpl::stopLoading()
{
    if (!frame())
        return;
    // FIXME: Figure out what we should really do here. It seems like a bug
    // that FrameLoader::stopLoading doesn't call stopAllLoaders.
    frame()->loader().stopAllLoaders();
}

WebDataSource* WebLocalFrameImpl::provisionalDataSource() const
{
    DCHECK(frame());
    return DataSourceForDocLoader(frame()->loader().provisionalDocumentLoader());
}

WebDataSource* WebLocalFrameImpl::dataSource() const
{
    DCHECK(frame());
    return DataSourceForDocLoader(frame()->loader().documentLoader());
}

void WebLocalFrameImpl::enableViewSourceMode(bool enable)
{
    if (frame())
        frame()->setInViewSourceMode(enable);
}

bool WebLocalFrameImpl::isViewSourceModeEnabled() const
{
    if (!frame())
        return false;
    return frame()->inViewSourceMode();
}

void WebLocalFrameImpl::setReferrerForRequest(WebURLRequest& request, const WebURL& referrerURL)
{
    String referrer = referrerURL.isEmpty() ? frame()->document()->outgoingReferrer() : String(referrerURL.string());
    request.toMutableResourceRequest().setHTTPReferrer(SecurityPolicy::generateReferrer(frame()->document()->getReferrerPolicy(), request.url(), referrer));
}

void WebLocalFrameImpl::dispatchWillSendRequest(WebURLRequest& request)
{
    ResourceResponse response;
    frame()->loader().client()->dispatchWillSendRequest(0, 0, request.toMutableResourceRequest(), response);
}

WebURLLoader* WebLocalFrameImpl::createAssociatedURLLoader(const WebURLLoaderOptions& options)
{
    return new AssociatedURLLoader(this, options);
}

unsigned WebLocalFrameImpl::unloadListenerCount() const
{
    return frame()->localDOMWindow()->pendingUnloadEventListeners();
}

void WebLocalFrameImpl::replaceSelection(const WebString& text)
{
    bool selectReplacement = frame()->editor().behavior().shouldSelectReplacement();
    bool smartReplace = true;
    frame()->editor().replaceSelectionWithText(text, selectReplacement, smartReplace);
}

void WebLocalFrameImpl::insertText(const WebString& text)
{
    if (frame()->inputMethodController().hasComposition())
        frame()->inputMethodController().confirmComposition(text);
    else
        frame()->editor().insertText(text, 0);
}

void WebLocalFrameImpl::setMarkedText(const WebString& text, unsigned location, unsigned length)
{
    Vector<CompositionUnderline> decorations;
    frame()->inputMethodController().setComposition(text, decorations, location, length);
}

void WebLocalFrameImpl::unmarkText()
{
    frame()->inputMethodController().cancelComposition();
}

bool WebLocalFrameImpl::hasMarkedText() const
{
    return frame()->inputMethodController().hasComposition();
}

WebRange WebLocalFrameImpl::markedRange() const
{
    return frame()->inputMethodController().compositionRange();
}

bool WebLocalFrameImpl::firstRectForCharacterRange(unsigned location, unsigned length, WebRect& rectInViewport) const
{
    if ((location + length < location) && (location + length))
        length = 0;

    Element* editable = frame()->selection().rootEditableElementOrDocumentElement();
    if (!editable)
        return false;
    const EphemeralRange range = PlainTextRange(location, location + length).createRange(*editable);
    if (range.isNull())
        return false;
    IntRect intRect = frame()->editor().firstRectForRange(range);
    rectInViewport = WebRect(intRect);
    rectInViewport = frame()->view()->contentsToViewport(rectInViewport);
    return true;
}

size_t WebLocalFrameImpl::characterIndexForPoint(const WebPoint& pointInViewport) const
{
    if (!frame())
        return kNotFound;

    IntPoint point = frame()->view()->viewportToContents(pointInViewport);
    HitTestResult result = frame()->eventHandler().hitTestResultAtPoint(point, HitTestRequest::ReadOnly | HitTestRequest::Active);
    const EphemeralRange range = frame()->rangeForPoint(result.roundedPointInInnerNodeFrame());
    if (range.isNull())
        return kNotFound;
    Element* editable = frame()->selection().rootEditableElementOrDocumentElement();
    DCHECK(editable);
    return PlainTextRange::create(*editable, range).start();
}

bool WebLocalFrameImpl::executeCommand(const WebString& name)
{
    DCHECK(frame());

    if (name.length() <= 2)
        return false;

    // Since we don't have NSControl, we will convert the format of command
    // string and call the function on Editor directly.
    String command = name;

    // Make sure the first letter is upper case.
    command.replace(0, 1, command.substring(0, 1).upper());

    // Remove the trailing ':' if existing.
    if (command[command.length() - 1] == UChar(':'))
        command = command.substring(0, command.length() - 1);

    Node* pluginLookupContextNode = m_contextMenuNode && name == "Copy" ? m_contextMenuNode : nullptr;
    WebPluginContainerImpl* pluginContainer = currentPluginContainer(frame(), pluginLookupContextNode);
    if (pluginContainer && pluginContainer->executeEditCommand(name))
        return true;

    return frame()->editor().executeCommand(command);
}

bool WebLocalFrameImpl::executeCommand(const WebString& name, const WebString& value)
{
    DCHECK(frame());

    WebPluginContainerImpl* pluginContainer = currentPluginContainer(frame());
    if (pluginContainer && pluginContainer->executeEditCommand(name, value))
        return true;

    return frame()->editor().executeCommand(name, value);
}

bool WebLocalFrameImpl::isCommandEnabled(const WebString& name) const
{
    DCHECK(frame());
    return frame()->editor().createCommand(name).isEnabled();
}

void WebLocalFrameImpl::enableContinuousSpellChecking(bool enable)
{
    if (enable == isContinuousSpellCheckingEnabled())
        return;
    frame()->spellChecker().toggleContinuousSpellChecking();
}

bool WebLocalFrameImpl::isContinuousSpellCheckingEnabled() const
{
    return frame()->spellChecker().isContinuousSpellCheckingEnabled();
}

void WebLocalFrameImpl::requestTextChecking(const WebElement& webElement)
{
    if (webElement.isNull())
        return;
    frame()->spellChecker().requestTextChecking(*webElement.constUnwrap<Element>());
}

void WebLocalFrameImpl::replaceMisspelledRange(const WebString& text)
{
    // If this caret selection has two or more markers, this function replace the range covered by the first marker with the specified word as Microsoft Word does.
    if (pluginContainerFromFrame(frame()))
        return;
    frame()->spellChecker().replaceMisspelledRange(text);
}

void WebLocalFrameImpl::removeSpellingMarkers()
{
    frame()->spellChecker().removeSpellingMarkers();
}

bool WebLocalFrameImpl::hasSelection() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->hasSelection();

    // frame()->selection()->isNone() never returns true.
    return frame()->selection().start() != frame()->selection().end();
}

WebRange WebLocalFrameImpl::selectionRange() const
{
    return createRange(frame()->selection().selection().toNormalizedEphemeralRange());
}

WebString WebLocalFrameImpl::selectionAsText() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->selectionAsText();

    String text = frame()->selection().selectedText(TextIteratorEmitsObjectReplacementCharacter);
#if OS(WIN)
    replaceNewlinesWithWindowsStyleNewlines(text);
#endif
    replaceNBSPWithSpace(text);
    return text;
}

WebString WebLocalFrameImpl::selectionAsMarkup() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->selectionAsMarkup();

    return frame()->selection().selectedHTMLForClipboard();
}

void WebLocalFrameImpl::selectWordAroundPosition(LocalFrame* frame, VisiblePosition position)
{
    TRACE_EVENT0("blink", "WebLocalFrameImpl::selectWordAroundPosition");
    frame->selection().selectWordAroundPosition(position);
}

bool WebLocalFrameImpl::selectWordAroundCaret()
{
    TRACE_EVENT0("blink", "WebLocalFrameImpl::selectWordAroundCaret");
    FrameSelection& selection = frame()->selection();
    if (selection.isNone() || selection.isRange())
        return false;
    return frame()->selection().selectWordAroundPosition(selection.selection().visibleStart());
}

void WebLocalFrameImpl::selectRange(const WebPoint& baseInViewport, const WebPoint& extentInViewport)
{
    moveRangeSelection(baseInViewport, extentInViewport);
}

void WebLocalFrameImpl::selectRange(const WebRange& webRange)
{
    TRACE_EVENT0("blink", "WebLocalFrameImpl::selectRange");
    if (Range* range = static_cast<Range*>(webRange))
        frame()->selection().setSelectedRange(range, VP_DEFAULT_AFFINITY, SelectionDirectionalMode::NonDirectional, NotUserTriggered);
}

void WebLocalFrameImpl::moveRangeSelectionExtent(const WebPoint& point)
{
    TRACE_EVENT0("blink", "WebLocalFrameImpl::moveRangeSelectionExtent");
    frame()->selection().moveRangeSelectionExtent(frame()->view()->viewportToContents(point));
}

void WebLocalFrameImpl::moveRangeSelection(const WebPoint& baseInViewport, const WebPoint& extentInViewport, WebFrame::TextGranularity granularity)
{
    TRACE_EVENT0("blink", "WebLocalFrameImpl::moveRangeSelection");
    blink::TextGranularity blinkGranularity = blink::CharacterGranularity;
    if (granularity == WebFrame::WordGranularity)
        blinkGranularity = blink::WordGranularity;
    frame()->selection().moveRangeSelection(
        visiblePositionForViewportPoint(baseInViewport),
        visiblePositionForViewportPoint(extentInViewport),
        blinkGranularity);
}

void WebLocalFrameImpl::moveCaretSelection(const WebPoint& pointInViewport)
{
    TRACE_EVENT0("blink", "WebLocalFrameImpl::moveCaretSelection");
    Element* editable = frame()->selection().rootEditableElement();
    if (!editable)
        return;

    VisiblePosition position = visiblePositionForViewportPoint(pointInViewport);
    frame()->selection().moveTo(position, UserTriggered);
}

bool WebLocalFrameImpl::setEditableSelectionOffsets(int start, int end)
{
    TRACE_EVENT0("blink", "WebLocalFrameImpl::setEditableSelectionOffsets");
    return frame()->inputMethodController().setEditableSelectionOffsets(PlainTextRange(start, end));
}

bool WebLocalFrameImpl::setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines)
{
    TRACE_EVENT0("blink", "WebLocalFrameImpl::setCompositionFromExistingText");
    if (!frame()->editor().canEdit())
        return false;

    InputMethodController& inputMethodController = frame()->inputMethodController();
    inputMethodController.cancelComposition();

    if (compositionStart == compositionEnd)
        return true;

    inputMethodController.setCompositionFromExistingText(CompositionUnderlineVectorBuilder(underlines), compositionStart, compositionEnd);

    return true;
}

void WebLocalFrameImpl::extendSelectionAndDelete(int before, int after)
{
    TRACE_EVENT0("blink", "WebLocalFrameImpl::extendSelectionAndDelete");
    if (WebPlugin* plugin = focusedPluginIfInputMethodSupported()) {
        plugin->extendSelectionAndDelete(before, after);
        return;
    }
    frame()->inputMethodController().extendSelectionAndDelete(before, after);
}

void WebLocalFrameImpl::setCaretVisible(bool visible)
{
    frame()->selection().setCaretVisible(visible);
}

VisiblePosition WebLocalFrameImpl::visiblePositionForViewportPoint(const WebPoint& pointInViewport)
{
    return visiblePositionForContentsPoint(frame()->view()->viewportToContents(pointInViewport), frame());
}

WebPlugin* WebLocalFrameImpl::focusedPluginIfInputMethodSupported()
{
    WebPluginContainerImpl* container = WebLocalFrameImpl::currentPluginContainer(frame());
    if (container && container->supportsInputMethod())
        return container->plugin();
    return 0;
}

int WebLocalFrameImpl::printBegin(const WebPrintParams& printParams, const WebNode& constrainToNode)
{
    DCHECK(!frame()->document()->isFrameSet());
    WebPluginContainerImpl* pluginContainer = nullptr;
    if (constrainToNode.isNull()) {
        // If this is a plugin document, check if the plugin supports its own
        // printing. If it does, we will delegate all printing to that.
        pluginContainer = pluginContainerFromFrame(frame());
    } else {
        // We only support printing plugin nodes for now.
        pluginContainer = toWebPluginContainerImpl(constrainToNode.pluginContainer());
    }

    if (pluginContainer && pluginContainer->supportsPaginatedPrint())
        m_printContext = new ChromePluginPrintContext(frame(), pluginContainer, printParams);
    else
        m_printContext = new ChromePrintContext(frame());

    FloatRect rect(0, 0, static_cast<float>(printParams.printContentArea.width), static_cast<float>(printParams.printContentArea.height));
    m_printContext->begin(rect.width(), rect.height());
    float pageHeight;
    // We ignore the overlays calculation for now since they are generated in the
    // browser. pageHeight is actually an output parameter.
    m_printContext->computePageRects(rect, 0, 0, 1.0, pageHeight);

    return static_cast<int>(m_printContext->pageCount());
}

float WebLocalFrameImpl::getPrintPageShrink(int page)
{
    DCHECK(m_printContext);
    DCHECK_GE(page, 0);
    return m_printContext->getPageShrink(page);
}

float WebLocalFrameImpl::printPage(int page, WebCanvas* canvas)
{
#if ENABLE(PRINTING)
    DCHECK(m_printContext);
    DCHECK_GE(page, 0);
    DCHECK(frame());
    DCHECK(frame()->document());

    return m_printContext->spoolSinglePage(canvas, page);
#else
    return 0;
#endif
}

void WebLocalFrameImpl::printEnd()
{
    DCHECK(m_printContext);
    m_printContext->end();
    m_printContext.clear();
}

bool WebLocalFrameImpl::isPrintScalingDisabledForPlugin(const WebNode& node)
{
    WebPluginContainerImpl* pluginContainer =  node.isNull() ? pluginContainerFromFrame(frame()) : toWebPluginContainerImpl(node.pluginContainer());

    if (!pluginContainer || !pluginContainer->supportsPaginatedPrint())
        return false;

    return pluginContainer->isPrintScalingDisabled();
}

bool WebLocalFrameImpl::getPrintPresetOptionsForPlugin(const WebNode& node, WebPrintPresetOptions* presetOptions)
{
    WebPluginContainerImpl* pluginContainer = node.isNull() ? pluginContainerFromFrame(frame()) : toWebPluginContainerImpl(node.pluginContainer());

    if (!pluginContainer || !pluginContainer->supportsPaginatedPrint())
        return false;

    return pluginContainer->getPrintPresetOptionsFromDocument(presetOptions);
}

bool WebLocalFrameImpl::hasCustomPageSizeStyle(int pageIndex)
{
    return frame()->document()->styleForPage(pageIndex)->getPageSizeType() != PAGE_SIZE_AUTO;
}

bool WebLocalFrameImpl::isPageBoxVisible(int pageIndex)
{
    return frame()->document()->isPageBoxVisible(pageIndex);
}

void WebLocalFrameImpl::pageSizeAndMarginsInPixels(int pageIndex, WebSize& pageSize, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft)
{
    IntSize size = pageSize;
    frame()->document()->pageSizeAndMarginsInPixels(pageIndex, size, marginTop, marginRight, marginBottom, marginLeft);
    pageSize = size;
}

WebString WebLocalFrameImpl::pageProperty(const WebString& propertyName, int pageIndex)
{
    DCHECK(m_printContext);
    return m_printContext->pageProperty(frame(), propertyName.utf8().data(), pageIndex);
}

void WebLocalFrameImpl::printPagesWithBoundaries(WebCanvas* canvas, const WebSize& pageSizeInPixels)
{
    DCHECK(m_printContext);

    m_printContext->spoolAllPagesWithBoundaries(canvas, FloatSize(pageSizeInPixels.width, pageSizeInPixels.height));
}

WebRect WebLocalFrameImpl::selectionBoundsRect() const
{
    return hasSelection() ? WebRect(IntRect(frame()->selection().bounds())) : WebRect();
}

bool WebLocalFrameImpl::selectionStartHasSpellingMarkerFor(int from, int length) const
{
    if (!frame())
        return false;
    return frame()->spellChecker().selectionStartHasSpellingMarkerFor(from, length);
}

WebString WebLocalFrameImpl::layerTreeAsText(bool showDebugInfo) const
{
    if (!frame())
        return WebString();

    return WebString(frame()->layerTreeAsText(showDebugInfo ? LayerTreeIncludesDebugInfo : LayerTreeNormal));
}

// WebLocalFrameImpl public ---------------------------------------------------------

WebLocalFrame* WebLocalFrame::create(WebTreeScopeType scope, WebFrameClient* client, WebFrame* opener)
{
    return WebLocalFrameImpl::create(scope, client, opener);
}

WebLocalFrame* WebLocalFrame::createProvisional(WebFrameClient* client, WebRemoteFrame* oldWebFrame, WebSandboxFlags flags)
{
    return WebLocalFrameImpl::createProvisional(client, oldWebFrame, flags);
}

WebLocalFrameImpl* WebLocalFrameImpl::create(WebTreeScopeType scope, WebFrameClient* client, WebFrame* opener)
{
    WebLocalFrameImpl* frame = new WebLocalFrameImpl(scope, client);
    frame->setOpener(opener);
    return frame;
}

WebLocalFrameImpl* WebLocalFrameImpl::createProvisional(WebFrameClient* client, WebRemoteFrame* oldWebFrame, WebSandboxFlags flags)
{
    WebLocalFrameImpl* webFrame = new WebLocalFrameImpl(oldWebFrame, client);
    Frame* oldFrame = oldWebFrame->toImplBase()->frame();
    webFrame->setParent(oldWebFrame->parent());
    webFrame->setOpener(oldWebFrame->opener());
    // Note: this *always* temporarily sets a frame owner, even for main frames!
    // When a core Frame is created with no owner, it attempts to set itself as
    // the main frame of the Page. However, this is a provisional frame, and may
    // disappear, so Page::m_mainFrame can't be updated just yet.
    FrameOwner* tempOwner = DummyFrameOwner::create();
    // TODO(dcheng): This block is very similar to initializeCoreFrame. Try to reuse it here.
    LocalFrame* frame = LocalFrame::create(webFrame->m_frameLoaderClientImpl.get(), oldFrame->host(), tempOwner, client ? client->serviceRegistry() : nullptr);
    // Set the name and unique name directly, bypassing any of the normal logic
    // to calculate unique name.
    frame->tree().setPrecalculatedName(toWebRemoteFrameImpl(oldWebFrame)->frame()->tree().name(), toWebRemoteFrameImpl(oldWebFrame)->frame()->tree().uniqueName());
    webFrame->setCoreFrame(frame);

    frame->setOwner(oldFrame->owner());

    if (frame->owner() && frame->owner()->isRemote())
        toRemoteFrameOwner(frame->owner())->setSandboxFlags(static_cast<SandboxFlags>(flags));

    // We must call init() after m_frame is assigned because it is referenced
    // during init(). Note that this may dispatch JS events; the frame may be
    // detached after init() returns.
    frame->init();
    return webFrame;
}

WebLocalFrameImpl::WebLocalFrameImpl(WebTreeScopeType scope, WebFrameClient* client)
    : WebLocalFrame(scope)
    , m_frameLoaderClientImpl(FrameLoaderClientImpl::create(this))
    , m_frameWidget(0)
    , m_client(client)
    , m_autofillClient(0)
    , m_contentSettingsClient(0)
    , m_inputEventsScaleFactorForEmulation(1)
    , m_userMediaClientImpl(this)
    , m_webDevToolsFrontend(0)
    , m_selfKeepAlive(this)
{
    frameCount++;
}

WebLocalFrameImpl::WebLocalFrameImpl(WebRemoteFrame* oldWebFrame, WebFrameClient* client)
    : WebLocalFrameImpl(oldWebFrame->inShadowTree() ? WebTreeScopeType::Shadow : WebTreeScopeType::Document, client)
{
}

WebLocalFrameImpl::~WebLocalFrameImpl()
{
    // The widget for the frame, if any, must have already been closed.
    DCHECK(!m_frameWidget);
    frameCount--;
}

DEFINE_TRACE(WebLocalFrameImpl)
{
    visitor->trace(m_frameLoaderClientImpl);
    visitor->trace(m_frame);
    visitor->trace(m_devToolsAgent);
    visitor->trace(m_textFinder);
    visitor->trace(m_printContext);
    visitor->trace(m_contextMenuNode);
    visitor->template registerWeakMembers<WebFrame, &WebFrame::clearWeakFrames>(this);
    WebFrame::traceFrames(visitor, this);
    WebFrameImplBase::trace(visitor);
}

void WebLocalFrameImpl::setCoreFrame(LocalFrame* frame)
{
    m_frame = frame;

    // FIXME: we shouldn't add overhead to every frame by registering these objects when they're not used.
    if (!m_frame)
        return;

    if (m_client)
        providePushControllerTo(*m_frame, m_client->pushClient());

    provideNotificationPermissionClientTo(*m_frame, NotificationPermissionClientImpl::create());
    provideUserMediaTo(*m_frame, &m_userMediaClientImpl);
    provideMIDITo(*m_frame, MIDIClientProxy::create(m_client ? m_client->webMIDIClient() : nullptr));
    provideIndexedDBClientTo(*m_frame, IndexedDBClientImpl::create());
    provideLocalFileSystemTo(*m_frame, LocalFileSystemClient::create());
    provideNavigatorContentUtilsTo(*m_frame, NavigatorContentUtilsClientImpl::create(this));

    bool enableWebBluetooth = RuntimeEnabledFeatures::webBluetoothEnabled();
#if OS(CHROMEOS) || OS(ANDROID) || OS(MACOSX)
    enableWebBluetooth = true;
#endif
    if (enableWebBluetooth)
        BluetoothSupplement::provideTo(*m_frame, m_client ? m_client->bluetooth() : nullptr);

    if (RuntimeEnabledFeatures::screenOrientationEnabled())
        ScreenOrientationController::provideTo(*m_frame, m_client ? m_client->webScreenOrientationClient() : nullptr);
    if (RuntimeEnabledFeatures::presentationEnabled())
        PresentationController::provideTo(*m_frame, m_client ? m_client->presentationClient() : nullptr);
    if (RuntimeEnabledFeatures::permissionsEnabled())
        PermissionController::provideTo(*m_frame, m_client ? m_client->permissionClient() : nullptr);
    if (RuntimeEnabledFeatures::webVREnabled())
        VRController::provideTo(*m_frame, m_client ? m_client->serviceRegistry() : nullptr);
    if (RuntimeEnabledFeatures::wakeLockEnabled())
        ScreenWakeLock::provideTo(*m_frame, m_client ? m_client->serviceRegistry(): nullptr);
    if (RuntimeEnabledFeatures::audioOutputDevicesEnabled())
        provideAudioOutputDeviceClientTo(*m_frame, AudioOutputDeviceClientImpl::create());
    if (RuntimeEnabledFeatures::installedAppEnabled())
        InstalledAppController::provideTo(*m_frame, m_client ? m_client->installedAppClient() : nullptr);
}

void WebLocalFrameImpl::initializeCoreFrame(FrameHost* host, FrameOwner* owner, const AtomicString& name, const AtomicString& uniqueName)
{
    setCoreFrame(LocalFrame::create(m_frameLoaderClientImpl.get(), host, owner, client() ? client()->serviceRegistry() : nullptr));
    frame()->tree().setPrecalculatedName(name, uniqueName);
    // We must call init() after m_frame is assigned because it is referenced
    // during init(). Note that this may dispatch JS events; the frame may be
    // detached after init() returns.
    frame()->init();
    if (frame() && frame()->loader().stateMachine()->isDisplayingInitialEmptyDocument() && !parent() && !opener() && frame()->settings()->shouldReuseGlobalForUnownedMainFrame())
        frame()->document()->getSecurityOrigin()->grantUniversalAccess();
}

LocalFrame* WebLocalFrameImpl::createChildFrame(const FrameLoadRequest& request,
    const AtomicString& name, HTMLFrameOwnerElement* ownerElement)
{
    DCHECK(m_client);
    TRACE_EVENT0("blink", "WebLocalFrameImpl::createChildframe");
    WebTreeScopeType scope = frame()->document() == ownerElement->treeScope()
        ? WebTreeScopeType::Document
        : WebTreeScopeType::Shadow;
    WebFrameOwnerProperties ownerProperties(ownerElement->scrollingMode(), ownerElement->marginWidth(), ownerElement->marginHeight(), ownerElement->allowFullscreen(), ownerElement->delegatedPermissions());
    // FIXME: Using subResourceAttributeName as fallback is not a perfect
    // solution. subResourceAttributeName returns just one attribute name. The
    // element might not have the attribute, and there might be other attributes
    // which can identify the element.
    AtomicString uniqueName = frame()->tree().calculateUniqueNameForNewChildFrame(
        name, ownerElement->getAttribute(ownerElement->subResourceAttributeName()));
    WebLocalFrameImpl* webframeChild = toWebLocalFrameImpl(m_client->createChildFrame(this, scope, name, uniqueName, static_cast<WebSandboxFlags>(ownerElement->getSandboxFlags()), ownerProperties));
    if (!webframeChild)
        return nullptr;

    webframeChild->initializeCoreFrame(frame()->host(), ownerElement, name, uniqueName);
    // Initializing the core frame may cause the new child to be detached, since
    // it may dispatch a load event in the parent.
    if (!webframeChild->parent())
        return nullptr;

    // If we're moving in the back/forward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    HistoryItem* childItem = nullptr;
    if (isBackForwardLoadType(frame()->loader().loadType()) && !frame()->document()->loadEventFinished())
        childItem = webframeChild->client()->historyItemForNewChildFrame();

    FrameLoadRequest newRequest = request;
    FrameLoadType loadType = FrameLoadTypeStandard;
    if (childItem) {
        newRequest = FrameLoadRequest(request.originDocument(),
            FrameLoader::resourceRequestFromHistoryItem(childItem, WebCachePolicy::UseProtocolCachePolicy));
        loadType = FrameLoadTypeInitialHistoryLoad;
    }
    webframeChild->frame()->loader().load(newRequest, loadType, childItem);

    // Note a synchronous navigation (about:blank) would have already processed
    // onload, so it is possible for the child frame to have already been
    // detached by script in the page.
    if (!webframeChild->parent())
        return nullptr;
    return webframeChild->frame();
}

void WebLocalFrameImpl::didChangeContentsSize(const IntSize& size)
{
    if (m_textFinder && m_textFinder->totalMatchCount() > 0)
        m_textFinder->increaseMarkerVersion();
}

void WebLocalFrameImpl::createFrameView()
{
    TRACE_EVENT0("blink", "WebLocalFrameImpl::createFrameView");

    DCHECK(frame()); // If frame() doesn't exist, we probably didn't init properly.

    WebViewImpl* webView = viewImpl();

    // Check if we're shutting down.
    if (!webView->page())
        return;

    bool isMainFrame = !parent();
    IntSize initialSize = (isMainFrame || !frameWidget()) ? webView->mainFrameSize() : (IntSize)frameWidget()->size();
    bool isTransparent = !isMainFrame && parent()->isWebRemoteFrame() ? true : webView->isTransparent();

    frame()->createView(initialSize, webView->baseBackgroundColor(), isTransparent);
    if (webView->shouldAutoResize() && frame()->isLocalRoot())
        frame()->view()->enableAutoSizeMode(webView->minAutoSize(), webView->maxAutoSize());

    frame()->view()->setInputEventsTransformForEmulation(m_inputEventsOffsetForEmulation, m_inputEventsScaleFactorForEmulation);
    frame()->view()->setDisplayMode(webView->displayMode());
}

WebLocalFrameImpl* WebLocalFrameImpl::fromFrame(LocalFrame* frame)
{
    if (!frame)
        return nullptr;
    return fromFrame(*frame);
}

WebLocalFrameImpl* WebLocalFrameImpl::fromFrame(LocalFrame& frame)
{
    FrameLoaderClient* client = frame.loader().client();
    if (!client || !client->isFrameLoaderClientImpl())
        return nullptr;
    return toFrameLoaderClientImpl(client)->webFrame();
}

WebLocalFrameImpl* WebLocalFrameImpl::fromFrameOwnerElement(Element* element)
{
    if (!element->isFrameOwnerElement())
        return nullptr;
    return fromFrame(toLocalFrame(toHTMLFrameOwnerElement(element)->contentFrame()));
}

WebViewImpl* WebLocalFrameImpl::viewImpl() const
{
    if (!frame())
        return nullptr;
    return WebViewImpl::fromPage(frame()->page());
}

WebDataSourceImpl* WebLocalFrameImpl::dataSourceImpl() const
{
    return static_cast<WebDataSourceImpl*>(dataSource());
}

WebDataSourceImpl* WebLocalFrameImpl::provisionalDataSourceImpl() const
{
    return static_cast<WebDataSourceImpl*>(provisionalDataSource());
}

void WebLocalFrameImpl::setFindEndstateFocusAndSelection()
{
    if (!m_textFinder || !m_textFinder->activeMatchFrame())
        return;

    if (Range* activeMatch = m_textFinder->activeMatch()) {
        // If the user has set the selection since the match was found, we
        // don't focus anything.
        VisibleSelection selection(frame()->selection().selection());
        if (!selection.isNone())
            return;

        // Need to clean out style and layout state before querying Element::isFocusable().
        frame()->document()->updateStyleAndLayoutIgnorePendingStylesheets();

        // Try to find the first focusable node up the chain, which will, for
        // example, focus links if we have found text within the link.
        Node* node = activeMatch->firstNode();
        if (node && node->isInShadowTree()) {
            if (Node* host = node->shadowHost()) {
                if (isHTMLInputElement(*host) || isHTMLTextAreaElement(*host))
                    node = host;
            }
        }
        if (node) {
            for (Node& runner : NodeTraversal::inclusiveAncestorsOf(*node)) {
                if (!runner.isElementNode())
                    continue;
                Element& element = toElement(runner);
                if (element.isFocusable()) {
                    // Found a focusable parent node. Set the active match as the
                    // selection and focus to the focusable node.
                    frame()->selection().setSelection(VisibleSelection(EphemeralRange(activeMatch)));
                    frame()->document()->setFocusedElement(&element, FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone, nullptr));
                    return;
                }
            }
        }

        // Iterate over all the nodes in the range until we find a focusable node.
        // This, for example, sets focus to the first link if you search for
        // text and text that is within one or more links.
        node = activeMatch->firstNode();
        for (; node && node != activeMatch->pastLastNode(); node = NodeTraversal::next(*node)) {
            if (!node->isElementNode())
                continue;
            Element* element = toElement(node);
            if (element->isFocusable()) {
                frame()->document()->setFocusedElement(element, FocusParams(SelectionBehaviorOnFocus::None, WebFocusTypeNone, nullptr));
                return;
            }
        }

        // No node related to the active match was focusable, so set the
        // active match as the selection (so that when you end the Find session,
        // you'll have the last thing you found highlighted) and make sure that
        // we have nothing focused (otherwise you might have text selected but
        // a link focused, which is weird).
        frame()->selection().setSelection(VisibleSelection(EphemeralRange(activeMatch)));
        frame()->document()->clearFocusedElement();

        // Finally clear the active match, for two reasons:
        // We just finished the find 'session' and we don't want future (potentially
        // unrelated) find 'sessions' operations to start at the same place.
        // The WebLocalFrameImpl could get reused and the activeMatch could end up pointing
        // to a document that is no longer valid. Keeping an invalid reference around
        // is just asking for trouble.
        m_textFinder->resetActiveMatch();
    }
}

void WebLocalFrameImpl::didFail(const ResourceError& error, bool wasProvisional, HistoryCommitType commitType)
{
    if (!client())
        return;
    WebURLError webError = error;
    WebHistoryCommitType webCommitType = static_cast<WebHistoryCommitType>(commitType);

    if (WebPluginContainerImpl* plugin = pluginContainerFromFrame(frame()))
        plugin->didFailLoading(error);

    if (wasProvisional)
        client()->didFailProvisionalLoad(this, webError, webCommitType);
    else
        client()->didFailLoad(this, webError, webCommitType);
}

void WebLocalFrameImpl::didFinish()
{
    if (!client())
        return;

    if (WebPluginContainerImpl* plugin = pluginContainerFromFrame(frame()))
        plugin->didFinishLoading();

    client()->didFinishLoad(this);
}

void WebLocalFrameImpl::setCanHaveScrollbars(bool canHaveScrollbars)
{
    frame()->view()->setCanHaveScrollbars(canHaveScrollbars);
}

void WebLocalFrameImpl::setInputEventsTransformForEmulation(const IntSize& offset, float contentScaleFactor)
{
    m_inputEventsOffsetForEmulation = offset;
    m_inputEventsScaleFactorForEmulation = contentScaleFactor;
    if (frame()->view())
        frame()->view()->setInputEventsTransformForEmulation(m_inputEventsOffsetForEmulation, m_inputEventsScaleFactorForEmulation);
}

void WebLocalFrameImpl::loadJavaScriptURL(const KURL& url)
{
    // This is copied from ScriptController::executeScriptIfJavaScriptURL.
    // Unfortunately, we cannot just use that method since it is private, and
    // it also doesn't quite behave as we require it to for bookmarklets. The
    // key difference is that we need to suppress loading the string result
    // from evaluating the JS URL if executing the JS URL resulted in a
    // location change. We also allow a JS URL to be loaded even if scripts on
    // the page are otherwise disabled.

    if (!frame()->document() || !frame()->page())
        return;

    Document* ownerDocument(frame()->document());

    // Protect privileged pages against bookmarklets and other javascript manipulations.
    if (SchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(frame()->document()->url().protocol()))
        return;

    String script = decodeURLEscapeSequences(url.getString().substring(strlen("javascript:")));
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    v8::HandleScope handleScope(toIsolate(frame()));
    v8::Local<v8::Value> result = frame()->script().executeScriptInMainWorldAndReturnValue(ScriptSourceCode(script));
    if (result.IsEmpty() || !result->IsString())
        return;
    String scriptResult = toCoreString(v8::Local<v8::String>::Cast(result));
    if (!frame()->navigationScheduler().locationChangePending())
        frame()->loader().replaceDocumentWhileExecutingJavaScriptURL(scriptResult, ownerDocument);
}

HitTestResult WebLocalFrameImpl::hitTestResultForVisualViewportPos(const IntPoint& posInViewport)
{
    IntPoint rootFramePoint(frame()->host()->visualViewport().viewportToRootFrame(posInViewport));
    IntPoint docPoint(frame()->view()->rootFrameToContents(rootFramePoint));
    HitTestResult result = frame()->eventHandler().hitTestResultAtPoint(docPoint, HitTestRequest::ReadOnly | HitTestRequest::Active);
    result.setToShadowHostIfInUserAgentShadowRoot();
    return result;
}

static void ensureFrameLoaderHasCommitted(FrameLoader& frameLoader)
{
    // Internally, Blink uses CommittedMultipleRealLoads to track whether the
    // next commit should create a new history item or not. Ensure we have
    // reached that state.
    if (frameLoader.stateMachine()->committedMultipleRealLoads())
        return;
    frameLoader.stateMachine()->advanceTo(FrameLoaderStateMachine::CommittedMultipleRealLoads);
}

void WebLocalFrameImpl::setAutofillClient(WebAutofillClient* autofillClient)
{
    m_autofillClient = autofillClient;
}

WebAutofillClient* WebLocalFrameImpl::autofillClient()
{
    return m_autofillClient;
}

void WebLocalFrameImpl::setDevToolsAgentClient(WebDevToolsAgentClient* devToolsClient)
{
    DCHECK(devToolsClient);
    m_devToolsAgent = WebDevToolsAgentImpl::create(this, devToolsClient);
}

WebDevToolsAgent* WebLocalFrameImpl::devToolsAgent()
{
    return m_devToolsAgent.get();
}

WebLocalFrameImpl* WebLocalFrameImpl::localRoot()
{
    // This can't use the LocalFrame::localFrameRoot, since it may be called
    // when the WebLocalFrame exists but the core LocalFrame does not.
    // TODO(alexmos, dcheng): Clean this up to only calculate this in one place.
    WebLocalFrameImpl* localRoot = this;
    while (localRoot->parent() && localRoot->parent()->isWebLocalFrame())
        localRoot = toWebLocalFrameImpl(localRoot->parent());
    return localRoot;
}

WebLocalFrame* WebLocalFrameImpl::traversePreviousLocal(bool wrap) const
{
    WebFrame* previousLocalFrame = this->traversePrevious(wrap);
    while (previousLocalFrame && !previousLocalFrame->isWebLocalFrame())
        previousLocalFrame = previousLocalFrame->traversePrevious(wrap);
    return previousLocalFrame ? previousLocalFrame->toWebLocalFrame() : nullptr;
}

WebLocalFrame* WebLocalFrameImpl::traverseNextLocal(bool wrap) const
{
    WebFrame* nextLocalFrame = this->traverseNext(wrap);
    while (nextLocalFrame && !nextLocalFrame->isWebLocalFrame())
        nextLocalFrame = nextLocalFrame->traverseNext(wrap);
    return nextLocalFrame ? nextLocalFrame->toWebLocalFrame() : nullptr;
}

void WebLocalFrameImpl::sendPings(const WebURL& destinationURL)
{
    DCHECK(frame());
    DCHECK(m_contextMenuNode.get());
    Element* anchor = m_contextMenuNode->enclosingLinkEventParentOrSelf();
    if (isHTMLAnchorElement(anchor))
        toHTMLAnchorElement(anchor)->sendPings(destinationURL);
}

bool WebLocalFrameImpl::dispatchBeforeUnloadEvent(bool isReload)
{
    if (!frame())
        return true;

    return frame()->loader().shouldClose(isReload);
}

WebURLRequest WebLocalFrameImpl::requestFromHistoryItem(const WebHistoryItem& item, WebCachePolicy cachePolicy) const
{
    HistoryItem* historyItem = item;
    ResourceRequest request = FrameLoader::resourceRequestFromHistoryItem(
        historyItem, cachePolicy);
    return WrappedResourceRequest(request);
}

WebURLRequest WebLocalFrameImpl::requestForReload(WebFrameLoadType loadType,
    const WebURL& overrideUrl) const
{
    DCHECK(frame());
    ResourceRequest request = frame()->loader().resourceRequestForReload(
        static_cast<FrameLoadType>(loadType), overrideUrl);
    return WrappedResourceRequest(request);
}

void WebLocalFrameImpl::load(const WebURLRequest& request, WebFrameLoadType webFrameLoadType,
    const WebHistoryItem& item, WebHistoryLoadType webHistoryLoadType, bool isClientRedirect)
{
    DCHECK(frame());
    DCHECK(!request.isNull());
    const ResourceRequest& resourceRequest = request.toResourceRequest();

    if (resourceRequest.url().protocolIs("javascript")
        && webFrameLoadType == WebFrameLoadType::Standard) {
        loadJavaScriptURL(resourceRequest.url());
        return;
    }

    FrameLoadRequest frameRequest = FrameLoadRequest(nullptr, resourceRequest);
    if (isClientRedirect)
        frameRequest.setClientRedirect(ClientRedirectPolicy::ClientRedirect);
    HistoryItem* historyItem = item;
    frame()->loader().load(
        frameRequest, static_cast<FrameLoadType>(webFrameLoadType), historyItem,
        static_cast<HistoryLoadType>(webHistoryLoadType));
}

void WebLocalFrameImpl::loadData(const WebData& data, const WebString& mimeType,
    const WebString& textEncoding, const WebURL& baseURL, const WebURL& unreachableURL, bool replace,
    WebFrameLoadType webFrameLoadType, const WebHistoryItem& item,
    WebHistoryLoadType webHistoryLoadType, bool isClientRedirect)
{
    DCHECK(frame());

    // If we are loading substitute data to replace an existing load, then
    // inherit all of the properties of that original request. This way,
    // reload will re-attempt the original request. It is essential that
    // we only do this when there is an unreachableURL since a non-empty
    // unreachableURL informs FrameLoader::reload to load unreachableURL
    // instead of the currently loaded URL.
    ResourceRequest request;
    if (replace && !unreachableURL.isEmpty() && frame()->loader().provisionalDocumentLoader())
        request = frame()->loader().provisionalDocumentLoader()->originalRequest();
    request.setURL(baseURL);
    request.setCheckForBrowserSideNavigation(false);

    FrameLoadRequest frameRequest(0, request, SubstituteData(data, mimeType, textEncoding, unreachableURL));
    DCHECK(frameRequest.substituteData().isValid());
    frameRequest.setReplacesCurrentItem(replace);
    if (isClientRedirect)
        frameRequest.setClientRedirect(ClientRedirectPolicy::ClientRedirect);

    HistoryItem* historyItem = item;
    frame()->loader().load(
        frameRequest, static_cast<FrameLoadType>(webFrameLoadType), historyItem,
        static_cast<HistoryLoadType>(webHistoryLoadType));
}

bool WebLocalFrameImpl::isLoading() const
{
    if (!frame() || !frame()->document())
        return false;
    return frame()->loader().stateMachine()->isDisplayingInitialEmptyDocument() || frame()->loader().provisionalDocumentLoader() || !frame()->document()->loadEventFinished();
}

bool WebLocalFrameImpl::isNavigationScheduledWithin(double intervalInSeconds) const
{
    return frame() && frame()->navigationScheduler().isNavigationScheduledWithin(intervalInSeconds);
}

void WebLocalFrameImpl::setCommittedFirstRealLoad()
{
    DCHECK(frame());
    ensureFrameLoaderHasCommitted(frame()->loader());
}

void WebLocalFrameImpl::sendOrientationChangeEvent()
{
    if (!frame())
        return;

    // Screen Orientation API
    if (ScreenOrientationController::from(*frame()))
        ScreenOrientationController::from(*frame())->notifyOrientationChanged();

    // Legacy window.orientation API
    if (RuntimeEnabledFeatures::orientationEventEnabled() && frame()->domWindow())
        frame()->localDOMWindow()->sendOrientationChangeEvent();
}

void WebLocalFrameImpl::willShowInstallBannerPrompt(int requestId, const WebVector<WebString>& platforms, WebAppBannerPromptReply* reply)
{
    if (!RuntimeEnabledFeatures::appBannerEnabled() || !frame())
        return;

    AppBannerController::willShowInstallBannerPrompt(requestId, client()->appBannerClient(), frame(), platforms, reply);
}

void WebLocalFrameImpl::requestRunTask(WebSuspendableTask* task) const
{
    DCHECK(frame());
    DCHECK(frame()->document());
    frame()->document()->postSuspendableTask(WebSuspendableTaskWrapper::create(wrapUnique(task)));
}

void WebLocalFrameImpl::didCallAddSearchProvider()
{
    UseCounter::count(frame(), UseCounter::ExternalAddSearchProvider);
}

void WebLocalFrameImpl::didCallIsSearchProviderInstalled()
{
    UseCounter::count(frame(), UseCounter::ExternalIsSearchProviderInstalled);
}

bool WebLocalFrameImpl::find(int identifier, const WebString& searchText, const WebFindOptions& options, bool wrapWithinFrame, WebRect* selectionRect, bool* activeNow)
{
    // Search for an active match only if this frame is focused or if this is a
    // find next request.
    if (isFocused() || options.findNext)
        return ensureTextFinder().find(identifier, searchText, options, wrapWithinFrame, selectionRect, activeNow);

    return false;
}

void WebLocalFrameImpl::stopFinding(StopFindAction action)
{
    bool clearSelection = action == StopFindActionClearSelection;
    if (clearSelection)
        executeCommand(WebString::fromUTF8("Unselect"));

    if (m_textFinder) {
        if (!clearSelection)
            setFindEndstateFocusAndSelection();
        m_textFinder->stopFindingAndClearSelection();
    }

    if (action == StopFindActionActivateSelection && isFocused()) {
        WebDocument doc = document();
        if (!doc.isNull()) {
            WebElement element = doc.focusedElement();
            if (!element.isNull())
                element.simulateClick();
        }
    }
}

void WebLocalFrameImpl::scopeStringMatches(int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
{
    ensureTextFinder().scopeStringMatches(identifier, searchText, options, reset);
}

void WebLocalFrameImpl::cancelPendingScopingEffort()
{
    if (m_textFinder)
        m_textFinder->cancelPendingScopingEffort();
}

void WebLocalFrameImpl::increaseMatchCount(int count, int identifier)
{
    ensureTextFinder().increaseMatchCount(identifier, count);
}

void WebLocalFrameImpl::resetMatchCount()
{
    ensureTextFinder().resetMatchCount();
}

void WebLocalFrameImpl::dispatchMessageEventWithOriginCheck(const WebSecurityOrigin& intendedTargetOrigin, const WebDOMEvent& event)
{
    DCHECK(!event.isNull());
    frame()->localDOMWindow()->dispatchMessageEventWithOriginCheck(intendedTargetOrigin.get(), event, SourceLocation::create(String(), 0, 0, nullptr));
}

int WebLocalFrameImpl::findMatchMarkersVersion() const
{
    if (m_textFinder)
        return m_textFinder->findMatchMarkersVersion();
    return 0;
}

int WebLocalFrameImpl::selectNearestFindMatch(const WebFloatPoint& point, WebRect* selectionRect)
{
    return ensureTextFinder().selectNearestFindMatch(point, selectionRect);
}

float WebLocalFrameImpl::distanceToNearestFindMatch(const WebFloatPoint& point)
{
    float nearestDistance;
    ensureTextFinder().nearestFindMatch(point, &nearestDistance);
    return nearestDistance;
}

WebFloatRect WebLocalFrameImpl::activeFindMatchRect()
{
    if (m_textFinder)
        return m_textFinder->activeFindMatchRect();
    return WebFloatRect();
}

void WebLocalFrameImpl::findMatchRects(WebVector<WebFloatRect>& outputRects)
{
    ensureTextFinder().findMatchRects(outputRects);
}

void WebLocalFrameImpl::setTickmarks(const WebVector<WebRect>& tickmarks)
{
    if (frameView()) {
        Vector<IntRect> tickmarksConverted(tickmarks.size());
        for (size_t i = 0; i < tickmarks.size(); ++i)
            tickmarksConverted[i] = tickmarks[i];
        frameView()->setTickmarks(tickmarksConverted);
    }
}

void WebLocalFrameImpl::willBeDetached()
{
    if (m_devToolsAgent)
        m_devToolsAgent->willBeDestroyed();
}

void WebLocalFrameImpl::willDetachParent()
{
    // Do not expect string scoping results from any frames that got detached
    // in the middle of the operation.
    if (m_textFinder && m_textFinder->scopingInProgress()) {

        // There is a possibility that the frame being detached was the only
        // pending one. We need to make sure final replies can be sent.
        m_textFinder->flushCurrentScoping();

        m_textFinder->cancelPendingScopingEffort();
    }
}

TextFinder* WebLocalFrameImpl::textFinder() const
{
    return m_textFinder;
}

TextFinder& WebLocalFrameImpl::ensureTextFinder()
{
    if (!m_textFinder)
        m_textFinder = TextFinder::create(*this);

    return *m_textFinder;
}

void WebLocalFrameImpl::setFrameWidget(WebFrameWidget* frameWidget)
{
    m_frameWidget = frameWidget;
}

WebFrameWidget* WebLocalFrameImpl::frameWidget() const
{
    return m_frameWidget;
}

void WebLocalFrameImpl::copyImageAt(const WebPoint& posInViewport)
{
    HitTestResult result = hitTestResultForVisualViewportPos(posInViewport);
    if (!isHTMLCanvasElement(result.innerNodeOrImageMapImage()) && result.absoluteImageURL().isEmpty()) {
        // There isn't actually an image at these coordinates.  Might be because
        // the window scrolled while the context menu was open or because the page
        // changed itself between when we thought there was an image here and when
        // we actually tried to retreive the image.
        //
        // FIXME: implement a cache of the most recent HitTestResult to avoid having
        //        to do two hit tests.
        return;
    }

    frame()->editor().copyImage(result);
}

void WebLocalFrameImpl::saveImageAt(const WebPoint& posInViewport)
{
    Node* node = hitTestResultForVisualViewportPos(posInViewport).innerNodeOrImageMapImage();
    if (!node || !(isHTMLCanvasElement(*node) || isHTMLImageElement(*node)))
        return;

    String url = toElement(*node).imageSourceURL();
    if (!KURL(KURL(), url).protocolIsData())
        return;

    m_client->saveImageFromDataURL(url);
}

WebSandboxFlags WebLocalFrameImpl::effectiveSandboxFlags() const
{
    if (!frame())
        return WebSandboxFlags::None;
    return static_cast<WebSandboxFlags>(frame()->loader().effectiveSandboxFlags());
}

void WebLocalFrameImpl::forceSandboxFlags(WebSandboxFlags flags)
{
    frame()->loader().forceSandboxFlags(static_cast<SandboxFlags>(flags));
}

void WebLocalFrameImpl::clearActiveFindMatch()
{
    ensureTextFinder().clearActiveFindMatch();
}

} // namespace blink
