/*
 * Copyright (C) 2007, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Google Inc.
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

#include "core/page/DragController.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/InputTypeNames.h"
#include "core/clipboard/DataObject.h"
#include "core/clipboard/DataTransfer.h"
#include "core/clipboard/DataTransferAccessPolicy.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/DragCaretController.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/serializers/Serialization.h"
#include "core/events/TextEvent.h"
#include "core/fetch/ImageResource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/HitTestRequest.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/page/ChromeClient.h"
#include "core/page/DragData.h"
#include "core/page/DragSession.h"
#include "core/page/DragState.h"
#include "core/page/Page.h"
#include "platform/DragImage.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/BitmapImage.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebDragData.h"
#include "public/platform/WebDragOperation.h"
#include "public/platform/WebImage.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebScreenInfo.h"
#include "wtf/Assertions.h"
#include "wtf/CurrentTime.h"
#include "wtf/RefPtr.h"
#include <memory>

#if OS(WIN)
#include <windows.h>
#endif

namespace blink {

const int DragController::DragIconRightInset = 7;
const int DragController::DragIconBottomInset = 3;

static const int MaxOriginalImageArea = 1500 * 1500;
static const int LinkDragBorderInset = 2;
static const float DragImageAlpha = 0.75f;

#if ENABLE(ASSERT)
static bool dragTypeIsValid(DragSourceAction action)
{
    switch (action) {
    case DragSourceActionDHTML:
    case DragSourceActionImage:
    case DragSourceActionLink:
    case DragSourceActionSelection:
        return true;
    case DragSourceActionNone:
        return false;
    }
    // Make sure MSVC doesn't complain that not all control paths return a value.
    return false;
}
#endif

static PlatformMouseEvent createMouseEvent(DragData* dragData)
{
    return PlatformMouseEvent(dragData->clientPosition(), dragData->globalPosition(),
        LeftButton, PlatformEvent::MouseMoved, 0,
        static_cast<PlatformEvent::Modifiers>(dragData->modifiers()),
        PlatformMouseEvent::RealOrIndistinguishable, monotonicallyIncreasingTime());
}

static DataTransfer* createDraggingDataTransfer(DataTransferAccessPolicy policy, DragData* dragData)
{
    return DataTransfer::create(DataTransfer::DragAndDrop, policy, dragData->platformData());
}

DragController::DragController(Page* page)
    : m_page(page)
    , m_documentUnderMouse(nullptr)
    , m_dragInitiator(nullptr)
    , m_fileInputElementUnderMouse(nullptr)
    , m_documentIsHandlingDrag(false)
    , m_dragDestinationAction(DragDestinationActionNone)
    , m_didInitiateDrag(false)
{
}

DragController* DragController::create(Page* page)
{
    return new DragController(page);
}

static DocumentFragment* documentFragmentFromDragData(DragData* dragData, LocalFrame* frame, Range* context, bool allowPlainText, bool& chosePlainText)
{
    ASSERT(dragData);
    chosePlainText = false;

    Document& document = context->ownerDocument();
    if (dragData->containsCompatibleContent()) {
        if (DocumentFragment* fragment = dragData->asFragment(frame))
            return fragment;

        if (dragData->containsURL(DragData::DoNotConvertFilenames)) {
            String title;
            String url = dragData->asURL(DragData::DoNotConvertFilenames, &title);
            if (!url.isEmpty()) {
                HTMLAnchorElement* anchor = HTMLAnchorElement::create(document);
                anchor->setHref(AtomicString(url));
                if (title.isEmpty()) {
                    // Try the plain text first because the url might be normalized or escaped.
                    if (dragData->containsPlainText())
                        title = dragData->asPlainText();
                    if (title.isEmpty())
                        title = url;
                }
                Node* anchorText = document.createTextNode(title);
                anchor->appendChild(anchorText);
                DocumentFragment* fragment = document.createDocumentFragment();
                fragment->appendChild(anchor);
                return fragment;
            }
        }
    }
    if (allowPlainText && dragData->containsPlainText()) {
        chosePlainText = true;
        return createFragmentFromText(EphemeralRange(context), dragData->asPlainText());
    }

    return nullptr;
}

bool DragController::dragIsMove(FrameSelection& selection, DragData* dragData)
{
    return m_documentUnderMouse == m_dragInitiator && selection.isContentEditable() && selection.isRange() && !isCopyKeyDown(dragData);
}

// FIXME: This method is poorly named.  We're just clearing the selection from the document this drag is exiting.
void DragController::cancelDrag()
{
    m_page->dragCaretController().clear();
}

void DragController::dragEnded()
{
    m_dragInitiator = nullptr;
    m_didInitiateDrag = false;
    m_page->dragCaretController().clear();
}

DragSession DragController::dragEntered(DragData* dragData)
{
    return dragEnteredOrUpdated(dragData);
}

void DragController::dragExited(DragData* dragData)
{
    ASSERT(dragData);
    LocalFrame* mainFrame = m_page->deprecatedLocalMainFrame();

    FrameView* frameView(mainFrame->view());
    if (frameView) {
        DataTransferAccessPolicy policy = (!m_documentUnderMouse || m_documentUnderMouse->getSecurityOrigin()->isLocal()) ? DataTransferReadable : DataTransferTypesReadable;
        DataTransfer* dataTransfer = createDraggingDataTransfer(policy, dragData);
        dataTransfer->setSourceOperation(dragData->draggingSourceOperationMask());
        mainFrame->eventHandler().cancelDragAndDrop(createMouseEvent(dragData), dataTransfer);
        dataTransfer->setAccessPolicy(DataTransferNumb); // invalidate clipboard here for security
    }
    mouseMovedIntoDocument(nullptr);
    if (m_fileInputElementUnderMouse)
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
    m_fileInputElementUnderMouse = nullptr;
}

DragSession DragController::dragUpdated(DragData* dragData)
{
    return dragEnteredOrUpdated(dragData);
}

bool DragController::performDrag(DragData* dragData)
{
    ASSERT(dragData);
    m_documentUnderMouse = m_page->deprecatedLocalMainFrame()->documentAtPoint(dragData->clientPosition());
    if ((m_dragDestinationAction & DragDestinationActionDHTML) && m_documentIsHandlingDrag) {
        LocalFrame* mainFrame = m_page->deprecatedLocalMainFrame();
        bool preventedDefault = false;
        if (mainFrame->view()) {
            // Sending an event can result in the destruction of the view and part.
            DataTransfer* dataTransfer = createDraggingDataTransfer(DataTransferReadable, dragData);
            dataTransfer->setSourceOperation(dragData->draggingSourceOperationMask());
            EventHandler& eventHandler = mainFrame->eventHandler();
            preventedDefault = eventHandler.performDragAndDrop(createMouseEvent(dragData), dataTransfer) != WebInputEventResult::NotHandled;
            if (!preventedDefault) {
                // When drop target is plugin element and it can process drag, we
                // should prevent default behavior.
                const IntPoint point = mainFrame->view()->rootFrameToContents(dragData->clientPosition());
                const HitTestResult result = eventHandler.hitTestResultAtPoint(point);
                preventedDefault |= isHTMLPlugInElement(*result.innerNode()) && toHTMLPlugInElement(result.innerNode())->canProcessDrag();
            }
            dataTransfer->setAccessPolicy(DataTransferNumb); // Invalidate clipboard here for security
        }
        if (preventedDefault) {
            m_documentUnderMouse = nullptr;
            cancelDrag();
            return true;
        }
    }

    if ((m_dragDestinationAction & DragDestinationActionEdit) && concludeEditDrag(dragData)) {
        m_documentUnderMouse = nullptr;
        return true;
    }

    m_documentUnderMouse = nullptr;

    if (operationForLoad(dragData) == DragOperationNone)
        return false;

    if (m_page->settings().navigateOnDragDrop())
        m_page->deprecatedLocalMainFrame()->loader().load(FrameLoadRequest(nullptr, ResourceRequest(dragData->asURL())));
    return true;
}

void DragController::mouseMovedIntoDocument(Document* newDocument)
{
    if (m_documentUnderMouse == newDocument)
        return;

    // If we were over another document clear the selection
    if (m_documentUnderMouse)
        cancelDrag();
    m_documentUnderMouse = newDocument;
}

DragSession DragController::dragEnteredOrUpdated(DragData* dragData)
{
    ASSERT(dragData);
    ASSERT(m_page->mainFrame());
    mouseMovedIntoDocument(m_page->deprecatedLocalMainFrame()->documentAtPoint(dragData->clientPosition()));

    // TODO(esprehn): Replace acceptsLoadDrops with a Setting used in core.
    m_dragDestinationAction = m_page->chromeClient().acceptsLoadDrops()
        ? DragDestinationActionAny
        : static_cast<DragDestinationAction>(DragDestinationActionDHTML | DragDestinationActionEdit);
    if (m_dragDestinationAction == DragDestinationActionNone) {
        cancelDrag(); // FIXME: Why not call mouseMovedIntoDocument(0)?
        return DragSession();
    }

    DragSession dragSession;
    m_documentIsHandlingDrag = tryDocumentDrag(dragData, m_dragDestinationAction, dragSession);
    if (!m_documentIsHandlingDrag && (m_dragDestinationAction & DragDestinationActionLoad))
        dragSession.operation = operationForLoad(dragData);
    return dragSession;
}

static HTMLInputElement* asFileInput(Node* node)
{
    ASSERT(node);
    for (; node; node = node->shadowHost()) {
        if (isHTMLInputElement(*node) && toHTMLInputElement(node)->type() == InputTypeNames::file)
            return toHTMLInputElement(node);
    }
    return nullptr;
}

// This can return null if an empty document is loaded.
static Element* elementUnderMouse(Document* documentUnderMouse, const IntPoint& point)
{
    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
    HitTestResult result(request, point);
    documentUnderMouse->layoutViewItem().hitTest(result);

    Node* n = result.innerNode();
    while (n && !n->isElementNode())
        n = n->parentOrShadowHostNode();
    if (n && n->isInShadowTree())
        n = n->shadowHost();

    return toElement(n);
}

bool DragController::tryDocumentDrag(DragData* dragData, DragDestinationAction actionMask, DragSession& dragSession)
{
    ASSERT(dragData);

    if (!m_documentUnderMouse)
        return false;

    if (m_dragInitiator && !m_documentUnderMouse->getSecurityOrigin()->canAccess(m_dragInitiator->getSecurityOrigin()))
        return false;

    bool isHandlingDrag = false;
    if (actionMask & DragDestinationActionDHTML) {
        isHandlingDrag = tryDHTMLDrag(dragData, dragSession.operation);
        // Do not continue if m_documentUnderMouse has been reset by tryDHTMLDrag.
        // tryDHTMLDrag fires dragenter event. The event listener that listens
        // to this event may create a nested message loop (open a modal dialog),
        // which could process dragleave event and reset m_documentUnderMouse in
        // dragExited.
        if (!m_documentUnderMouse)
            return false;
    }

    // It's unclear why this check is after tryDHTMLDrag.
    // We send drag events in tryDHTMLDrag and that may be the reason.
    FrameView* frameView = m_documentUnderMouse->view();
    if (!frameView)
        return false;

    if (isHandlingDrag) {
        m_page->dragCaretController().clear();
        return true;
    }

    if ((actionMask & DragDestinationActionEdit) && canProcessDrag(dragData)) {
        IntPoint point = frameView->rootFrameToContents(dragData->clientPosition());
        Element* element = elementUnderMouse(m_documentUnderMouse.get(), point);
        if (!element)
            return false;

        HTMLInputElement* elementAsFileInput = asFileInput(element);
        if (m_fileInputElementUnderMouse != elementAsFileInput) {
            if (m_fileInputElementUnderMouse)
                m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
            m_fileInputElementUnderMouse = elementAsFileInput;
        }

        if (!m_fileInputElementUnderMouse)
            m_page->dragCaretController().setCaretPosition(m_documentUnderMouse->frame()->positionForPoint(point));

        LocalFrame* innerFrame = element->document().frame();
        dragSession.operation = dragIsMove(innerFrame->selection(), dragData) ? DragOperationMove : DragOperationCopy;
        dragSession.mouseIsOverFileInput = m_fileInputElementUnderMouse;
        dragSession.numberOfItemsToBeAccepted = 0;

        const unsigned numberOfFiles = dragData->numberOfFiles();
        if (m_fileInputElementUnderMouse) {
            if (m_fileInputElementUnderMouse->isDisabledFormControl())
                dragSession.numberOfItemsToBeAccepted = 0;
            else if (m_fileInputElementUnderMouse->multiple())
                dragSession.numberOfItemsToBeAccepted = numberOfFiles;
            else if (numberOfFiles == 1)
                dragSession.numberOfItemsToBeAccepted = 1;
            else
                dragSession.numberOfItemsToBeAccepted = 0;

            if (!dragSession.numberOfItemsToBeAccepted)
                dragSession.operation = DragOperationNone;
            m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(dragSession.numberOfItemsToBeAccepted);
        } else {
            // We are not over a file input element. The dragged item(s) will only
            // be loaded into the view the number of dragged items is 1.
            dragSession.numberOfItemsToBeAccepted = numberOfFiles != 1 ? 0 : 1;
        }

        return true;
    }

    // We are not over an editable region. Make sure we're clearing any prior drag cursor.
    m_page->dragCaretController().clear();
    if (m_fileInputElementUnderMouse)
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
    m_fileInputElementUnderMouse = nullptr;
    return false;
}

DragOperation DragController::operationForLoad(DragData* dragData)
{
    ASSERT(dragData);
    Document* doc = m_page->deprecatedLocalMainFrame()->documentAtPoint(dragData->clientPosition());

    if (doc && (m_didInitiateDrag || doc->isPluginDocument() || doc->hasEditableStyle()))
        return DragOperationNone;
    return dragOperation(dragData);
}

static bool setSelectionToDragCaret(LocalFrame* frame, VisibleSelection& dragCaret, Range*& range, const IntPoint& point)
{
    frame->selection().setSelection(dragCaret);
    if (frame->selection().isNone()) {
        dragCaret = VisibleSelection(frame->positionForPoint(point));
        frame->selection().setSelection(dragCaret);
        range = createRange(dragCaret.toNormalizedEphemeralRange());
    }
    return !frame->selection().isNone() && frame->selection().isContentEditable();
}

DispatchEventResult DragController::dispatchTextInputEventFor(LocalFrame* innerFrame, DragData* dragData)
{
    ASSERT(m_page->dragCaretController().hasCaret());
    String text = m_page->dragCaretController().isContentRichlyEditable() ? "" : dragData->asPlainText();
    Element* target = innerFrame->editor().findEventTargetFrom(VisibleSelection(m_page->dragCaretController().caretPosition()));
    return target->dispatchEvent(TextEvent::createForDrop(innerFrame->domWindow(), text));
}

bool DragController::concludeEditDrag(DragData* dragData)
{
    ASSERT(dragData);

    HTMLInputElement* fileInput = m_fileInputElementUnderMouse;
    if (m_fileInputElementUnderMouse) {
        m_fileInputElementUnderMouse->setCanReceiveDroppedFiles(false);
        m_fileInputElementUnderMouse = nullptr;
    }

    if (!m_documentUnderMouse)
        return false;

    IntPoint point = m_documentUnderMouse->view()->rootFrameToContents(dragData->clientPosition());
    Element* element = elementUnderMouse(m_documentUnderMouse.get(), point);
    if (!element)
        return false;
    LocalFrame* innerFrame = element->ownerDocument()->frame();
    ASSERT(innerFrame);

    if (m_page->dragCaretController().hasCaret() && dispatchTextInputEventFor(innerFrame, dragData) != DispatchEventResult::NotCanceled)
        return true;

    if (dragData->containsFiles() && fileInput) {
        // fileInput should be the element we hit tested for, unless it was made
        // display:none in a drop event handler.
        ASSERT(fileInput == element || !fileInput->layoutObject());
        if (fileInput->isDisabledFormControl())
            return false;

        return fileInput->receiveDroppedFiles(dragData);
    }

    if (!m_page->dragController().canProcessDrag(dragData)) {
        m_page->dragCaretController().clear();
        return false;
    }

    VisibleSelection dragCaret(m_page->dragCaretController().caretPosition());
    m_page->dragCaretController().clear();
    // |innerFrame| can be removed by event handler called by
    // |dispatchTextInputEventFor()|.
    if (!innerFrame->selection().isAvailable()) {
        // "editing/pasteboard/drop-text-events-sideeffect-crash.html" reaches
        // here.
        return false;
    }
    Range* range = createRange(dragCaret.toNormalizedEphemeralRange());
    Element* rootEditableElement = innerFrame->selection().rootEditableElement();

    // For range to be null a WebKit client must have done something bad while
    // manually controlling drag behaviour
    if (!range)
        return false;
    ResourceFetcher* fetcher = range->ownerDocument().fetcher();
    ResourceCacheValidationSuppressor validationSuppressor(fetcher);
    if (dragIsMove(innerFrame->selection(), dragData) || dragCaret.isContentRichlyEditable()) {
        bool chosePlainText = false;
        DocumentFragment* fragment = documentFragmentFromDragData(dragData, innerFrame, range, true, chosePlainText);
        if (!fragment)
            return false;

        if (dragIsMove(innerFrame->selection(), dragData)) {
            // NSTextView behavior is to always smart delete on moving a selection,
            // but only to smart insert if the selection granularity is word granularity.
            bool smartDelete = innerFrame->editor().smartInsertDeleteEnabled();
            bool smartInsert = smartDelete && innerFrame->selection().granularity() == WordGranularity && dragData->canSmartReplace();
            innerFrame->editor().moveSelectionAfterDragging(fragment, dragCaret.base(), smartInsert, smartDelete);
        } else {
            if (setSelectionToDragCaret(innerFrame, dragCaret, range, point)) {
                ASSERT(m_documentUnderMouse);
                m_documentUnderMouse->frame()->editor().replaceSelectionAfterDragging(fragment, dragData->canSmartReplace(), chosePlainText);
            }
        }
    } else {
        String text = dragData->asPlainText();
        if (text.isEmpty())
            return false;

        if (setSelectionToDragCaret(innerFrame, dragCaret, range, point)) {
            const bool canSmartReplace = false;
            const bool chosePlainText = true;
            ASSERT(m_documentUnderMouse);
            m_documentUnderMouse->frame()->editor().replaceSelectionAfterDragging(createFragmentFromText(EphemeralRange(range), text), canSmartReplace, chosePlainText);
        }
    }

    if (rootEditableElement) {
        if (LocalFrame* frame = rootEditableElement->document().frame())
            frame->eventHandler().updateDragStateAfterEditDragIfNeeded(rootEditableElement);
    }

    return true;
}

bool DragController::canProcessDrag(DragData* dragData)
{
    ASSERT(dragData);

    if (!dragData->containsCompatibleContent())
        return false;

    IntPoint point = m_page->deprecatedLocalMainFrame()->view()->rootFrameToContents(dragData->clientPosition());
    if (m_page->deprecatedLocalMainFrame()->contentLayoutItem().isNull())
        return false;

    HitTestResult result = m_page->deprecatedLocalMainFrame()->eventHandler().hitTestResultAtPoint(point);

    if (!result.innerNode())
        return false;

    if (dragData->containsFiles() && asFileInput(result.innerNode()))
        return true;

    if (isHTMLPlugInElement(*result.innerNode())) {
        HTMLPlugInElement* plugin = toHTMLPlugInElement(result.innerNode());
        if (!plugin->canProcessDrag() && !result.innerNode()->hasEditableStyle())
            return false;
    } else if (!result.innerNode()->hasEditableStyle()) {
        return false;
    }

    if (m_didInitiateDrag && m_documentUnderMouse == m_dragInitiator && result.isSelected())
        return false;

    return true;
}

static DragOperation defaultOperationForDrag(DragOperation srcOpMask)
{
    // This is designed to match IE's operation fallback for the case where
    // the page calls preventDefault() in a drag event but doesn't set dropEffect.
    if (srcOpMask == DragOperationEvery)
        return DragOperationCopy;
    if (srcOpMask == DragOperationNone)
        return DragOperationNone;
    if (srcOpMask & DragOperationMove || srcOpMask & DragOperationGeneric)
        return DragOperationMove;
    if (srcOpMask & DragOperationCopy)
        return DragOperationCopy;
    if (srcOpMask & DragOperationLink)
        return DragOperationLink;

    // FIXME: Does IE really return "generic" even if no operations were allowed by the source?
    return DragOperationGeneric;
}

bool DragController::tryDHTMLDrag(DragData* dragData, DragOperation& operation)
{
    ASSERT(dragData);
    ASSERT(m_documentUnderMouse);
    LocalFrame* mainFrame = m_page->deprecatedLocalMainFrame();
    if (!mainFrame->view())
        return false;

    DataTransferAccessPolicy policy = m_documentUnderMouse->getSecurityOrigin()->isLocal() ? DataTransferReadable : DataTransferTypesReadable;
    DataTransfer* dataTransfer = createDraggingDataTransfer(policy, dragData);
    DragOperation srcOpMask = dragData->draggingSourceOperationMask();
    dataTransfer->setSourceOperation(srcOpMask);

    PlatformMouseEvent event = createMouseEvent(dragData);
    if (mainFrame->eventHandler().updateDragAndDrop(event, dataTransfer) == WebInputEventResult::NotHandled) {
        dataTransfer->setAccessPolicy(DataTransferNumb); // invalidate clipboard here for security
        return false;
    }

    operation = dataTransfer->destinationOperation();
    if (dataTransfer->dropEffectIsUninitialized()) {
        operation = defaultOperationForDrag(srcOpMask);
    } else if (!(srcOpMask & operation)) {
        // The element picked an operation which is not supported by the source
        operation = DragOperationNone;
    }

    dataTransfer->setAccessPolicy(DataTransferNumb); // invalidate clipboard here for security
    return true;
}

Node* DragController::draggableNode(const LocalFrame* src, Node* startNode, const IntPoint& dragOrigin, SelectionDragPolicy selectionDragPolicy, DragSourceAction& dragType) const
{
    if (src->selection().contains(dragOrigin)) {
        dragType = DragSourceActionSelection;
        if (selectionDragPolicy == ImmediateSelectionDragResolution)
            return startNode;
    } else {
        dragType = DragSourceActionNone;
    }

    Node* node = nullptr;
    DragSourceAction candidateDragType = DragSourceActionNone;
    for (const LayoutObject* layoutObject = startNode->layoutObject(); layoutObject; layoutObject = layoutObject->parent()) {
        node = layoutObject->nonPseudoNode();
        if (!node) {
            // Anonymous layout blocks don't correspond to actual DOM nodes, so we skip over them
            // for the purposes of finding a draggable node.
            continue;
        }
        if (dragType != DragSourceActionSelection && node->isTextNode() && node->canStartSelection()) {
            // In this case we have a click in the unselected portion of text. If this text is
            // selectable, we want to start the selection process instead of looking for a parent
            // to try to drag.
            return nullptr;
        }
        if (node->isElementNode()) {
            EUserDrag dragMode = layoutObject->style()->userDrag();
            if (dragMode == DRAG_NONE)
                continue;
            // Even if the image is part of a selection, we always only drag the image in this case.
            if (layoutObject->isImage()
                && src->settings()
                && src->settings()->loadsImagesAutomatically()) {
                dragType = DragSourceActionImage;
                return node;
            }
            // Other draggable elements are considered unselectable.
            if (isHTMLAnchorElement(*node) && toHTMLAnchorElement(node)->isLiveLink()) {
                candidateDragType = DragSourceActionLink;
                break;
            }
            if (dragMode == DRAG_ELEMENT) {
                candidateDragType = DragSourceActionDHTML;
                break;
            }
        }
    }

    if (candidateDragType == DragSourceActionNone) {
        // Either:
        // 1) Nothing under the cursor is considered draggable, so we bail out.
        // 2) There was a selection under the cursor but selectionDragPolicy is set to
        //    DelayedSelectionDragResolution and no other draggable element could be found, so bail
        //    out and allow text selection to start at the cursor instead.
        return nullptr;
    }

    ASSERT(node);
    if (dragType == DragSourceActionSelection) {
        // Dragging unselectable elements in a selection has special behavior if selectionDragPolicy
        // is DelayedSelectionDragResolution and this drag was flagged as a potential selection
        // drag. In that case, don't allow selection and just drag the entire selection instead.
        ASSERT(selectionDragPolicy == DelayedSelectionDragResolution);
        node = startNode;
    } else {
        // If the cursor isn't over a selection, then just drag the node we found earlier.
        ASSERT(dragType == DragSourceActionNone);
        dragType = candidateDragType;
    }
    return node;
}

static ImageResource* getImageResource(Element* element)
{
    ASSERT(element);
    LayoutObject* layoutObject = element->layoutObject();
    if (!layoutObject || !layoutObject->isImage())
        return nullptr;
    LayoutImage* image = toLayoutImage(layoutObject);
    return image->cachedImage();
}

static Image* getImage(Element* element)
{
    ASSERT(element);
    ImageResource* cachedImage = getImageResource(element);
    return (cachedImage && !cachedImage->errorOccurred()) ?
        cachedImage->getImage() : nullptr;
}

static void prepareDataTransferForImageDrag(LocalFrame* source, DataTransfer* dataTransfer, Element* node, const KURL& linkURL, const KURL& imageURL, const String& label)
{
    if (node->isContentRichlyEditable()) {
        Range* range = source->document()->createRange();
        range->selectNode(node, ASSERT_NO_EXCEPTION);
        source->selection().setSelection(VisibleSelection(EphemeralRange(range)));
    }
    dataTransfer->declareAndWriteDragImage(node, !linkURL.isEmpty() ? linkURL : imageURL, label);
}

bool DragController::populateDragDataTransfer(LocalFrame* src, const DragState& state, const IntPoint& dragOrigin)
{
    ASSERT(dragTypeIsValid(state.m_dragType));
    ASSERT(src);
    if (!src->view() || src->contentLayoutItem().isNull())
        return false;

    HitTestResult hitTestResult = src->eventHandler().hitTestResultAtPoint(dragOrigin);
    // FIXME: Can this even happen? I guess it's possible, but should verify
    // with a layout test.
    if (!state.m_dragSrc->isShadowIncludingInclusiveAncestorOf(hitTestResult.innerNode())) {
        // The original node being dragged isn't under the drag origin anymore... maybe it was
        // hidden or moved out from under the cursor. Regardless, we don't want to start a drag on
        // something that's not actually under the drag origin.
        return false;
    }
    const KURL& linkURL = hitTestResult.absoluteLinkURL();
    const KURL& imageURL = hitTestResult.absoluteImageURL();

    DataTransfer* dataTransfer = state.m_dragDataTransfer.get();
    Node* node = state.m_dragSrc.get();

    if (state.m_dragType == DragSourceActionSelection) {
        dataTransfer->writeSelection(src->selection());
    } else if (state.m_dragType == DragSourceActionImage) {
        if (imageURL.isEmpty() || !node || !node->isElementNode())
            return false;
        Element* element = toElement(node);
        prepareDataTransferForImageDrag(src, dataTransfer, element, linkURL, imageURL, hitTestResult.altDisplayString());
    } else if (state.m_dragType == DragSourceActionLink) {
        if (linkURL.isEmpty())
            return false;
        // Simplify whitespace so the title put on the clipboard resembles what the user sees
        // on the web page. This includes replacing newlines with spaces.
        dataTransfer->writeURL(node, linkURL, hitTestResult.textContent().simplifyWhiteSpace());
    }
    // FIXME: For DHTML/draggable element drags, write element markup to clipboard.
    return true;
}

static IntPoint dragLocationForDHTMLDrag(const IntPoint& mouseDraggedPoint, const IntPoint& dragOrigin, const IntPoint& dragImageOffset, bool isLinkImage)
{
    // dragImageOffset is the cursor position relative to the lower-left corner of the image.
    const int yOffset = -dragImageOffset.y();

    if (isLinkImage)
        return IntPoint(mouseDraggedPoint.x() - dragImageOffset.x(), mouseDraggedPoint.y() + yOffset);

    return IntPoint(dragOrigin.x() - dragImageOffset.x(), dragOrigin.y() + yOffset);
}

static IntPoint dragLocationForSelectionDrag(LocalFrame* sourceFrame)
{
    IntRect draggingRect = enclosingIntRect(sourceFrame->selection().bounds());
    int xpos = draggingRect.maxX();
    xpos = draggingRect.x() < xpos ? draggingRect.x() : xpos;
    int ypos = draggingRect.maxY();
    ypos = draggingRect.y() < ypos ? draggingRect.y() : ypos;
    return IntPoint(xpos, ypos);
}

static const IntSize maxDragImageSize(float deviceScaleFactor)
{
#if OS(MACOSX)
    // Match Safari's drag image size.
    static const IntSize maxDragImageSize(400, 400);
#else
    static const IntSize maxDragImageSize(200, 200);
#endif
    IntSize maxSizeInPixels = maxDragImageSize;
    maxSizeInPixels.scale(deviceScaleFactor);
    return maxSizeInPixels;
}

static std::unique_ptr<DragImage> dragImageForImage(Element* element, Image* image, float deviceScaleFactor, const IntPoint& dragOrigin, const IntPoint& imageElementLocation, const IntSize& imageElementSizeInPixels, IntPoint& dragLocation)
{
    std::unique_ptr<DragImage> dragImage;
    IntPoint origin;

    InterpolationQuality interpolationQuality = element->ensureComputedStyle()->imageRendering() == ImageRenderingPixelated ? InterpolationNone : InterpolationHigh;
    RespectImageOrientationEnum shouldRespectImageOrientation = LayoutObject::shouldRespectImageOrientation(element->layoutObject());
    ImageOrientation orientation;

    if (shouldRespectImageOrientation == RespectImageOrientation && image->isBitmapImage())
        orientation = toBitmapImage(image)->currentFrameOrientation();

    IntSize imageSize = orientation.usesWidthAsHeight() ? image->size().transposedSize() : image->size();

    FloatSize imageScale = DragImage::clampedImageScale(imageSize, imageElementSizeInPixels, maxDragImageSize(deviceScaleFactor));

    if (imageSize.area() <= MaxOriginalImageArea
        && (dragImage = DragImage::create(image, shouldRespectImageOrientation,
            deviceScaleFactor, interpolationQuality, DragImageAlpha, imageScale))) {
        IntSize originalSize = imageElementSizeInPixels;
        origin = imageElementLocation;

        IntSize newSize = dragImage->size();

        // Properly orient the drag image and orient it differently if it's smaller than the original
        float scale = newSize.width() / (float)originalSize.width();
        float dx = origin.x() - dragOrigin.x();
        dx *= scale;
        origin.setX((int)(dx + 0.5));
        float dy = origin.y() - dragOrigin.y();
        dy *= scale;
        origin.setY((int)(dy + 0.5));
    }

    dragLocation = dragOrigin + origin;
    return dragImage;
}

static std::unique_ptr<DragImage> dragImageForLink(const KURL& linkURL, const String& linkText, float deviceScaleFactor, const IntPoint& mouseDraggedPoint, IntPoint& dragLoc)
{
    FontDescription fontDescription;
    LayoutTheme::theme().systemFont(blink::CSSValueNone, fontDescription);
    std::unique_ptr<DragImage> dragImage = DragImage::create(linkURL, linkText, fontDescription, deviceScaleFactor);

    IntSize size = dragImage ? dragImage->size() : IntSize();
    IntPoint dragImageOffset(-size.width() / 2, -LinkDragBorderInset);
    dragLoc = IntPoint(mouseDraggedPoint.x() + dragImageOffset.x(), mouseDraggedPoint.y() + dragImageOffset.y());

    return dragImage;
}

bool DragController::startDrag(LocalFrame* src, const DragState& state, const PlatformMouseEvent& dragEvent, const IntPoint& dragOrigin)
{
    ASSERT(dragTypeIsValid(state.m_dragType));
    ASSERT(src);
    if (!src->view() || src->contentLayoutItem().isNull())
        return false;

    HitTestResult hitTestResult = src->eventHandler().hitTestResultAtPoint(dragOrigin);
    if (!state.m_dragSrc->isShadowIncludingInclusiveAncestorOf(hitTestResult.innerNode())) {
        // The original node being dragged isn't under the drag origin anymore... maybe it was
        // hidden or moved out from under the cursor. Regardless, we don't want to start a drag on
        // something that's not actually under the drag origin.
        return false;
    }
    const KURL& linkURL = hitTestResult.absoluteLinkURL();
    const KURL& imageURL = hitTestResult.absoluteImageURL();

    IntPoint mouseDraggedPoint = src->view()->rootFrameToContents(dragEvent.position());

    IntPoint dragLocation;
    IntPoint dragOffset;

    DataTransfer* dataTransfer = state.m_dragDataTransfer.get();
    // We allow DHTML/JS to set the drag image, even if its a link, image or text we're dragging.
    // This is in the spirit of the IE API, which allows overriding of pasteboard data and DragOp.
    std::unique_ptr<DragImage> dragImage = dataTransfer->createDragImage(dragOffset, src);
    if (dragImage) {
        dragLocation = dragLocationForDHTMLDrag(mouseDraggedPoint, dragOrigin, dragOffset, !linkURL.isEmpty());
    }

    Node* node = state.m_dragSrc.get();
    if (state.m_dragType == DragSourceActionSelection) {
        if (!dragImage) {
            dragImage = src->dragImageForSelection(DragImageAlpha);
            dragLocation = dragLocationForSelectionDrag(src);
        }
        doSystemDrag(dragImage.get(), dragLocation, dragOrigin, dataTransfer, src, false);
    } else if (state.m_dragType == DragSourceActionImage) {
        if (imageURL.isEmpty() || !node || !node->isElementNode())
            return false;
        Element* element = toElement(node);
        Image* image = getImage(element);
        if (!image || image->isNull())
            return false;
        // We shouldn't be starting a drag for an image that can't provide an extension.
        // This is an early detection for problems encountered later upon drop.
        ASSERT(!image->filenameExtension().isEmpty());
        if (!dragImage) {
            const IntRect& imageRect = hitTestResult.imageRect();
            IntSize imageSizeInPixels = imageRect.size();
            // TODO(oshima): Remove this scaling and simply pass imageRect to dragImageForImage
            // once all platforms are migrated to use zoom for dsf.
            imageSizeInPixels.scale(src->host()->deviceScaleFactorDeprecated());

            float screenDeviceScaleFactor = src->page()->chromeClient().screenInfo().deviceScaleFactor;
            // Pass the selected image size in DIP becasue dragImageForImage clips the image in DIP.
            // The coordinates of the locations are in Viewport coordinates, and they're converted in the Blink client.
            // TODO(oshima): Currently, the dragged image on high DPI is scaled and can be blurry because of this.
            // Consider to clip in the screen coordinates to use high resolution image on high DPI screens.
            dragImage = dragImageForImage(element, image, screenDeviceScaleFactor, dragOrigin, imageRect.location(), imageSizeInPixels, dragLocation);
        }
        doSystemDrag(dragImage.get(), dragLocation, dragOrigin, dataTransfer, src, false);
    } else if (state.m_dragType == DragSourceActionLink) {
        if (linkURL.isEmpty())
            return false;
        if (src->selection().isCaret() && src->selection().isContentEditable()) {
            // a user can initiate a drag on a link without having any text
            // selected.  In this case, we should expand the selection to
            // the enclosing anchor element
            if (Node* node = enclosingAnchorElement(src->selection().base()))
                src->selection().setSelection(VisibleSelection::selectionFromContentsOfNode(node));
        }

        if (!dragImage) {
            ASSERT(src->page());
            float screenDeviceScaleFactor = src->page()->chromeClient().screenInfo().deviceScaleFactor;
            dragImage = dragImageForLink(linkURL, hitTestResult.textContent(), screenDeviceScaleFactor, mouseDraggedPoint, dragLocation);
        }
        doSystemDrag(dragImage.get(), dragLocation, mouseDraggedPoint, dataTransfer, src, true);
    } else if (state.m_dragType == DragSourceActionDHTML) {
        doSystemDrag(dragImage.get(), dragLocation, dragOrigin, dataTransfer, src, false);
    } else {
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

// TODO(esprehn): forLink is dead code, what was it for?
void DragController::doSystemDrag(DragImage* image, const IntPoint& dragLocation, const IntPoint& eventPos, DataTransfer* dataTransfer, LocalFrame* frame, bool forLink)
{
    // TODO(dcheng): Drag and drop is not yet supported for OOPI.
    if (m_page->mainFrame()->isRemoteFrame())
        return;
    m_didInitiateDrag = true;
    m_dragInitiator = frame->document();

    LocalFrame* mainFrame = m_page->deprecatedLocalMainFrame();
    FrameView* mainFrameView = mainFrame->view();
    IntPoint adjustedDragLocation = mainFrameView->rootFrameToContents(frame->view()->contentsToRootFrame(dragLocation));
    IntPoint adjustedEventPos = mainFrameView->rootFrameToContents(frame->view()->contentsToRootFrame(eventPos));
    WebDragData dragData = dataTransfer->dataObject()->toWebDragData();
    WebDragOperationsMask dragOperationMask = static_cast<WebDragOperationsMask>(dataTransfer->sourceOperation());
    IntSize offsetSize(adjustedEventPos - adjustedDragLocation);
    WebPoint offsetPoint(offsetSize.width(), offsetSize.height());
    WebImage dragImage;

    if (image) {
        float resolutionScale = image->resolutionScale();
        float deviceScaleFactor = m_page->chromeClient().screenInfo().deviceScaleFactor;
        if (deviceScaleFactor != resolutionScale) {
            DCHECK_GT(resolutionScale, 0);
            float scale = deviceScaleFactor / resolutionScale;
            image->scale(scale, scale);
        }
        dragImage = image->bitmap();
    }

    m_page->chromeClient().startDragging(frame, dragData, dragOperationMask, dragImage, offsetPoint);
}

DragOperation DragController::dragOperation(DragData* dragData)
{
    // FIXME: To match the MacOS behaviour we should return DragOperationNone
    // if we are a modal window, we are the drag source, or the window is an
    // attached sheet If this can be determined from within WebCore
    // operationForDrag can be pulled into WebCore itself
    ASSERT(dragData);
    return dragData->containsURL() && !m_didInitiateDrag ? DragOperationCopy : DragOperationNone;
}

bool DragController::isCopyKeyDown(DragData* dragData)
{
    int modifiers = dragData->modifiers();

#if OS(MACOSX)
    return modifiers & PlatformEvent::AltKey;
#else
    return modifiers & PlatformEvent::CtrlKey;
#endif
}

DEFINE_TRACE(DragController)
{
    visitor->trace(m_page);
    visitor->trace(m_documentUnderMouse);
    visitor->trace(m_dragInitiator);
    visitor->trace(m_fileInputElementUnderMouse);
}

} // namespace blink
