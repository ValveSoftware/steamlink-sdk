/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
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

#include "core/frame/LocalFrame.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/InstrumentingAgents.h"
#include "core/dom/DocumentType.h"
#include "core/dom/StyleChangeReason.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/Event.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/input/EventHandler.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/NavigationScheduler.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/TransformRecorder.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "platform/DragImage.h"
#include "platform/JSONValues.h"
#include "platform/PluginScriptForbiddenScope.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/PaintController.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "platform/graphics/paint/TransformDisplayItem.h"
#include "platform/plugins/PluginData.h"
#include "platform/text/TextStream.h"
#include "public/platform/ServiceRegistry.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/platform/WebScreenInfo.h"
#include "public/platform/WebViewScheduler.h"
#include "third_party/skia/include/core/SkImage.h"
#include "wtf/PtrUtil.h"
#include "wtf/StdLibExtras.h"
#include <memory>

namespace blink {

using namespace HTMLNames;

namespace {

// Convenience class for initializing a GraphicsContext to build a DragImage from a specific
// region specified by |bounds|. After painting the using context(), the DragImage returned from
// createImage() will only contain the content in |bounds| with the appropriate device scale
// factor included.
class DragImageBuilder {
    STACK_ALLOCATED();
public:
    DragImageBuilder(const LocalFrame& localFrame, const FloatRect& bounds)
        : m_localFrame(&localFrame)
        , m_bounds(bounds)
    {
        // TODO(oshima): Remove this when all platforms are migrated to use-zoom-for-dsf.
        float deviceScaleFactor = m_localFrame->host()->deviceScaleFactorDeprecated();
        m_bounds.setWidth(m_bounds.width() * deviceScaleFactor);
        m_bounds.setHeight(m_bounds.height() * deviceScaleFactor);
        m_pictureBuilder = wrapUnique(new SkPictureBuilder(SkRect::MakeIWH(m_bounds.width(), m_bounds.height())));

        AffineTransform transform;
        transform.scale(deviceScaleFactor, deviceScaleFactor);
        transform.translate(-m_bounds.x(), -m_bounds.y());
        context().getPaintController().createAndAppend<BeginTransformDisplayItem>(*m_pictureBuilder, transform);
    }

    GraphicsContext& context() { return m_pictureBuilder->context(); }

    std::unique_ptr<DragImage> createImage(
        float opacity,
        RespectImageOrientationEnum imageOrientation = DoNotRespectImageOrientation)
    {
        context().getPaintController().endItem<EndTransformDisplayItem>(*m_pictureBuilder);
        // TODO(fmalita): endRecording() should return a non-const SKP.
        sk_sp<SkPicture> recording(const_cast<SkPicture*>(m_pictureBuilder->endRecording().leakRef()));
        RefPtr<SkImage> skImage = fromSkSp(SkImage::MakeFromPicture(std::move(recording),
            SkISize::Make(m_bounds.width(), m_bounds.height()), nullptr, nullptr));
        RefPtr<Image> image = StaticBitmapImage::create(skImage.release());
        float screenDeviceScaleFactor = m_localFrame->page()->chromeClient().screenInfo().deviceScaleFactor;

        return DragImage::create(image.get(), imageOrientation, screenDeviceScaleFactor, InterpolationHigh, opacity);
    }

private:
    const Member<const LocalFrame> m_localFrame;
    FloatRect m_bounds;
    std::unique_ptr<SkPictureBuilder> m_pictureBuilder;
};

class NodeImageBuilder {
    STACK_ALLOCATED();
public:
    NodeImageBuilder(const LocalFrame& localFrame, const Node& node)
        : m_localFrame(&localFrame)
        , m_draggedLayoutObject(draggedLayoutObjectOf(localFrame, node))
#if DCHECK_IS_ON()
        , m_layoutCount(m_localFrame->view()->layoutCount())
#endif
    {
    }

    ~NodeImageBuilder()
    {
        DCHECK(isValid());
        if (!m_draggedLayoutObject)
            return;
        DCHECK(m_draggedLayoutObject->isDragging());
        m_draggedLayoutObject->updateDragState(false);
    }

    std::unique_ptr<DragImage> createImage()
    {
        DCHECK(isValid());
        if (!m_draggedLayoutObject)
            return nullptr;
        // Paint starting at the nearest stacking context, clipped to the object
        // itself. This will also paint the contents behind the object if the
        // object contains transparency and there are other elements in the same
        // stacking context which stacked below.
        PaintLayer* layer = m_draggedLayoutObject->enclosingLayer();
        if (!layer->stackingNode()->isStackingContext())
            layer = layer->stackingNode()->ancestorStackingContextNode()->layer();
        IntRect absoluteBoundingBox = m_draggedLayoutObject->absoluteBoundingBoxRectIncludingDescendants();
        FloatRect boundingBox = layer->layoutObject()->absoluteToLocalQuad(FloatQuad(absoluteBoundingBox), UseTransforms).boundingBox();
        DragImageBuilder dragImageBuilder(*m_localFrame, boundingBox);
        {
            PaintLayerPaintingInfo paintingInfo(layer, LayoutRect(boundingBox), GlobalPaintFlattenCompositingLayers, LayoutSize());
            PaintLayerFlags flags = PaintLayerHaveTransparency | PaintLayerAppliedTransform | PaintLayerUncachedClipRects;
            PaintLayerPainter(*layer).paintLayer(dragImageBuilder.context(), paintingInfo, flags);
        }
        return dragImageBuilder.createImage(1.0f, LayoutObject::shouldRespectImageOrientation(m_draggedLayoutObject));
    }

private:
    static LayoutObject* draggedLayoutObjectOf(const LocalFrame& localFrame, const Node& node)
    {
        // TODO(yosin): We should handle pseudo-class ":-webkit-drag" as similar
        // as ":hover" and ":active", rather than using flag in |LayoutObject|,
        // to avoid update layout tree here.
        localFrame.view()->updateAllLifecyclePhasesExceptPaint();
        LayoutObject* layoutObject = node.layoutObject();
        if (!layoutObject)
            return nullptr;
        // Update layout object for |node| with pseudo-class ":-webkit-drag".
        layoutObject->updateDragState(true);
        localFrame.view()->updateAllLifecyclePhasesExceptPaint();
        // |node| with pseudo-class ":-webkit-drag" may blow away layout object.
        return node.layoutObject();
    }

    bool isValid() const
    {
#if DCHECK_IS_ON()
        return m_layoutCount == m_localFrame->view()->layoutCount();
#else
        return true;
#endif
    }

    const Member<const LocalFrame> m_localFrame;

    // This class manages |isDrag()| of |m_draggedLayoutObject|.
    LayoutObject* const m_draggedLayoutObject;

#if DCHECK_IS_ON()
    const int m_layoutCount;
#endif
};

inline float parentPageZoomFactor(LocalFrame* frame)
{
    Frame* parent = frame->tree().parent();
    if (!parent || !parent->isLocalFrame())
        return 1;
    return toLocalFrame(parent)->pageZoomFactor();
}

inline float parentTextZoomFactor(LocalFrame* frame)
{
    Frame* parent = frame->tree().parent();
    if (!parent || !parent->isLocalFrame())
        return 1;
    return toLocalFrame(parent)->textZoomFactor();
}

} // namespace

LocalFrame* LocalFrame::create(FrameLoaderClient* client, FrameHost* host, FrameOwner* owner, ServiceRegistry* serviceRegistry)
{
    LocalFrame* frame = new LocalFrame(client, host, owner, serviceRegistry ? serviceRegistry : ServiceRegistry::getEmptyServiceRegistry());
    InspectorInstrumentation::frameAttachedToParent(frame);
    return frame;
}

void LocalFrame::setView(FrameView* view)
{
    ASSERT(!m_view || m_view != view);
    ASSERT(!document() || !document()->isActive());

    eventHandler().clear();

    m_view = view;
}

void LocalFrame::createView(const IntSize& viewportSize, const Color& backgroundColor, bool transparent,
    ScrollbarMode horizontalScrollbarMode, bool horizontalLock,
    ScrollbarMode verticalScrollbarMode, bool verticalLock)
{
    ASSERT(this);
    ASSERT(page());

    bool isLocalRoot = this->isLocalRoot();

    if (isLocalRoot && view())
        view()->setParentVisible(false);

    setView(nullptr);

    FrameView* frameView = nullptr;
    if (isLocalRoot) {
        frameView = FrameView::create(this, viewportSize);

        // The layout size is set by WebViewImpl to support @viewport
        frameView->setLayoutSizeFixedToFrameSize(false);
    } else {
        frameView = FrameView::create(this);
    }

    frameView->setScrollbarModes(horizontalScrollbarMode, verticalScrollbarMode, horizontalLock, verticalLock);

    setView(frameView);

    frameView->updateBackgroundRecursively(backgroundColor, transparent);

    if (isLocalRoot)
        frameView->setParentVisible(true);

    // FIXME: Not clear what the right thing for OOPI is here.
    if (ownerLayoutObject()) {
        HTMLFrameOwnerElement* owner = deprecatedLocalOwner();
        ASSERT(owner);
        // FIXME: OOPI might lead to us temporarily lying to a frame and telling it
        // that it's owned by a FrameOwner that knows nothing about it. If we're
        // lying to this frame, don't let it clobber the existing widget.
        if (owner->contentFrame() == this)
            owner->setWidget(frameView);
    }

    if (owner())
        view()->setCanHaveScrollbars(owner()->scrollingMode() != ScrollbarAlwaysOff);
}

LocalFrame::~LocalFrame()
{
    // Verify that the FrameView has been cleared as part of detaching
    // the frame owner.
    ASSERT(!m_view);
}

DEFINE_TRACE(LocalFrame)
{
    visitor->trace(m_instrumentingAgents);
    visitor->trace(m_loader);
    visitor->trace(m_navigationScheduler);
    visitor->trace(m_view);
    visitor->trace(m_domWindow);
    visitor->trace(m_pagePopupOwner);
    visitor->trace(m_script);
    visitor->trace(m_editor);
    visitor->trace(m_spellChecker);
    visitor->trace(m_selection);
    visitor->trace(m_eventHandler);
    visitor->trace(m_console);
    visitor->trace(m_inputMethodController);
    Frame::trace(visitor);
    LocalFrameLifecycleNotifier::trace(visitor);
    Supplementable<LocalFrame>::trace(visitor);
}

DOMWindow* LocalFrame::domWindow() const
{
    return m_domWindow.get();
}

WindowProxy* LocalFrame::windowProxy(DOMWrapperWorld& world)
{
    return m_script->windowProxy(world);
}

void LocalFrame::navigate(Document& originDocument, const KURL& url, bool replaceCurrentItem, UserGestureStatus userGestureStatus)
{
    m_navigationScheduler->scheduleLocationChange(&originDocument, url.getString(), replaceCurrentItem);
}

void LocalFrame::navigate(const FrameLoadRequest& request)
{
    m_loader.load(request);
}

void LocalFrame::reload(FrameLoadType loadType, ClientRedirectPolicy clientRedirectPolicy)
{
    ASSERT(loadType == FrameLoadTypeReload || loadType == FrameLoadTypeReloadBypassingCache);
    ASSERT(clientRedirectPolicy == ClientRedirectPolicy::NotClientRedirect || loadType == FrameLoadTypeReload);
    if (clientRedirectPolicy == ClientRedirectPolicy::NotClientRedirect) {
        if (!m_loader.currentItem())
            return;
        FrameLoadRequest request = FrameLoadRequest(
            nullptr, m_loader.resourceRequestForReload(loadType, KURL(), clientRedirectPolicy));
        request.setClientRedirect(clientRedirectPolicy);
        m_loader.load(request, loadType);
    } else {
        m_navigationScheduler->scheduleReload();
    }
}

void LocalFrame::detach(FrameDetachType type)
{
    // Note that detach() can be re-entered, so it's not possible to
    // DCHECK(!m_isDetaching) here.
    m_isDetaching = true;

    PluginScriptForbiddenScope forbidPluginDestructorScripting;
    m_loader.stopAllLoaders();
    // Don't allow any new child frames to load in this frame: attaching a new
    // child frame during or after detaching children results in an attached
    // frame on a detached DOM tree, which is bad.
    SubframeLoadingDisabler disabler(*document());
    m_loader.dispatchUnloadEvent();
    detachChildren();
    m_frameScheduler.reset();

    // All done if detaching the subframes brought about a detach of this frame also.
    if (!client())
        return;

    // stopAllLoaders() needs to be called after detachChildren(), because detachChildren()
    // will trigger the unload event handlers of any child frames, and those event
    // handlers might start a new subresource load in this frame.
    m_loader.stopAllLoaders();
    m_loader.detach();
    document()->detach();
    // This is the earliest that scripting can be disabled:
    // - FrameLoader::detach() can fire XHR abort events
    // - Document::detach()'s deferred widget updates can run script.
    ScriptForbiddenScope forbidScript;
    m_loader.clear();
    // Clear FrameScheduler again in case it is recreated in scripting.
    m_frameScheduler.reset();
    if (!client())
        return;

    client()->willBeDetached();
    // Notify ScriptController that the frame is closing, since its cleanup ends up calling
    // back to FrameLoaderClient via WindowProxy.
    script().clearForClose();
    setView(nullptr);
    willDetachFrameHost();
    InspectorInstrumentation::frameDetachedFromParent(this);
    Frame::detach(type);

    // Signal frame destruction here rather than in the destructor.
    // Main motivation is to avoid being dependent on its exact timing (Oilpan.)
    LocalFrameLifecycleNotifier::notifyContextDestroyed();
    m_supplements.clear();
    WeakIdentifierMap<LocalFrame>::notifyObjectDestroyed(this);
}

bool LocalFrame::prepareForCommit()
{
    return loader().prepareForCommit();
}

SecurityContext* LocalFrame::securityContext() const
{
    return document();
}

void LocalFrame::printNavigationErrorMessage(const Frame& targetFrame, const char* reason)
{
    // URLs aren't available for RemoteFrames, so the error message uses their
    // origin instead.
    String targetFrameDescription = targetFrame.isLocalFrame() ? "with URL '" + toLocalFrame(targetFrame).document()->url().getString() + "'" : "with origin '" + targetFrame.securityContext()->getSecurityOrigin()->toString() + "'";
    String message = "Unsafe JavaScript attempt to initiate navigation for frame " + targetFrameDescription + " from frame with URL '" + document()->url().getString() + "'. " + reason + "\n";

    localDOMWindow()->printErrorMessage(message);
}

WindowProxyManager* LocalFrame::getWindowProxyManager() const
{
    return m_script->getWindowProxyManager();
}

bool LocalFrame::shouldClose()
{
    // TODO(dcheng): This should be fixed to dispatch beforeunload events to
    // both local and remote frames.
    return m_loader.shouldClose();
}

void LocalFrame::willDetachFrameHost()
{
    LocalFrameLifecycleNotifier::notifyWillDetachFrameHost();

    // FIXME: Page should take care of updating focus/scrolling instead of Frame.
    // FIXME: It's unclear as to why this is called more than once, but it is,
    // so page() could be null.
    if (page() && page()->focusController().focusedFrame() == this)
        page()->focusController().setFocusedFrame(nullptr);

    if (page() && page()->scrollingCoordinator() && m_view)
        page()->scrollingCoordinator()->willDestroyScrollableArea(m_view.get());
}

void LocalFrame::setDOMWindow(LocalDOMWindow* domWindow)
{
    // Oilpan: setDOMWindow() cannot be used when finalizing. Which
    // is acceptable as its actions are either not needed or handled
    // by other means --
    //
    //  - LocalFrameLifecycleObserver::willDetachFrameHost() will have
    //    signalled the Inspector frameWindowDiscarded() notifications.
    //    We assume that all LocalFrames are detached, where that notification
    //    will have been done.
    //
    //  - Calling LocalDOMWindow::reset() is not needed (called from
    //    Frame::setDOMWindow().) The Member references it clears will now
    //    die with the window. And the registered DOMWindowProperty instances that don't,
    //    only keep a weak reference to this frame, so there's no need to be
    //    explicitly notified that this frame is going away.
    if (domWindow)
        script().clearWindowProxy();

    if (m_domWindow)
        m_domWindow->reset();
    m_domWindow = domWindow;
}

Document* LocalFrame::document() const
{
    return m_domWindow ? m_domWindow->document() : nullptr;
}

void LocalFrame::setPagePopupOwner(Element& owner)
{
    m_pagePopupOwner = &owner;
}

LayoutView* LocalFrame::contentLayoutObject() const
{
    return document() ? document()->layoutView() : nullptr;
}

LayoutViewItem LocalFrame::contentLayoutItem() const
{
    return LayoutViewItem(contentLayoutObject());
}

void LocalFrame::didChangeVisibilityState()
{
    if (document())
        document()->didChangeVisibilityState();

    Frame::didChangeVisibilityState();
}

LocalFrame* LocalFrame::localFrameRoot()
{
    LocalFrame* curFrame = this;
    while (curFrame && curFrame->tree().parent() && curFrame->tree().parent()->isLocalFrame())
        curFrame = toLocalFrame(curFrame->tree().parent());

    return curFrame;
}

void LocalFrame::setPrinting(bool printing, const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkRatio)
{
    // In setting printing, we should not validate resources already cached for the document.
    // See https://bugs.webkit.org/show_bug.cgi?id=43704
    ResourceCacheValidationSuppressor validationSuppressor(document()->fetcher());

    document()->setPrinting(printing);
    view()->adjustMediaTypeForPrinting(printing);

    if (shouldUsePrintingLayout()) {
        view()->forceLayoutForPagination(pageSize, originalPageSize, maximumShrinkRatio);
    } else {
        if (LayoutView* layoutView = view()->layoutView()) {
            layoutView->setPreferredLogicalWidthsDirty();
            layoutView->setNeedsLayout(LayoutInvalidationReason::PrintingChanged);
            layoutView->setShouldDoFullPaintInvalidationForViewAndAllDescendants();
        }
        view()->layout();
        view()->adjustViewSize();
    }

    // Subframes of the one we're printing don't lay out to the page size.
    for (Frame* child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            toLocalFrame(child)->setPrinting(printing, FloatSize(), FloatSize(), 0);
    }
}

bool LocalFrame::shouldUsePrintingLayout() const
{
    // Only top frame being printed should be fit to page size.
    // Subframes should be constrained by parents only.
    return document()->printing() && (!tree().parent() || !tree().parent()->isLocalFrame() || !toLocalFrame(tree().parent())->document()->printing());
}

FloatSize LocalFrame::resizePageRectsKeepingRatio(const FloatSize& originalSize, const FloatSize& expectedSize)
{
    FloatSize resultSize;
    if (contentLayoutItem().isNull())
        return FloatSize();

    if (contentLayoutItem().style()->isHorizontalWritingMode()) {
        ASSERT(fabs(originalSize.width()) > std::numeric_limits<float>::epsilon());
        float ratio = originalSize.height() / originalSize.width();
        resultSize.setWidth(floorf(expectedSize.width()));
        resultSize.setHeight(floorf(resultSize.width() * ratio));
    } else {
        ASSERT(fabs(originalSize.height()) > std::numeric_limits<float>::epsilon());
        float ratio = originalSize.width() / originalSize.height();
        resultSize.setHeight(floorf(expectedSize.height()));
        resultSize.setWidth(floorf(resultSize.height() * ratio));
    }
    return resultSize;
}

void LocalFrame::setPageZoomFactor(float factor)
{
    setPageAndTextZoomFactors(factor, m_textZoomFactor);
}

void LocalFrame::setTextZoomFactor(float factor)
{
    setPageAndTextZoomFactors(m_pageZoomFactor, factor);
}

void LocalFrame::setPageAndTextZoomFactors(float pageZoomFactor, float textZoomFactor)
{
    if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
        return;

    Page* page = this->page();
    if (!page)
        return;

    Document* document = this->document();
    if (!document)
        return;

    // Respect SVGs zoomAndPan="disabled" property in standalone SVG documents.
    // FIXME: How to handle compound documents + zoomAndPan="disabled"? Needs SVG WG clarification.
    if (document->isSVGDocument()) {
        if (!document->accessSVGExtensions().zoomAndPanEnabled())
            return;
    }

    if (m_pageZoomFactor != pageZoomFactor) {
        if (FrameView* view = this->view()) {
            // Update the scroll position when doing a full page zoom, so the content stays in relatively the same position.
            LayoutPoint scrollPosition = view->scrollPosition();
            float percentDifference = (pageZoomFactor / m_pageZoomFactor);
            view->setScrollPosition(
                DoublePoint(scrollPosition.x() * percentDifference, scrollPosition.y() * percentDifference),
                ProgrammaticScroll);
        }
    }

    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;

    for (Frame* child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            toLocalFrame(child)->setPageAndTextZoomFactors(m_pageZoomFactor, m_textZoomFactor);
    }

    document->mediaQueryAffectingValueChanged();
    document->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::Zoom));
    document->updateStyleAndLayoutIgnorePendingStylesheets();
}

void LocalFrame::deviceScaleFactorChanged()
{
    document()->mediaQueryAffectingValueChanged();
    for (Frame* child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            toLocalFrame(child)->deviceScaleFactorChanged();
    }
}

double LocalFrame::devicePixelRatio() const
{
    if (!m_host)
        return 0;

    double ratio = m_host->deviceScaleFactorDeprecated();
    ratio *= pageZoomFactor();
    return ratio;
}

std::unique_ptr<DragImage> LocalFrame::nodeImage(Node& node)
{
    NodeImageBuilder imageNode(*this, node);
    return imageNode.createImage();
}

std::unique_ptr<DragImage> LocalFrame::dragImageForSelection(float opacity)
{
    if (!selection().isRange())
        return nullptr;

    m_view->updateAllLifecyclePhasesExceptPaint();
    ASSERT(document()->isActive());

    FloatRect paintingRect = FloatRect(selection().bounds());
    DragImageBuilder dragImageBuilder(*this, paintingRect);
    GlobalPaintFlags paintFlags = GlobalPaintSelectionOnly | GlobalPaintFlattenCompositingLayers;
    m_view->paintContents(dragImageBuilder.context(), paintFlags, enclosingIntRect(paintingRect));
    return dragImageBuilder.createImage(opacity);
}

String LocalFrame::selectedText() const
{
    return selection().selectedText();
}

String LocalFrame::selectedTextForClipboard() const
{
    return selection().selectedTextForClipboard();
}

PositionWithAffinity LocalFrame::positionForPoint(const IntPoint& framePoint)
{
    HitTestResult result = eventHandler().hitTestResultAtPoint(framePoint);
    Node* node = result.innerNodeOrImageMapImage();
    if (!node)
        return PositionWithAffinity();
    LayoutObject* layoutObject = node->layoutObject();
    if (!layoutObject)
        return PositionWithAffinity();
    const PositionWithAffinity position = layoutObject->positionForPoint(result.localPoint());
    if (position.isNull())
        return PositionWithAffinity(firstPositionInOrBeforeNode(node));
    return position;
}

Document* LocalFrame::documentAtPoint(const IntPoint& pointInRootFrame)
{
    if (!view())
        return nullptr;

    IntPoint pt = view()->rootFrameToContents(pointInRootFrame);

    if (contentLayoutItem().isNull())
        return nullptr;
    HitTestResult result = eventHandler().hitTestResultAtPoint(pt, HitTestRequest::ReadOnly | HitTestRequest::Active);
    return result.innerNode() ? &result.innerNode()->document() : nullptr;
}

EphemeralRange LocalFrame::rangeForPoint(const IntPoint& framePoint)
{
    const PositionWithAffinity positionWithAffinity = positionForPoint(framePoint);
    if (positionWithAffinity.isNull())
        return EphemeralRange();

    VisiblePosition position = createVisiblePosition(positionWithAffinity);
    VisiblePosition previous = previousPositionOf(position);
    if (previous.isNotNull()) {
        const EphemeralRange previousCharacterRange = makeRange(previous, position);
        IntRect rect = editor().firstRectForRange(previousCharacterRange);
        if (rect.contains(framePoint))
            return EphemeralRange(previousCharacterRange);
    }

    VisiblePosition next = nextPositionOf(position);
    const EphemeralRange nextCharacterRange = makeRange(position, next);
    if (nextCharacterRange.isNotNull()) {
        IntRect rect = editor().firstRectForRange(nextCharacterRange);
        if (rect.contains(framePoint))
            return EphemeralRange(nextCharacterRange);
    }

    return EphemeralRange();
}

bool LocalFrame::isURLAllowed(const KURL& url) const
{
    // Exempt about: URLs from self-reference check.
    if (url.protocolIsAbout())
        return true;

    // We allow one level of self-reference because some sites depend on that,
    // but we don't allow more than one.
    bool foundSelfReference = false;
    for (const Frame* frame = this; frame; frame = frame->tree().parent()) {
        if (!frame->isLocalFrame())
            continue;
        if (equalIgnoringFragmentIdentifier(toLocalFrame(frame)->document()->url(), url)) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    return true;
}

bool LocalFrame::shouldReuseDefaultView(const KURL& url) const
{
    // Secure transitions can only happen when navigating from the initial empty
    // document.
    if (!loader().stateMachine()->isDisplayingInitialEmptyDocument())
        return false;

    return document()->isSecureTransitionTo(url);
}

void LocalFrame::removeSpellingMarkersUnderWords(const Vector<String>& words)
{
    spellChecker().removeSpellingMarkersUnderWords(words);
}

String LocalFrame::layerTreeAsText(unsigned flags) const
{
    if (contentLayoutItem().isNull())
        return String();

    RefPtr<JSONObject> layerTree = contentLayoutItem().compositor()->layerTreeAsJSON(static_cast<LayerTreeFlags>(flags));

    if (flags & LayerTreeIncludesPaintInvalidations) {
        RefPtr<JSONArray> objectPaintInvalidations = m_view->trackedObjectPaintInvalidationsAsJSON();
        if (objectPaintInvalidations) {
            if (!layerTree)
                layerTree = JSONObject::create();
            layerTree->setArray("objectPaintInvalidations", objectPaintInvalidations);
        }
    }

    return layerTree ? layerTree->toPrettyJSONString() : String();
}

bool LocalFrame::shouldThrottleRendering() const
{
    return view() && view()->shouldThrottleRendering();
}

inline LocalFrame::LocalFrame(FrameLoaderClient* client, FrameHost* host, FrameOwner* owner, ServiceRegistry* serviceRegistry)
    : Frame(client, host, owner)
    , m_loader(this)
    , m_navigationScheduler(NavigationScheduler::create(this))
    , m_script(ScriptController::create(this))
    , m_editor(Editor::create(*this))
    , m_spellChecker(SpellChecker::create(*this))
    , m_selection(FrameSelection::create(this))
    , m_eventHandler(new EventHandler(this))
    , m_console(FrameConsole::create(*this))
    , m_inputMethodController(InputMethodController::create(*this))
    , m_navigationDisableCount(0)
    , m_pageZoomFactor(parentPageZoomFactor(this))
    , m_textZoomFactor(parentTextZoomFactor(this))
    , m_inViewSourceMode(false)
    , m_serviceRegistry(serviceRegistry)
{
    if (isLocalRoot())
        m_instrumentingAgents = new InstrumentingAgents();
    else
        m_instrumentingAgents = localFrameRoot()->m_instrumentingAgents;
}

WebFrameScheduler* LocalFrame::frameScheduler()
{
    if (!m_frameScheduler.get())
        m_frameScheduler = page()->chromeClient().createFrameScheduler(client()->frameBlameContext());

    ASSERT(m_frameScheduler.get());
    return m_frameScheduler.get();
}

void LocalFrame::scheduleVisualUpdateUnlessThrottled()
{
    if (shouldThrottleRendering())
        return;
    page()->animator().scheduleVisualUpdate(this);
}

FrameLoaderClient* LocalFrame::client() const
{
    return static_cast<FrameLoaderClient*>(Frame::client());
}

PluginData* LocalFrame::pluginData() const
{
    if (!loader().allowPlugins(NotAboutToInstantiatePlugin))
        return nullptr;
    return page()->pluginData();
}

DEFINE_WEAK_IDENTIFIER_MAP(LocalFrame);

FrameNavigationDisabler::FrameNavigationDisabler(LocalFrame& frame)
    : m_frame(&frame)
{
    m_frame->disableNavigation();
}

FrameNavigationDisabler::~FrameNavigationDisabler()
{
    m_frame->enableNavigation();
}

ScopedFrameBlamer::ScopedFrameBlamer(LocalFrame* frame)
    : m_frame(frame)
{
    if (m_frame && m_frame->client() && m_frame->client()->frameBlameContext())
        m_frame->client()->frameBlameContext()->Enter();
}

ScopedFrameBlamer::~ScopedFrameBlamer()
{
    if (m_frame && m_frame->client() && m_frame->client()->frameBlameContext())
        m_frame->client()->frameBlameContext()->Leave();
}

} // namespace blink
