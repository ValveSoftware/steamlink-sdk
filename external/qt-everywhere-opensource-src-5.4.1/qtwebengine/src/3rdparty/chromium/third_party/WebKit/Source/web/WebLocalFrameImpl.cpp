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
// corresponding LocalFrame in WebCore.
//
// How frames are destroyed
// ------------------------
//
// The main frame is never destroyed and is re-used. The FrameLoader is re-used
// and a reference to the main frame is kept by the Page.
//
// When frame content is replaced, all subframes are destroyed. This happens
// in FrameLoader::detachFromParent for each subframe in a pre-order depth-first
// traversal. Note that child node order may not match DOM node order!
// detachFromParent() calls FrameLoaderClient::detachedFromParent(), which calls
// WebFrame::frameDetached(). This triggers WebFrame to clear its reference to
// LocalFrame, and also notifies the embedder via WebFrameClient that the frame is
// detached. Most embedders will invoke close() on the WebFrame at this point,
// triggering its deletion unless something else is still retaining a reference.
//
// Thie client is expected to be set whenever the WebLocalFrameImpl is attached to
// the DOM.

#include "config.h"
#include "web/WebLocalFrameImpl.h"

#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8GCController.h"
#include "bindings/v8/V8PerIsolateData.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentMarker.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/IconURL.h"
#include "core/dom/MessagePort.h"
#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/PlainTextRange.h"
#include "core/editing/SpellChecker.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/TextIterator.h"
#include "core/editing/htmlediting.h"
#include "core/editing/markup.h"
#include "core/frame/Console.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/PluginDocument.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/HistoryItem.h"
#include "core/loader/SubstituteData.h"
#include "core/page/Chrome.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/Page.h"
#include "core/page/PrintContext.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderFrame.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderTreeAsText.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/StyleInheritedData.h"
#include "core/timing/Performance.h"
#include "modules/geolocation/GeolocationController.h"
#include "modules/notifications/NotificationController.h"
#include "modules/screen_orientation/ScreenOrientationController.h"
#include "platform/TraceEvent.h"
#include "platform/UserGestureIndicator.h"
#include "platform/clipboard/ClipboardUtilities.h"
#include "platform/fonts/FontCache.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayerClient.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/heap/Handle.h"
#include "platform/network/ResourceRequest.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebVector.h"
#include "public/web/WebConsoleMessage.h"
#include "public/web/WebDOMEvent.h"
#include "public/web/WebDOMEventListener.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFindOptions.h"
#include "public/web/WebFormElement.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebIconURL.h"
#include "public/web/WebInputElement.h"
#include "public/web/WebNode.h"
#include "public/web/WebPerformance.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebPrintParams.h"
#include "public/web/WebRange.h"
#include "public/web/WebScriptSource.h"
#include "public/web/WebSecurityOrigin.h"
#include "public/web/WebSerializedScriptValue.h"
#include "web/AssociatedURLLoader.h"
#include "web/CompositionUnderlineVectorBuilder.h"
#include "web/EventListenerWrapper.h"
#include "web/FindInPageCoordinates.h"
#include "web/GeolocationClientProxy.h"
#include "web/LocalFileSystemClient.h"
#include "web/MIDIClientProxy.h"
#include "web/PageOverlay.h"
#include "web/SharedWorkerRepositoryClientImpl.h"
#include "web/TextFinder.h"
#include "web/WebDataSourceImpl.h"
#include "web/WebDevToolsAgentPrivate.h"
#include "web/WebPluginContainerImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/CurrentTime.h"
#include "wtf/HashMap.h"
#include <algorithm>

using namespace WebCore;

namespace blink {

static int frameCount = 0;

// Key for a StatsCounter tracking how many WebFrames are active.
static const char webFrameActiveCount[] = "WebFrameActiveCount";

static void frameContentAsPlainText(size_t maxChars, LocalFrame* frame, StringBuilder& output)
{
    Document* document = frame->document();
    if (!document)
        return;

    if (!frame->view())
        return;

    // TextIterator iterates over the visual representation of the DOM. As such,
    // it requires you to do a layout before using it (otherwise it'll crash).
    document->updateLayout();

    // Select the document body.
    RefPtrWillBeRawPtr<Range> range(document->createRange());
    TrackExceptionState exceptionState;
    range->selectNodeContents(document->body(), exceptionState);

    if (!exceptionState.hadException()) {
        // The text iterator will walk nodes giving us text. This is similar to
        // the plainText() function in core/editing/TextIterator.h, but we implement the maximum
        // size and also copy the results directly into a wstring, avoiding the
        // string conversion.
        for (TextIterator it(range.get()); !it.atEnd(); it.advance()) {
            it.appendTextToStringBuilder(output, 0, maxChars - output.length());
            if (output.length() >= maxChars)
                return; // Filled up the buffer.
        }
    }

    // The separator between frames when the frames are converted to plain text.
    const LChar frameSeparator[] = { '\n', '\n' };
    const size_t frameSeparatorLength = WTF_ARRAY_LENGTH(frameSeparator);

    // Recursively walk the children.
    const FrameTree& frameTree = frame->tree();
    for (Frame* curChild = frameTree.firstChild(); curChild; curChild = curChild->tree().nextSibling()) {
        if (!curChild->isLocalFrame())
            continue;
        LocalFrame* curLocalChild = toLocalFrame(curChild);
        // Ignore the text of non-visible frames.
        RenderView* contentRenderer = curLocalChild->contentRenderer();
        RenderPart* ownerRenderer = curLocalChild->ownerRenderer();
        if (!contentRenderer || !contentRenderer->width() || !contentRenderer->height()
            || (contentRenderer->x() + contentRenderer->width() <= 0) || (contentRenderer->y() + contentRenderer->height() <= 0)
            || (ownerRenderer && ownerRenderer->style() && ownerRenderer->style()->visibility() != VISIBLE)) {
            continue;
        }

        // Make sure the frame separator won't fill up the buffer, and give up if
        // it will. The danger is if the separator will make the buffer longer than
        // maxChars. This will cause the computation above:
        //   maxChars - output->size()
        // to be a negative number which will crash when the subframe is added.
        if (output.length() >= maxChars - frameSeparatorLength)
            return;

        output.append(frameSeparator, frameSeparatorLength);
        frameContentAsPlainText(maxChars, curLocalChild, output);
        if (output.length() >= maxChars)
            return; // Filled up the buffer.
    }
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

WebPluginContainerImpl* WebLocalFrameImpl::pluginContainerFromNode(WebCore::LocalFrame* frame, const WebNode& node)
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame);
    if (pluginContainer)
        return pluginContainer;
    return toWebPluginContainerImpl(node.pluginContainer());
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

    virtual ~ChromePrintContext() { }

    virtual void begin(float width, float height)
    {
        ASSERT(!m_printedPageWidth);
        m_printedPageWidth = width;
        PrintContext::begin(m_printedPageWidth, height);
    }

    virtual void end()
    {
        PrintContext::end();
    }

    virtual float getPageShrink(int pageNumber) const
    {
        IntRect pageRect = m_pageRects[pageNumber];
        return m_printedPageWidth / pageRect.width();
    }

    // Spools the printed page, a subrect of frame(). Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead. Returns the scale to be applied.
    // On Linux, we don't have the problem with NativeTheme, hence we let WebKit
    // do the scaling and ignore the return value.
    virtual float spoolPage(GraphicsContext& context, int pageNumber)
    {
        IntRect pageRect = m_pageRects[pageNumber];
        float scale = m_printedPageWidth / pageRect.width();

        context.save();
#if OS(POSIX) && !OS(MACOSX)
        context.scale(scale, scale);
#endif
        context.translate(static_cast<float>(-pageRect.x()), static_cast<float>(-pageRect.y()));
        context.clip(pageRect);
        frame()->view()->paintContents(&context, pageRect);
        if (context.supportsURLFragments())
            outputLinkedDestinations(context, frame()->document(), pageRect);
        context.restore();
        return scale;
    }

    void spoolAllPagesWithBoundaries(GraphicsContext& graphicsContext, const FloatSize& pageSizeInPixels)
    {
        if (!frame()->document() || !frame()->view() || !frame()->document()->renderView())
            return;

        frame()->document()->updateLayout();

        float pageHeight;
        computePageRects(FloatRect(FloatPoint(0, 0), pageSizeInPixels), 0, 0, 1, pageHeight);

        const float pageWidth = pageSizeInPixels.width();
        size_t numPages = pageRects().size();
        int totalHeight = numPages * (pageSizeInPixels.height() + 1) - 1;

        // Fill the whole background by white.
        graphicsContext.setFillColor(Color::white);
        graphicsContext.fillRect(FloatRect(0, 0, pageWidth, totalHeight));

        int currentHeight = 0;
        for (size_t pageIndex = 0; pageIndex < numPages; pageIndex++) {
            // Draw a line for a page boundary if this isn't the first page.
            if (pageIndex > 0) {
                graphicsContext.save();
                graphicsContext.setStrokeColor(Color(0, 0, 255));
                graphicsContext.setFillColor(Color(0, 0, 255));
                graphicsContext.drawLine(IntPoint(0, currentHeight), IntPoint(pageWidth, currentHeight));
                graphicsContext.restore();
            }

            graphicsContext.save();

            graphicsContext.translate(0, currentHeight);
#if OS(WIN) || OS(MACOSX)
            // Account for the disabling of scaling in spoolPage. In the context
            // of spoolAllPagesWithBoundaries the scale HAS NOT been pre-applied.
            float scale = getPageShrink(pageIndex);
            graphicsContext.scale(scale, scale);
#endif
            spoolPage(graphicsContext, pageIndex);
            graphicsContext.restore();

            currentHeight += pageSizeInPixels.height() + 1;
        }
    }

    virtual void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
    {
        PrintContext::computePageRects(printRect, headerHeight, footerHeight, userScaleFactor, outPageHeight);
    }

    virtual int pageCount() const
    {
        return PrintContext::pageCount();
    }

private:
    // Set when printing.
    float m_printedPageWidth;
};

// Simple class to override some of PrintContext behavior. This is used when
// the frame hosts a plugin that supports custom printing. In this case, we
// want to delegate all printing related calls to the plugin.
class ChromePluginPrintContext : public ChromePrintContext {
public:
    ChromePluginPrintContext(LocalFrame* frame, WebPluginContainerImpl* plugin, const WebPrintParams& printParams)
        : ChromePrintContext(frame), m_plugin(plugin), m_pageCount(0), m_printParams(printParams)
    {
    }

    virtual ~ChromePluginPrintContext() { }

    virtual void begin(float width, float height)
    {
    }

    virtual void end()
    {
        m_plugin->printEnd();
    }

    virtual float getPageShrink(int pageNumber) const
    {
        // We don't shrink the page (maybe we should ask the widget ??)
        return 1.0;
    }

    virtual void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
    {
        m_printParams.printContentArea = IntRect(printRect);
        m_pageCount = m_plugin->printBegin(m_printParams);
    }

    virtual int pageCount() const
    {
        return m_pageCount;
    }

    // Spools the printed page, a subrect of frame(). Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead. Returns the scale to be applied.
    virtual float spoolPage(GraphicsContext& context, int pageNumber)
    {
        m_plugin->printPage(pageNumber, &context);
        return 1.0;
    }

private:
    // Set when printing.
    WebPluginContainerImpl* m_plugin;
    int m_pageCount;
    WebPrintParams m_printParams;

};

static WebDataSource* DataSourceForDocLoader(DocumentLoader* loader)
{
    return loader ? WebDataSourceImpl::fromDocumentLoader(loader) : 0;
}

// WebFrame -------------------------------------------------------------------

int WebFrame::instanceCount()
{
    return frameCount;
}

WebLocalFrame* WebLocalFrame::frameForCurrentContext()
{
    v8::Handle<v8::Context> context = v8::Isolate::GetCurrent()->GetCurrentContext();
    if (context.IsEmpty())
        return 0;
    return frameForContext(context);
}

WebLocalFrame* WebLocalFrame::frameForContext(v8::Handle<v8::Context> context)
{
    return WebLocalFrameImpl::fromFrame(toFrameIfNotDetached(context));
}

WebLocalFrame* WebLocalFrame::fromFrameOwnerElement(const WebElement& element)
{
    return WebLocalFrameImpl::fromFrameOwnerElement(PassRefPtrWillBeRawPtr<Element>(element).get());
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
    ASSERT_NOT_REACHED();
    return 0;
}

void WebLocalFrameImpl::close()
{
    m_client = 0;

    // FIXME: Oilpan: Signal to LocalFrame and its supplements that the frame is
    // being torn down so it can do prompt clean-up. For example, this will
    // clear the raw back pointer to m_geolocationClientProxy. Once
    // GeolocationClientProxy is on-heap it looks like we can completely remove
    // |willBeDestroyed| from supplements since tracing will ensure safety.
    if (m_frame)
        m_frame->willBeDestroyed();

    deref(); // Balances ref() acquired in WebFrame::create
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
    if (frame()->loader().state() == FrameStateComplete)
        return frame()->document()->iconURLs(iconTypesMask);
    return WebVector<WebIconURL>();
}

void WebLocalFrameImpl::setIsRemote(bool isRemote)
{
    m_isRemote = isRemote;
    if (isRemote)
        client()->initializeChildFrame(frame()->view()->frameRect(), frame()->view()->visibleContentScaleFactor());
}

void WebLocalFrameImpl::setRemoteWebLayer(WebLayer* webLayer)
{
    if (!frame())
        return;

    if (frame()->remotePlatformLayer())
        GraphicsLayer::unregisterContentsLayer(frame()->remotePlatformLayer());
    if (webLayer)
        GraphicsLayer::registerContentsLayer(webLayer);
    frame()->setRemotePlatformLayer(webLayer);
    // FIXME: This should be moved to WebRemoteFrame.
    ASSERT(frame()->deprecatedLocalOwner());
    frame()->deprecatedLocalOwner()->setNeedsCompositingUpdate();
    if (RenderPart* renderer = frame()->ownerRenderer())
        renderer->layer()->updateSelfPaintingLayer();
}

void WebLocalFrameImpl::setPermissionClient(WebPermissionClient* permissionClient)
{
    m_permissionClient = permissionClient;
}

void WebLocalFrameImpl::setSharedWorkerRepositoryClient(WebSharedWorkerRepositoryClient* client)
{
    m_sharedWorkerRepositoryClient = SharedWorkerRepositoryClientImpl::create(client);
}

WebSize WebLocalFrameImpl::scrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return view->scrollOffset();
}

WebSize WebLocalFrameImpl::minimumScrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return toIntSize(view->minimumScrollPosition());
}

WebSize WebLocalFrameImpl::maximumScrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return toIntSize(view->maximumScrollPosition());
}

void WebLocalFrameImpl::setScrollOffset(const WebSize& offset)
{
    if (FrameView* view = frameView())
        view->setScrollOffset(IntPoint(offset.width, offset.height));
}

WebSize WebLocalFrameImpl::contentsSize() const
{
    return frame()->view()->contentsSize();
}

bool WebLocalFrameImpl::hasVisibleContent() const
{
    if (RenderPart* renderer = frame()->ownerRenderer()) {
        if (renderer->style()->visibility() != VISIBLE)
            return false;
    }
    return frame()->view()->visibleWidth() > 0 && frame()->view()->visibleHeight() > 0;
}

WebRect WebLocalFrameImpl::visibleContentRect() const
{
    return frame()->view()->visibleContentRect();
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

void WebLocalFrameImpl::setOpener(WebFrame* opener)
{
    // FIXME: Does this need to move up into WebFrame too?
    if (WebFrame::opener() && !opener && m_client)
        m_client->didDisownOpener(this);

    WebFrame::setOpener(opener);

    ASSERT(m_frame);
    if (m_frame && m_frame->document())
        m_frame->document()->initSecurityContext();
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
    return WebPerformance(&frame()->domWindow()->performance());
}

bool WebLocalFrameImpl::dispatchBeforeUnloadEvent()
{
    if (!frame())
        return true;
    return frame()->loader().shouldClose();
}

void WebLocalFrameImpl::dispatchUnloadEvent()
{
    if (!frame())
        return;
    frame()->loader().closeURL();
}

NPObject* WebLocalFrameImpl::windowObject() const
{
    if (!frame())
        return 0;
    return frame()->script().windowScriptNPObject();
}

void WebLocalFrameImpl::bindToWindowObject(const WebString& name, NPObject* object)
{
    bindToWindowObject(name, object, 0);
}

void WebLocalFrameImpl::bindToWindowObject(const WebString& name, NPObject* object, void*)
{
    if (!frame() || !frame()->script().canExecuteScripts(NotAboutToExecuteScript))
        return;
    frame()->script().bindToWindowObject(frame(), String(name), object);
}

void WebLocalFrameImpl::executeScript(const WebScriptSource& source)
{
    ASSERT(frame());
    TextPosition position(OrdinalNumber::fromOneBasedInt(source.startLine), OrdinalNumber::first());
    v8::HandleScope handleScope(toIsolate(frame()));
    frame()->script().executeScriptInMainWorld(ScriptSourceCode(source.code, source.url, position));
}

void WebLocalFrameImpl::executeScriptInIsolatedWorld(int worldID, const WebScriptSource* sourcesIn, unsigned numSources, int extensionGroup)
{
    ASSERT(frame());
    RELEASE_ASSERT(worldID > 0);
    RELEASE_ASSERT(worldID < EmbedderWorldIdLimit);

    Vector<ScriptSourceCode> sources;
    for (unsigned i = 0; i < numSources; ++i) {
        TextPosition position(OrdinalNumber::fromOneBasedInt(sourcesIn[i].startLine), OrdinalNumber::first());
        sources.append(ScriptSourceCode(sourcesIn[i].code, sourcesIn[i].url, position));
    }

    v8::HandleScope handleScope(toIsolate(frame()));
    frame()->script().executeScriptInIsolatedWorld(worldID, sources, extensionGroup, 0);
}

void WebLocalFrameImpl::setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin& securityOrigin)
{
    ASSERT(frame());
    DOMWrapperWorld::setIsolatedWorldSecurityOrigin(worldID, securityOrigin.get());
}

void WebLocalFrameImpl::setIsolatedWorldContentSecurityPolicy(int worldID, const WebString& policy)
{
    ASSERT(frame());
    DOMWrapperWorld::setIsolatedWorldContentSecurityPolicy(worldID, policy);
}

void WebLocalFrameImpl::addMessageToConsole(const WebConsoleMessage& message)
{
    ASSERT(frame());

    MessageLevel webCoreMessageLevel;
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
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    frame()->document()->addConsoleMessage(OtherMessageSource, webCoreMessageLevel, message.text);
}

void WebLocalFrameImpl::collectGarbage()
{
    if (!frame())
        return;
    if (!frame()->settings()->scriptEnabled())
        return;
    V8GCController::collectGarbage(v8::Isolate::GetCurrent());
}

bool WebLocalFrameImpl::checkIfRunInsecureContent(const WebURL& url) const
{
    ASSERT(frame());
    return frame()->loader().mixedContentChecker()->canRunInsecureContent(frame()->document()->securityOrigin(), url);
}

v8::Handle<v8::Value> WebLocalFrameImpl::executeScriptAndReturnValue(const WebScriptSource& source)
{
    ASSERT(frame());

    // FIXME: This fake user gesture is required to make a bunch of pyauto
    // tests pass. If this isn't needed in non-test situations, we should
    // consider removing this code and changing the tests.
    // http://code.google.com/p/chromium/issues/detail?id=86397
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);

    TextPosition position(OrdinalNumber::fromOneBasedInt(source.startLine), OrdinalNumber::first());
    return frame()->script().executeScriptInMainWorldAndReturnValue(ScriptSourceCode(source.code, source.url, position));
}

void WebLocalFrameImpl::executeScriptInIsolatedWorld(int worldID, const WebScriptSource* sourcesIn, unsigned numSources, int extensionGroup, WebVector<v8::Local<v8::Value> >* results)
{
    ASSERT(frame());
    RELEASE_ASSERT(worldID > 0);
    RELEASE_ASSERT(worldID < EmbedderWorldIdLimit);

    Vector<ScriptSourceCode> sources;

    for (unsigned i = 0; i < numSources; ++i) {
        TextPosition position(OrdinalNumber::fromOneBasedInt(sourcesIn[i].startLine), OrdinalNumber::first());
        sources.append(ScriptSourceCode(sourcesIn[i].code, sourcesIn[i].url, position));
    }

    if (results) {
        Vector<v8::Local<v8::Value> > scriptResults;
        frame()->script().executeScriptInIsolatedWorld(worldID, sources, extensionGroup, &scriptResults);
        WebVector<v8::Local<v8::Value> > v8Results(scriptResults.size());
        for (unsigned i = 0; i < scriptResults.size(); i++)
            v8Results[i] = v8::Local<v8::Value>::New(toIsolate(frame()), scriptResults[i]);
        results->swap(v8Results);
    } else {
        v8::HandleScope handleScope(toIsolate(frame()));
        frame()->script().executeScriptInIsolatedWorld(worldID, sources, extensionGroup, 0);
    }
}

v8::Handle<v8::Value> WebLocalFrameImpl::callFunctionEvenIfScriptDisabled(v8::Handle<v8::Function> function, v8::Handle<v8::Value> receiver, int argc, v8::Handle<v8::Value> argv[])
{
    ASSERT(frame());
    return frame()->script().callFunction(function, receiver, argc, argv);
}

v8::Local<v8::Context> WebLocalFrameImpl::mainWorldScriptContext() const
{
    return toV8Context(frame(), DOMWrapperWorld::mainWorld());
}

void WebLocalFrameImpl::reload(bool ignoreCache)
{
    ASSERT(frame());
    frame()->loader().reload(ignoreCache ? EndToEndReload : NormalReload);
}

void WebLocalFrameImpl::reloadWithOverrideURL(const WebURL& overrideUrl, bool ignoreCache)
{
    ASSERT(frame());
    frame()->loader().reload(ignoreCache ? EndToEndReload : NormalReload, overrideUrl);
}

void WebLocalFrameImpl::loadRequest(const WebURLRequest& request)
{
    ASSERT(frame());
    ASSERT(!request.isNull());
    const ResourceRequest& resourceRequest = request.toResourceRequest();

    if (resourceRequest.url().protocolIs("javascript")) {
        loadJavaScriptURL(resourceRequest.url());
        return;
    }

    frame()->loader().load(FrameLoadRequest(0, resourceRequest));
}

void WebLocalFrameImpl::loadHistoryItem(const WebHistoryItem& item, WebHistoryLoadType loadType, WebURLRequest::CachePolicy cachePolicy)
{
    ASSERT(frame());
    RefPtr<HistoryItem> historyItem = PassRefPtr<HistoryItem>(item);
    ASSERT(historyItem);
    frame()->loader().loadHistoryItem(historyItem.get(), static_cast<HistoryLoadType>(loadType), static_cast<ResourceRequestCachePolicy>(cachePolicy));
}

void WebLocalFrameImpl::loadData(const WebData& data, const WebString& mimeType, const WebString& textEncoding, const WebURL& baseURL, const WebURL& unreachableURL, bool replace)
{
    ASSERT(frame());

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

    FrameLoadRequest frameRequest(0, request, SubstituteData(data, mimeType, textEncoding, unreachableURL));
    ASSERT(frameRequest.substituteData().isValid());
    frameRequest.setLockBackForwardList(replace);
    frame()->loader().load(frameRequest);
}

void WebLocalFrameImpl::loadHTMLString(const WebData& data, const WebURL& baseURL, const WebURL& unreachableURL, bool replace)
{
    ASSERT(frame());
    loadData(data, WebString::fromUTF8("text/html"), WebString::fromUTF8("UTF-8"), baseURL, unreachableURL, replace);
}

bool WebLocalFrameImpl::isLoading() const
{
    if (!frame())
        return false;
    return frame()->loader().isLoading();
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
    ASSERT(frame());

    // We regard the policy document loader as still provisional.
    DocumentLoader* documentLoader = frame()->loader().provisionalDocumentLoader();
    if (!documentLoader)
        documentLoader = frame()->loader().policyDocumentLoader();

    return DataSourceForDocLoader(documentLoader);
}

WebDataSource* WebLocalFrameImpl::dataSource() const
{
    ASSERT(frame());
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
    String referrer = referrerURL.isEmpty() ? frame()->document()->outgoingReferrer() : String(referrerURL.spec().utf16());
    referrer = SecurityPolicy::generateReferrerHeader(frame()->document()->referrerPolicy(), request.url(), referrer);
    if (referrer.isEmpty())
        return;
    request.setHTTPReferrer(referrer, static_cast<WebReferrerPolicy>(frame()->document()->referrerPolicy()));
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
    return frame()->domWindow()->pendingUnloadEventListeners();
}

void WebLocalFrameImpl::replaceSelection(const WebString& text)
{
    bool selectReplacement = false;
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

bool WebLocalFrameImpl::firstRectForCharacterRange(unsigned location, unsigned length, WebRect& rect) const
{
    if ((location + length < location) && (location + length))
        length = 0;

    Element* editable = frame()->selection().rootEditableElementOrDocumentElement();
    ASSERT(editable);
    RefPtrWillBeRawPtr<Range> range = PlainTextRange(location, location + length).createRange(*editable);
    if (!range)
        return false;
    IntRect intRect = frame()->editor().firstRectForRange(range.get());
    rect = WebRect(intRect);
    rect = frame()->view()->contentsToWindow(rect);
    return true;
}

size_t WebLocalFrameImpl::characterIndexForPoint(const WebPoint& webPoint) const
{
    if (!frame())
        return kNotFound;

    IntPoint point = frame()->view()->windowToContents(webPoint);
    HitTestResult result = frame()->eventHandler().hitTestResultAtPoint(point, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent);
    RefPtrWillBeRawPtr<Range> range = frame()->rangeForPoint(result.roundedPointInInnerNodeFrame());
    if (!range)
        return kNotFound;
    Element* editable = frame()->selection().rootEditableElementOrDocumentElement();
    ASSERT(editable);
    return PlainTextRange::create(*editable, *range.get()).start();
}

bool WebLocalFrameImpl::executeCommand(const WebString& name, const WebNode& node)
{
    ASSERT(frame());

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

    WebPluginContainerImpl* pluginContainer = pluginContainerFromNode(frame(), node);
    if (pluginContainer && pluginContainer->executeEditCommand(name))
        return true;

    bool result = true;

    // Specially handling commands that Editor::execCommand does not directly
    // support.
    if (command == "DeleteToEndOfParagraph") {
        if (!frame()->editor().deleteWithDirection(DirectionForward, ParagraphBoundary, true, false))
            frame()->editor().deleteWithDirection(DirectionForward, CharacterGranularity, true, false);
    } else if (command == "Indent") {
        frame()->editor().indent();
    } else if (command == "Outdent") {
        frame()->editor().outdent();
    } else if (command == "DeleteBackward") {
        result = frame()->editor().command(AtomicString("BackwardDelete")).execute();
    } else if (command == "DeleteForward") {
        result = frame()->editor().command(AtomicString("ForwardDelete")).execute();
    } else if (command == "AdvanceToNextMisspelling") {
        // Wee need to pass false here or else the currently selected word will never be skipped.
        frame()->spellChecker().advanceToNextMisspelling(false);
    } else if (command == "ToggleSpellPanel") {
        frame()->spellChecker().showSpellingGuessPanel();
    } else {
        result = frame()->editor().command(command).execute();
    }
    return result;
}

bool WebLocalFrameImpl::executeCommand(const WebString& name, const WebString& value, const WebNode& node)
{
    ASSERT(frame());
    String webName = name;

    WebPluginContainerImpl* pluginContainer = pluginContainerFromNode(frame(), node);
    if (pluginContainer && pluginContainer->executeEditCommand(name, value))
        return true;

    // moveToBeginningOfDocument and moveToEndfDocument are only handled by WebKit for editable nodes.
    if (!frame()->editor().canEdit() && webName == "moveToBeginningOfDocument")
        return viewImpl()->bubblingScroll(ScrollUp, ScrollByDocument);

    if (!frame()->editor().canEdit() && webName == "moveToEndOfDocument")
        return viewImpl()->bubblingScroll(ScrollDown, ScrollByDocument);

    if (webName == "showGuessPanel") {
        frame()->spellChecker().showSpellingGuessPanel();
        return true;
    }

    return frame()->editor().command(webName).execute(value);
}

bool WebLocalFrameImpl::isCommandEnabled(const WebString& name) const
{
    ASSERT(frame());
    return frame()->editor().command(name).isEnabled();
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
    RefPtrWillBeRawPtr<Range> caretRange = frame()->selection().toNormalizedRange();
    if (!caretRange)
        return;
    WillBeHeapVector<DocumentMarker*> markers = frame()->document()->markers().markersInRange(caretRange.get(), DocumentMarker::MisspellingMarkers());
    if (markers.size() < 1 || markers[0]->startOffset() >= markers[0]->endOffset())
        return;
    RefPtrWillBeRawPtr<Range> markerRange = Range::create(caretRange->ownerDocument(), caretRange->startContainer(), markers[0]->startOffset(), caretRange->endContainer(), markers[0]->endOffset());
    if (!markerRange)
        return;
    frame()->selection().setSelection(VisibleSelection(markerRange.get()), CharacterGranularity);
    frame()->editor().replaceSelectionWithText(text, false, false);
}

void WebLocalFrameImpl::removeSpellingMarkers()
{
    frame()->document()->markers().removeMarkers(DocumentMarker::MisspellingMarkers());
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
    return frame()->selection().toNormalizedRange();
}

WebString WebLocalFrameImpl::selectionAsText() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->selectionAsText();

    RefPtrWillBeRawPtr<Range> range = frame()->selection().toNormalizedRange();
    if (!range)
        return WebString();

    String text = range->text();
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

    RefPtrWillBeRawPtr<Range> range = frame()->selection().toNormalizedRange();
    if (!range)
        return WebString();

    return createMarkup(range.get(), 0, AnnotateForInterchange, false, ResolveNonLocalURLs);
}

void WebLocalFrameImpl::selectWordAroundPosition(LocalFrame* frame, VisiblePosition position)
{
    VisibleSelection selection(position);
    selection.expandUsingGranularity(WordGranularity);

    TextGranularity granularity = selection.isRange() ? WordGranularity : CharacterGranularity;
    frame->selection().setSelection(selection, granularity);
}

bool WebLocalFrameImpl::selectWordAroundCaret()
{
    FrameSelection& selection = frame()->selection();
    if (selection.isNone() || selection.isRange())
        return false;
    selectWordAroundPosition(frame(), selection.selection().visibleStart());
    return true;
}

void WebLocalFrameImpl::selectRange(const WebPoint& base, const WebPoint& extent)
{
    moveRangeSelection(base, extent);
}

void WebLocalFrameImpl::selectRange(const WebRange& webRange)
{
    if (RefPtrWillBeRawPtr<Range> range = static_cast<PassRefPtrWillBeRawPtr<Range> >(webRange))
        frame()->selection().setSelectedRange(range.get(), WebCore::VP_DEFAULT_AFFINITY, FrameSelection::NonDirectional, NotUserTriggered);
}

void WebLocalFrameImpl::moveRangeSelection(const WebPoint& base, const WebPoint& extent)
{
    VisiblePosition basePosition = visiblePositionForWindowPoint(base);
    VisiblePosition extentPosition = visiblePositionForWindowPoint(extent);
    VisibleSelection newSelection = VisibleSelection(basePosition, extentPosition);
    frame()->selection().setSelection(newSelection, CharacterGranularity);
}

void WebLocalFrameImpl::moveCaretSelection(const WebPoint& point)
{
    Element* editable = frame()->selection().rootEditableElement();
    if (!editable)
        return;

    VisiblePosition position = visiblePositionForWindowPoint(point);
    frame()->selection().moveTo(position, UserTriggered);
}

bool WebLocalFrameImpl::setEditableSelectionOffsets(int start, int end)
{
    return frame()->inputMethodController().setEditableSelectionOffsets(PlainTextRange(start, end));
}

bool WebLocalFrameImpl::setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines)
{
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

VisiblePosition WebLocalFrameImpl::visiblePositionForWindowPoint(const WebPoint& point)
{
    // FIXME(bokan): crbug.com/371902 - These scale/pinch transforms shouldn't
    // be ad hoc and explicit.
    PinchViewport& pinchViewport = frame()->page()->frameHost().pinchViewport();
    FloatPoint unscaledPoint(point);
    unscaledPoint.scale(1 / view()->pageScaleFactor(), 1 / view()->pageScaleFactor());
    unscaledPoint.moveBy(pinchViewport.visibleRect().location());

    HitTestRequest request = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent;
    HitTestResult result(frame()->view()->windowToContents(roundedIntPoint(unscaledPoint)));
    frame()->document()->renderView()->layer()->hitTest(request, result);

    if (Node* node = result.targetNode())
        return frame()->selection().selection().visiblePositionRespectingEditingBoundary(result.localPoint(), node);
    return VisiblePosition();
}

WebPlugin* WebLocalFrameImpl::focusedPluginIfInputMethodSupported()
{
    WebPluginContainerImpl* container = WebLocalFrameImpl::pluginContainerFromNode(frame(), WebNode(frame()->document()->focusedElement()));
    if (container && container->supportsInputMethod())
        return container->plugin();
    return 0;
}

int WebLocalFrameImpl::printBegin(const WebPrintParams& printParams, const WebNode& constrainToNode)
{
    ASSERT(!frame()->document()->isFrameSet());
    WebPluginContainerImpl* pluginContainer = 0;
    if (constrainToNode.isNull()) {
        // If this is a plugin document, check if the plugin supports its own
        // printing. If it does, we will delegate all printing to that.
        pluginContainer = pluginContainerFromFrame(frame());
    } else {
        // We only support printing plugin nodes for now.
        pluginContainer = toWebPluginContainerImpl(constrainToNode.pluginContainer());
    }

    if (pluginContainer && pluginContainer->supportsPaginatedPrint())
        m_printContext = adoptPtr(new ChromePluginPrintContext(frame(), pluginContainer, printParams));
    else
        m_printContext = adoptPtr(new ChromePrintContext(frame()));

    FloatRect rect(0, 0, static_cast<float>(printParams.printContentArea.width), static_cast<float>(printParams.printContentArea.height));
    m_printContext->begin(rect.width(), rect.height());
    float pageHeight;
    // We ignore the overlays calculation for now since they are generated in the
    // browser. pageHeight is actually an output parameter.
    m_printContext->computePageRects(rect, 0, 0, 1.0, pageHeight);

    return m_printContext->pageCount();
}

float WebLocalFrameImpl::getPrintPageShrink(int page)
{
    ASSERT(m_printContext && page >= 0);
    return m_printContext->getPageShrink(page);
}

float WebLocalFrameImpl::printPage(int page, WebCanvas* canvas)
{
#if ENABLE(PRINTING)
    ASSERT(m_printContext && page >= 0 && frame() && frame()->document());

    GraphicsContext graphicsContext(canvas);
    graphicsContext.setPrinting(true);
    return m_printContext->spoolPage(graphicsContext, page);
#else
    return 0;
#endif
}

void WebLocalFrameImpl::printEnd()
{
    ASSERT(m_printContext);
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

bool WebLocalFrameImpl::hasCustomPageSizeStyle(int pageIndex)
{
    return frame()->document()->styleForPage(pageIndex)->pageSizeType() != PAGE_SIZE_AUTO;
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
    ASSERT(m_printContext);
    return m_printContext->pageProperty(frame(), propertyName.utf8().data(), pageIndex);
}

bool WebLocalFrameImpl::find(int identifier, const WebString& searchText, const WebFindOptions& options, bool wrapWithinFrame, WebRect* selectionRect)
{
    return ensureTextFinder().find(identifier, searchText, options, wrapWithinFrame, selectionRect);
}

void WebLocalFrameImpl::stopFinding(bool clearSelection)
{
    if (m_textFinder) {
        if (!clearSelection)
            setFindEndstateFocusAndSelection();
        m_textFinder->stopFindingAndClearSelection();
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
    // This function should only be called on the mainframe.
    ASSERT(!parent());
    ensureTextFinder().increaseMatchCount(identifier, count);
}

void WebLocalFrameImpl::resetMatchCount()
{
    ensureTextFinder().resetMatchCount();
}

void WebLocalFrameImpl::sendOrientationChangeEvent()
{
    if (frame())
        frame()->sendOrientationChangeEvent();
}

void WebLocalFrameImpl::dispatchMessageEventWithOriginCheck(const WebSecurityOrigin& intendedTargetOrigin, const WebDOMEvent& event)
{
    ASSERT(!event.isNull());
    frame()->domWindow()->dispatchMessageEventWithOriginCheck(intendedTargetOrigin.get(), event, nullptr);
}

int WebLocalFrameImpl::findMatchMarkersVersion() const
{
    ASSERT(!parent());

    if (m_textFinder)
        return m_textFinder->findMatchMarkersVersion();
    return 0;
}

int WebLocalFrameImpl::selectNearestFindMatch(const WebFloatPoint& point, WebRect* selectionRect)
{
    ASSERT(!parent());
    return ensureTextFinder().selectNearestFindMatch(point, selectionRect);
}

WebFloatRect WebLocalFrameImpl::activeFindMatchRect()
{
    ASSERT(!parent());

    if (m_textFinder)
        return m_textFinder->activeFindMatchRect();
    return WebFloatRect();
}

void WebLocalFrameImpl::findMatchRects(WebVector<WebFloatRect>& outputRects)
{
    ASSERT(!parent());
    ensureTextFinder().findMatchRects(outputRects);
}

void WebLocalFrameImpl::setTickmarks(const WebVector<WebRect>& tickmarks)
{
    if (frameView()) {
        Vector<IntRect> tickmarksConverted(tickmarks.size());
        for (size_t i = 0; i < tickmarks.size(); ++i)
            tickmarksConverted[i] = tickmarks[i];
        frameView()->setTickmarks(tickmarksConverted);
        invalidateScrollbar();
    }
}

WebString WebLocalFrameImpl::contentAsText(size_t maxChars) const
{
    if (!frame())
        return WebString();
    StringBuilder text;
    frameContentAsPlainText(maxChars, frame(), text);
    return text.toString();
}

WebString WebLocalFrameImpl::contentAsMarkup() const
{
    if (!frame())
        return WebString();
    return createFullMarkup(frame()->document());
}

WebString WebLocalFrameImpl::renderTreeAsText(RenderAsTextControls toShow) const
{
    RenderAsTextBehavior behavior = RenderAsTextBehaviorNormal;

    if (toShow & RenderAsTextDebug)
        behavior |= RenderAsTextShowCompositedLayers | RenderAsTextShowAddresses | RenderAsTextShowIDAndClass | RenderAsTextShowLayerNesting;

    if (toShow & RenderAsTextPrinting)
        behavior |= RenderAsTextPrintingMode;

    return externalRepresentation(frame(), behavior);
}

WebString WebLocalFrameImpl::markerTextForListItem(const WebElement& webElement) const
{
    return WebCore::markerTextForListItem(const_cast<Element*>(webElement.constUnwrap<Element>()));
}

void WebLocalFrameImpl::printPagesWithBoundaries(WebCanvas* canvas, const WebSize& pageSizeInPixels)
{
    ASSERT(m_printContext);

    GraphicsContext graphicsContext(canvas);
    graphicsContext.setPrinting(true);

    m_printContext->spoolAllPagesWithBoundaries(graphicsContext, FloatSize(pageSizeInPixels.width, pageSizeInPixels.height));
}

WebRect WebLocalFrameImpl::selectionBoundsRect() const
{
    return hasSelection() ? WebRect(IntRect(frame()->selection().bounds(false))) : WebRect();
}

bool WebLocalFrameImpl::selectionStartHasSpellingMarkerFor(int from, int length) const
{
    if (!frame())
        return false;
    return frame()->spellChecker().selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}

WebString WebLocalFrameImpl::layerTreeAsText(bool showDebugInfo) const
{
    if (!frame())
        return WebString();

    return WebString(frame()->layerTreeAsText(showDebugInfo ? LayerTreeIncludesDebugInfo : LayerTreeNormal));
}

// WebLocalFrameImpl public ---------------------------------------------------------

WebLocalFrame* WebLocalFrame::create(WebFrameClient* client)
{
    return WebLocalFrameImpl::create(client);
}

WebLocalFrameImpl* WebLocalFrameImpl::create(WebFrameClient* client)
{
    return adoptRef(new WebLocalFrameImpl(client)).leakRef();
}

WebLocalFrameImpl::WebLocalFrameImpl(WebFrameClient* client)
    : m_frameLoaderClientImpl(this)
    , m_client(client)
    , m_permissionClient(0)
    , m_inputEventsScaleFactorForEmulation(1)
    , m_userMediaClientImpl(this)
    , m_geolocationClientProxy(adoptPtr(new GeolocationClientProxy(client ? client->geolocationClient() : 0)))
{
    blink::Platform::current()->incrementStatsCounter(webFrameActiveCount);
    frameCount++;
}

WebLocalFrameImpl::~WebLocalFrameImpl()
{
    blink::Platform::current()->decrementStatsCounter(webFrameActiveCount);
    frameCount--;

    cancelPendingScopingEffort();
}

void WebLocalFrameImpl::setWebCoreFrame(PassRefPtr<WebCore::LocalFrame> frame)
{
    m_frame = frame;

    // FIXME: we shouldn't add overhead to every frame by registering these objects when they're not used.
    if (m_frame) {
        OwnPtr<NotificationPresenterImpl> notificationPresenter = adoptPtr(new NotificationPresenterImpl());
        if (m_client)
            notificationPresenter->initialize(m_client->notificationPresenter());

        provideNotification(*m_frame, notificationPresenter.release());
        provideUserMediaTo(*m_frame, &m_userMediaClientImpl);
        provideGeolocationTo(*m_frame, m_geolocationClientProxy.get());
        m_geolocationClientProxy->setController(GeolocationController::from(m_frame.get()));
        provideMIDITo(*m_frame, MIDIClientProxy::create(m_client ? m_client->webMIDIClient() : 0));
        if (RuntimeEnabledFeatures::screenOrientationEnabled())
            ScreenOrientationController::provideTo(*m_frame, m_client ? m_client->webScreenOrientationClient() : 0);
        provideLocalFileSystemTo(*m_frame, LocalFileSystemClient::create());
    }
}

void WebLocalFrameImpl::initializeAsMainFrame(WebCore::Page* page)
{
    setWebCoreFrame(LocalFrame::create(&m_frameLoaderClientImpl, &page->frameHost(), 0));

    // We must call init() after m_frame is assigned because it is referenced
    // during init().
    m_frame->init();
}

PassRefPtr<LocalFrame> WebLocalFrameImpl::createChildFrame(const FrameLoadRequest& request, HTMLFrameOwnerElement* ownerElement)
{
    ASSERT(m_client);
    WebLocalFrameImpl* webframeChild = toWebLocalFrameImpl(m_client->createChildFrame(this, request.frameName()));
    if (!webframeChild)
        return nullptr;

    // FIXME: Using subResourceAttributeName as fallback is not a perfect
    // solution. subResourceAttributeName returns just one attribute name. The
    // element might not have the attribute, and there might be other attributes
    // which can identify the element.
    RefPtr<LocalFrame> child = webframeChild->initializeAsChildFrame(frame()->host(), ownerElement, request.frameName(), ownerElement->getAttribute(ownerElement->subResourceAttributeName()));
    // Initializing the WebCore frame may cause the new child to be detached, since it may dispatch a load event in the parent.
    if (!child->tree().parent())
        return nullptr;

    // If we're moving in the back/forward list, we might want to replace the content
    // of this child frame with whatever was there at that point.
    RefPtr<HistoryItem> childItem;
    if (isBackForwardLoadType(frame()->loader().loadType()) && !frame()->document()->loadEventFinished())
        childItem = PassRefPtr<HistoryItem>(webframeChild->client()->historyItemForNewChildFrame(webframeChild));

    if (childItem)
        child->loader().loadHistoryItem(childItem.get());
    else
        child->loader().load(FrameLoadRequest(0, request.resourceRequest(), "_self"));

    // Note a synchronous navigation (about:blank) would have already processed
    // onload, so it is possible for the child frame to have already been
    // detached by script in the page.
    if (!child->tree().parent())
        return nullptr;
    return child;
}

void WebLocalFrameImpl::didChangeContentsSize(const IntSize& size)
{
    // This is only possible on the main frame.
    if (m_textFinder && m_textFinder->totalMatchCount() > 0) {
        ASSERT(!parent());
        m_textFinder->increaseMarkerVersion();
    }
}

void WebLocalFrameImpl::createFrameView()
{
    TRACE_EVENT0("webkit", "WebLocalFrameImpl::createFrameView");

    ASSERT(frame()); // If frame() doesn't exist, we probably didn't init properly.

    WebViewImpl* webView = viewImpl();
    bool isMainFrame = webView->mainFrameImpl()->frame() == frame();
    if (isMainFrame)
        webView->suppressInvalidations(true);

    frame()->createView(webView->size(), webView->baseBackgroundColor(), webView->isTransparent());
    if (webView->shouldAutoResize() && isMainFrame)
        frame()->view()->enableAutoSizeMode(true, webView->minAutoSize(), webView->maxAutoSize());

    frame()->view()->setInputEventsTransformForEmulation(m_inputEventsOffsetForEmulation, m_inputEventsScaleFactorForEmulation);

    if (isMainFrame)
        webView->suppressInvalidations(false);
}

WebLocalFrameImpl* WebLocalFrameImpl::fromFrame(LocalFrame* frame)
{
    if (!frame)
        return 0;
    return fromFrame(*frame);
}

WebLocalFrameImpl* WebLocalFrameImpl::fromFrame(LocalFrame& frame)
{
    FrameLoaderClient* client = frame.loader().client();
    if (!client || !client->isFrameLoaderClientImpl())
        return 0;
    return toFrameLoaderClientImpl(client)->webFrame();
}

WebLocalFrameImpl* WebLocalFrameImpl::fromFrameOwnerElement(Element* element)
{
    // FIXME: Why do we check specifically for <iframe> and <frame> here? Why can't we get the WebLocalFrameImpl from an <object> element, for example.
    if (!isHTMLFrameElementBase(element))
        return 0;
    return fromFrame(toLocalFrame(toHTMLFrameElementBase(element)->contentFrame()));
}

WebViewImpl* WebLocalFrameImpl::viewImpl() const
{
    if (!frame())
        return 0;
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
    WebLocalFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    if (this != mainFrameImpl->activeMatchFrame())
        return;

    if (Range* activeMatch = m_textFinder->activeMatch()) {
        // If the user has set the selection since the match was found, we
        // don't focus anything.
        VisibleSelection selection(frame()->selection().selection());
        if (!selection.isNone())
            return;

        // Try to find the first focusable node up the chain, which will, for
        // example, focus links if we have found text within the link.
        Node* node = activeMatch->firstNode();
        if (node && node->isInShadowTree()) {
            if (Node* host = node->shadowHost()) {
                if (isHTMLInputElement(*host) || isHTMLTextAreaElement(*host))
                    node = host;
            }
        }
        for (; node; node = node->parentNode()) {
            if (!node->isElementNode())
                continue;
            Element* element = toElement(node);
            if (element->isFocusable()) {
                // Found a focusable parent node. Set the active match as the
                // selection and focus to the focusable node.
                frame()->selection().setSelection(VisibleSelection(activeMatch));
                frame()->document()->setFocusedElement(element);
                return;
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
                frame()->document()->setFocusedElement(element);
                return;
            }
        }

        // No node related to the active match was focusable, so set the
        // active match as the selection (so that when you end the Find session,
        // you'll have the last thing you found highlighted) and make sure that
        // we have nothing focused (otherwise you might have text selected but
        // a link focused, which is weird).
        frame()->selection().setSelection(VisibleSelection(activeMatch));
        frame()->document()->setFocusedElement(nullptr);

        // Finally clear the active match, for two reasons:
        // We just finished the find 'session' and we don't want future (potentially
        // unrelated) find 'sessions' operations to start at the same place.
        // The WebLocalFrameImpl could get reused and the activeMatch could end up pointing
        // to a document that is no longer valid. Keeping an invalid reference around
        // is just asking for trouble.
        m_textFinder->resetActiveMatch();
    }
}

void WebLocalFrameImpl::didFail(const ResourceError& error, bool wasProvisional)
{
    if (!client())
        return;
    WebURLError webError = error;
    if (wasProvisional)
        client()->didFailProvisionalLoad(this, webError);
    else
        client()->didFailLoad(this, webError);
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

    RefPtrWillBeRawPtr<Document> ownerDocument(frame()->document());

    // Protect privileged pages against bookmarklets and other javascript manipulations.
    if (SchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(frame()->document()->url().protocol()))
        return;

    String script = decodeURLEscapeSequences(url.string().substring(strlen("javascript:")));
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    v8::HandleScope handleScope(toIsolate(frame()));
    v8::Local<v8::Value> result = frame()->script().executeScriptInMainWorldAndReturnValue(ScriptSourceCode(script));
    if (result.IsEmpty() || !result->IsString())
        return;
    String scriptResult = toCoreString(v8::Handle<v8::String>::Cast(result));
    if (!frame()->navigationScheduler().locationChangePending())
        frame()->document()->loader()->replaceDocument(scriptResult, ownerDocument.get());
}

void WebLocalFrameImpl::addStyleSheetByURL(const WebString& url)
{
    RefPtrWillBeRawPtr<Element> styleElement = frame()->document()->createElement(HTMLNames::linkTag, false);

    styleElement->setAttribute(HTMLNames::typeAttr, "text/css");
    styleElement->setAttribute(HTMLNames::relAttr, "stylesheet");
    styleElement->setAttribute(HTMLNames::hrefAttr, url);

    frame()->document()->head()->appendChild(styleElement.release(), IGNORE_EXCEPTION);
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

WebLocalFrameImpl* WebLocalFrameImpl::activeMatchFrame() const
{
    ASSERT(!parent());

    if (m_textFinder)
        return m_textFinder->activeMatchFrame();
    return 0;
}

WebCore::Range* WebLocalFrameImpl::activeMatch() const
{
    if (m_textFinder)
        return m_textFinder->activeMatch();
    return 0;
}

TextFinder& WebLocalFrameImpl::ensureTextFinder()
{
    if (!m_textFinder)
        m_textFinder = TextFinder::create(*this);

    return *m_textFinder;
}

void WebLocalFrameImpl::invalidateScrollbar() const
{
    ASSERT(frame() && frame()->view());
    FrameView* view = frame()->view();
    // Invalidate the vertical scroll bar region for the view.
    Scrollbar* scrollbar = view->verticalScrollbar();
    if (scrollbar)
        scrollbar->invalidate();
}

void WebLocalFrameImpl::invalidateAll() const
{
    ASSERT(frame() && frame()->view());
    FrameView* view = frame()->view();
    view->invalidateRect(view->frameRect());
    invalidateScrollbar();
}

PassRefPtr<LocalFrame> WebLocalFrameImpl::initializeAsChildFrame(FrameHost* host, FrameOwner* owner, const AtomicString& name, const AtomicString& fallbackName)
{
    RefPtr<LocalFrame> frame = LocalFrame::create(&m_frameLoaderClientImpl, host, owner);
    setWebCoreFrame(frame);
    frame->tree().setName(name, fallbackName);
    // May dispatch JS events; frame may be detached after this.
    frame->init();
    return frame;
}

} // namespace blink
