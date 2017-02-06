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

#include "public/web/WebAXObject.h"

#include "SkMatrix44.h"
#include "core/HTMLNames.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/editing/markers/DocumentMarker.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/style/ComputedStyle.h"
#include "core/page/Page.h"
#include "modules/accessibility/AXObject.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/AXTable.h"
#include "modules/accessibility/AXTableCell.h"
#include "modules/accessibility/AXTableColumn.h"
#include "modules/accessibility/AXTableRow.h"
#include "platform/PlatformKeyboardEvent.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/web/WebDocument.h"
#include "public/web/WebElement.h"
#include "public/web/WebNode.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

#if DCHECK_IS_ON()
// It's not safe to call some WebAXObject APIs if a layout is pending.
// Clients should call updateLayoutAndCheckValidity first.
static bool isLayoutClean(Document* document)
{
    if (!document || !document->view())
        return false;
    return document->lifecycle().state() >= DocumentLifecycle::LayoutClean
        || ((document->lifecycle().state() == DocumentLifecycle::StyleClean || document->lifecycle().state() == DocumentLifecycle::LayoutSubtreeChangeClean)
            && !document->view()->needsLayout());
}
#endif

WebScopedAXContext::WebScopedAXContext(WebDocument& rootDocument)
    : m_private(ScopedAXObjectCache::create(*rootDocument.unwrap<Document>()))
{
}

WebScopedAXContext::~WebScopedAXContext()
{
    m_private.reset(0);
}

WebAXObject WebScopedAXContext::root() const
{
    return WebAXObject(static_cast<AXObjectCacheImpl*>(m_private->get())->root());
}

void WebAXObject::reset()
{
    m_private.reset();
}

void WebAXObject::assign(const WebAXObject& other)
{
    m_private = other.m_private;
}

bool WebAXObject::equals(const WebAXObject& n) const
{
    return m_private.get() == n.m_private.get();
}

bool WebAXObject::isDetached() const
{
    if (m_private.isNull())
        return true;

    return m_private->isDetached();
}

int WebAXObject::axID() const
{
    if (isDetached())
        return -1;

    return m_private->axObjectID();
}

int WebAXObject::generateAXID() const
{
    if (isDetached())
        return -1;

    return m_private->axObjectCache().platformGenerateAXID();
}

bool WebAXObject::updateLayoutAndCheckValidity()
{
    if (!isDetached()) {
        Document* document = m_private->getDocument();
        if (!document || !document->view())
            return false;
        document->view()->updateLifecycleToCompositingCleanPlusScrolling();
    }

    // Doing a layout can cause this object to be invalid, so check again.
    return !isDetached();
}

WebString WebAXObject::actionVerb() const
{
    if (isDetached())
        return WebString();

    return m_private->actionVerb();
}

bool WebAXObject::canDecrement() const
{
    if (isDetached())
        return false;

    return m_private->isSlider();
}

bool WebAXObject::canIncrement() const
{
    if (isDetached())
        return false;

    return m_private->isSlider();
}

bool WebAXObject::canPress() const
{
    if (isDetached())
        return false;

    return m_private->actionElement() || m_private->isButton() || m_private->isMenuRelated();
}

bool WebAXObject::canSetFocusAttribute() const
{
    if (isDetached())
        return false;

    return m_private->canSetFocusAttribute();
}

bool WebAXObject::canSetValueAttribute() const
{
    if (isDetached())
        return false;

    return m_private->canSetValueAttribute();
}

unsigned WebAXObject::childCount() const
{
    if (isDetached())
        return 0;

    return m_private->children().size();
}

WebAXObject WebAXObject::childAt(unsigned index) const
{
    if (isDetached())
        return WebAXObject();

    if (m_private->children().size() <= index)
        return WebAXObject();

    return WebAXObject(m_private->children()[index]);
}

WebAXObject WebAXObject::parentObject() const
{
    if (isDetached())
        return WebAXObject();

    return WebAXObject(m_private->parentObject());
}

bool WebAXObject::canSetSelectedAttribute() const
{
    if (isDetached())
        return false;

    return m_private->canSetSelectedAttribute();
}

bool WebAXObject::isAnchor() const
{
    if (isDetached())
        return false;

    return m_private->isAnchor();
}

bool WebAXObject::isAriaReadOnly() const
{
    if (isDetached())
        return false;

    return equalIgnoringCase(m_private->getAttribute(HTMLNames::aria_readonlyAttr), "true");
}

WebString WebAXObject::ariaAutoComplete() const
{
    if (isDetached())
        return WebString();

    return m_private->ariaAutoComplete();
}

WebAXAriaCurrentState WebAXObject::ariaCurrentState() const
{
    if (isDetached())
        return WebAXAriaCurrentStateUndefined;

    return static_cast<WebAXAriaCurrentState>(m_private->ariaCurrentState());
}

bool WebAXObject::isButtonStateMixed() const
{
    if (isDetached())
        return false;

    return m_private->checkboxOrRadioValue() == ButtonStateMixed;
}

bool WebAXObject::isChecked() const
{
    if (isDetached())
        return false;

    return m_private->isChecked();
}

bool WebAXObject::isClickable() const
{
    if (isDetached())
        return false;

    return m_private->isClickable();
}

bool WebAXObject::isCollapsed() const
{
    if (isDetached())
        return false;

    return m_private->isCollapsed();
}

bool WebAXObject::isControl() const
{
    if (isDetached())
        return false;

    return m_private->isControl();
}

bool WebAXObject::isEnabled() const
{
    if (isDetached())
        return false;

    return m_private->isEnabled();
}

WebAXExpanded WebAXObject::isExpanded() const
{
    if (isDetached())
        return WebAXExpandedUndefined;

    return static_cast<WebAXExpanded>(m_private->isExpanded());
}

bool WebAXObject::isFocused() const
{
    if (isDetached())
        return false;

    return m_private->isFocused();
}

bool WebAXObject::isHovered() const
{
    if (isDetached())
        return false;

    return m_private->isHovered();
}

bool WebAXObject::isLinked() const
{
    if (isDetached())
        return false;

    return m_private->isLinked();
}

bool WebAXObject::isLoaded() const
{
    if (isDetached())
        return false;

    return m_private->isLoaded();
}

bool WebAXObject::isMultiSelectable() const
{
    if (isDetached())
        return false;

    return m_private->isMultiSelectable();
}

bool WebAXObject::isOffScreen() const
{
    if (isDetached())
        return false;

    return m_private->isOffScreen();
}

bool WebAXObject::isPasswordField() const
{
    if (isDetached())
        return false;

    return m_private->isPasswordField();
}

bool WebAXObject::isPressed() const
{
    if (isDetached())
        return false;

    return m_private->isPressed();
}

bool WebAXObject::isReadOnly() const
{
    if (isDetached())
        return false;

    return m_private->isReadOnly();
}

bool WebAXObject::isRequired() const
{
    if (isDetached())
        return false;

    return m_private->isRequired();
}

bool WebAXObject::isSelected() const
{
    if (isDetached())
        return false;

    return m_private->isSelected();
}

bool WebAXObject::isSelectedOptionActive() const
{
    if (isDetached())
        return false;

    return m_private->isSelectedOptionActive();
}

bool WebAXObject::isVisible() const
{
    if (isDetached())
        return false;

    return m_private->isVisible();
}

bool WebAXObject::isVisited() const
{
    if (isDetached())
        return false;

    return m_private->isVisited();
}

WebString WebAXObject::accessKey() const
{
    if (isDetached())
        return WebString();

    return WebString(m_private->accessKey());
}

unsigned WebAXObject::backgroundColor() const
{
    if (isDetached())
        return 0;

    // RGBA32 is an alias for unsigned int.
    return m_private->backgroundColor();
}

unsigned WebAXObject::color() const
{
    if (isDetached())
        return 0;

    // RGBA32 is an alias for unsigned int.
    return m_private->color();
}

// Deprecated.
void WebAXObject::colorValue(int& r, int& g, int& b) const
{
    if (isDetached())
        return;

    unsigned color = m_private->colorValue();
    r = (color >> 16) & 0xFF;
    g = (color >> 8) & 0xFF;
    b = color & 0xFF;
}

unsigned WebAXObject::colorValue() const
{
    if (isDetached())
        return 0;

    // RGBA32 is an alias for unsigned int.
    return m_private->colorValue();
}

WebAXObject WebAXObject::ariaActiveDescendant() const
{
    if (isDetached())
        return WebAXObject();

    return WebAXObject(m_private->activeDescendant());
}

bool WebAXObject::ariaControls(WebVector<WebAXObject>& controlsElements) const
{
    if (isDetached())
        return false;

    AXObject::AXObjectVector controls;
    m_private->ariaControlsElements(controls);

    WebVector<WebAXObject> result(controls.size());
    for (size_t i = 0; i < controls.size(); i++)
        result[i] = WebAXObject(controls[i]);
    controlsElements.swap(result);

    return true;
}

bool WebAXObject::ariaHasPopup() const
{
    if (isDetached())
        return false;

    return m_private->ariaHasPopup();
}

bool WebAXObject::ariaFlowTo(WebVector<WebAXObject>& flowToElements) const
{
    if (isDetached())
        return false;

    AXObject::AXObjectVector flowTo;
    m_private->ariaFlowToElements(flowTo);

    WebVector<WebAXObject> result(flowTo.size());
    for (size_t i = 0; i < flowTo.size(); i++)
        result[i] = WebAXObject(flowTo[i]);
    flowToElements.swap(result);

    return true;
}

bool WebAXObject::isEditable() const
{
    if (isDetached())
        return false;

    return m_private->isEditable();
}

bool WebAXObject::isMultiline() const
{
    if (isDetached())
        return false;

    return m_private->isMultiline();
}

bool WebAXObject::isRichlyEditable() const
{
    if (isDetached())
        return false;

    return m_private->isRichlyEditable();
}

int WebAXObject::posInSet() const
{
    if (isDetached())
        return 0;

    return m_private->posInSet();
}

int WebAXObject::setSize() const
{
    if (isDetached())
        return 0;

    return m_private->setSize();
}

bool WebAXObject::isInLiveRegion() const
{
    if (isDetached())
        return false;

    return 0 != m_private->liveRegionRoot();
}

bool WebAXObject::liveRegionAtomic() const
{
    if (isDetached())
        return false;

    return m_private->liveRegionAtomic();
}

bool WebAXObject::liveRegionBusy() const
{
    if (isDetached())
        return false;

    return m_private->liveRegionBusy();
}

WebString WebAXObject::liveRegionRelevant() const
{
    if (isDetached())
        return WebString();

    return m_private->liveRegionRelevant();
}

WebString WebAXObject::liveRegionStatus() const
{
    if (isDetached())
        return WebString();

    return m_private->liveRegionStatus();
}

WebAXObject WebAXObject::liveRegionRoot() const
{
    if (isDetached())
        return WebAXObject();

    AXObject* liveRegionRoot = m_private->liveRegionRoot();
    if (liveRegionRoot)
        return WebAXObject(liveRegionRoot);
    return WebAXObject();
}

bool WebAXObject::containerLiveRegionAtomic() const
{
    if (isDetached())
        return false;

    return m_private->containerLiveRegionAtomic();
}

bool WebAXObject::containerLiveRegionBusy() const
{
    if (isDetached())
        return false;

    return m_private->containerLiveRegionBusy();
}

WebString WebAXObject::containerLiveRegionRelevant() const
{
    if (isDetached())
        return WebString();

    return m_private->containerLiveRegionRelevant();
}

WebString WebAXObject::containerLiveRegionStatus() const
{
    if (isDetached())
        return WebString();

    return m_private->containerLiveRegionStatus();
}

bool WebAXObject::ariaOwns(WebVector<WebAXObject>& ownsElements) const
{
    // aria-owns rearranges the accessibility tree rather than just
    // exposing an attribute.

    // FIXME(dmazzoni): remove this function after we stop calling it
    // from Chromium.  http://crbug.com/489590

    return false;
}

WebRect WebAXObject::boundingBoxRect() const
{
    if (isDetached())
        return WebRect();

#if DCHECK_IS_ON()
    DCHECK(isLayoutClean(m_private->getDocument()));
#endif

    return pixelSnappedIntRect(m_private->elementRect());
}

WebString WebAXObject::fontFamily() const
{
    if (isDetached())
        return WebString();

    return m_private->fontFamily();
}

float WebAXObject::fontSize() const
{
    if (isDetached())
        return 0.0f;

    return m_private->fontSize();
}

bool WebAXObject::canvasHasFallbackContent() const
{
    if (isDetached())
        return false;

    return m_private->canvasHasFallbackContent();
}

WebPoint WebAXObject::clickPoint() const
{
    if (isDetached())
        return WebPoint();

    return WebPoint(m_private->clickPoint());
}

WebAXInvalidState WebAXObject::invalidState() const
{
    if (isDetached())
        return WebAXInvalidStateUndefined;

    return static_cast<WebAXInvalidState>(m_private->getInvalidState());
}

// Only used when invalidState() returns WebAXInvalidStateOther.
WebString WebAXObject::ariaInvalidValue() const
{
    if (isDetached())
        return WebString();

    return m_private->ariaInvalidValue();
}

double WebAXObject::estimatedLoadingProgress() const
{
    if (isDetached())
        return 0.0;

    return m_private->estimatedLoadingProgress();
}

int WebAXObject::headingLevel() const
{
    if (isDetached())
        return 0;

    return m_private->headingLevel();
}

int WebAXObject::hierarchicalLevel() const
{
    if (isDetached())
        return 0;

    return m_private->hierarchicalLevel();
}

// FIXME: This method passes in a point that has page scale applied but assumes that (0, 0)
// is the top left of the visual viewport. In other words, the point has the VisualViewport
// scale applied, but not the VisualViewport offset. crbug.com/459591.
WebAXObject WebAXObject::hitTest(const WebPoint& point) const
{
    if (isDetached())
        return WebAXObject();

    IntPoint contentsPoint = m_private->documentFrameView()->soonToBeRemovedUnscaledViewportToContents(point);
    AXObject* hit = m_private->accessibilityHitTest(contentsPoint);

    if (hit)
        return WebAXObject(hit);

    if (m_private->elementRect().contains(contentsPoint))
        return *this;

    return WebAXObject();
}

WebString WebAXObject::keyboardShortcut() const
{
    if (isDetached())
        return WebString();

    String accessKey = m_private->accessKey();
    if (accessKey.isNull())
        return WebString();

    DEFINE_STATIC_LOCAL(String, modifierString, ());
    if (modifierString.isNull()) {
        unsigned modifiers = PlatformKeyboardEvent::accessKeyModifiers();
        // Follow the same order as Mozilla MSAA implementation:
        // Ctrl+Alt+Shift+Meta+key. MSDN states that keyboard shortcut strings
        // should not be localized and defines the separator as "+".
        StringBuilder modifierStringBuilder;
        if (modifiers & PlatformEvent::CtrlKey)
            modifierStringBuilder.append("Ctrl+");
        if (modifiers & PlatformEvent::AltKey)
            modifierStringBuilder.append("Alt+");
        if (modifiers & PlatformEvent::ShiftKey)
            modifierStringBuilder.append("Shift+");
        if (modifiers & PlatformEvent::MetaKey)
            modifierStringBuilder.append("Win+");
        modifierString = modifierStringBuilder.toString();
    }

    return String(modifierString + accessKey);
}

WebString WebAXObject::language() const
{
    if (isDetached())
        return WebString();

    return m_private->language();
}

bool WebAXObject::performDefaultAction() const
{
    if (isDetached())
        return false;

    return m_private->performDefaultAction();
}

bool WebAXObject::increment() const
{
    if (isDetached())
        return false;

    if (canIncrement()) {
        m_private->increment();
        return true;
    }
    return false;
}

bool WebAXObject::decrement() const
{
    if (isDetached())
        return false;

    if (canDecrement()) {
        m_private->decrement();
        return true;
    }
    return false;
}

WebAXOrientation WebAXObject::orientation() const
{
    if (isDetached())
        return WebAXOrientationUndefined;

    return static_cast<WebAXOrientation>(m_private->orientation());
}

bool WebAXObject::press() const
{
    if (isDetached())
        return false;

    return m_private->press();
}

WebAXRole WebAXObject::role() const
{
    if (isDetached())
        return WebAXRoleUnknown;

    return static_cast<WebAXRole>(m_private->roleValue());
}

void WebAXObject::selection(WebAXObject& anchorObject, int& anchorOffset,
    WebAXObject& focusObject, int& focusOffset) const
{
    if (isDetached()) {
        anchorObject = WebAXObject();
        anchorOffset = -1;
        focusObject = WebAXObject();
        focusOffset = -1;
        return;
    }

    AXObject::AXRange axSelection = m_private->selection();
    anchorObject = WebAXObject(axSelection.anchorObject);
    anchorOffset = axSelection.anchorOffset;
    focusObject = WebAXObject(axSelection.focusObject);
    focusOffset = axSelection.focusOffset;
    return;
}

void WebAXObject::setSelection(const WebAXObject& anchorObject, int anchorOffset,
    const WebAXObject& focusObject, int focusOffset) const
{
    if (isDetached())
        return;

    AXObject::AXRange axSelection(anchorObject, anchorOffset,
        focusObject, focusOffset);
    m_private->setSelection(axSelection);
    return;
}

unsigned WebAXObject::selectionEnd() const
{
    if (isDetached())
        return 0;

    AXObject::AXRange axSelection = m_private->selectionUnderObject();
    if (axSelection.focusOffset < 0)
        return 0;

    return axSelection.focusOffset;
}

unsigned WebAXObject::selectionStart() const
{
    if (isDetached())
        return 0;

    AXObject::AXRange axSelection = m_private->selectionUnderObject();
    if (axSelection.anchorOffset < 0)
        return 0;

    return axSelection.anchorOffset;
}

unsigned WebAXObject::selectionEndLineNumber() const
{
    if (isDetached())
        return 0;

    VisiblePosition position = m_private->visiblePositionForIndex(selectionEnd());
    int lineNumber = m_private->lineForPosition(position);
    if (lineNumber < 0)
        return 0;

    return lineNumber;
}

unsigned WebAXObject::selectionStartLineNumber() const
{
    if (isDetached())
        return 0;

    VisiblePosition position = m_private->visiblePositionForIndex(selectionStart());
    int lineNumber = m_private->lineForPosition(position);
    if (lineNumber < 0)
        return 0;

    return lineNumber;
}

void WebAXObject::setFocused(bool on) const
{
    if (!isDetached())
        m_private->setFocused(on);
}

void WebAXObject::setSelectedTextRange(int selectionStart, int selectionEnd) const
{
    if (isDetached())
        return;

    m_private->setSelection(AXObject::AXRange(selectionStart, selectionEnd));
}

void WebAXObject::setValue(WebString value) const
{
    if (isDetached())
        return;

    m_private->setValue(value);
}

void WebAXObject::showContextMenu() const
{
    if (isDetached())
        return;

    Node* node = m_private->getNode();
    if (!node)
        return;

    Element* element = nullptr;
    if (node->isElementNode()) {
        element = toElement(node);
    } else {
        node->updateDistribution();
        ContainerNode* parent = FlatTreeTraversal::parent(*node);
        ASSERT_WITH_SECURITY_IMPLICATION(parent->isElementNode());
        element = toElement(parent);
    }

    if (!element)
        return;

    LocalFrame* frame = element->document().frame();
    if (!frame)
        return;

    WebViewImpl* view = WebLocalFrameImpl::fromFrame(frame)->viewImpl();
    if (!view)
        return;

    view->showContextMenuForElement(WebElement(element));
}

WebString WebAXObject::stringValue() const
{
    if (isDetached())
        return WebString();

    return m_private->stringValue();
}

WebAXTextDirection WebAXObject::textDirection() const
{
    if (isDetached())
        return WebAXTextDirectionLR;

    return static_cast<WebAXTextDirection>(m_private->textDirection());
}

WebAXTextStyle WebAXObject::textStyle() const
{
    if (isDetached())
        return WebAXTextStyleNone;

    return static_cast<WebAXTextStyle>(m_private->getTextStyle());
}

WebURL WebAXObject::url() const
{
    if (isDetached())
        return WebURL();

    return m_private->url();
}

WebString WebAXObject::name(WebAXNameFrom& outNameFrom, WebVector<WebAXObject>& outNameObjects) const
{
    if (isDetached())
        return WebString();

    AXNameFrom nameFrom = AXNameFromUninitialized;
    HeapVector<Member<AXObject>> nameObjects;
    WebString result = m_private->name(nameFrom, &nameObjects);
    outNameFrom = static_cast<WebAXNameFrom>(nameFrom);

    WebVector<WebAXObject> webNameObjects(nameObjects.size());
    for (size_t i = 0; i < nameObjects.size(); i++)
        webNameObjects[i] = WebAXObject(nameObjects[i]);
    outNameObjects.swap(webNameObjects);

    return result;
}

WebString WebAXObject::name() const
{
    if (isDetached())
        return WebString();

    AXNameFrom nameFrom;
    HeapVector<Member<AXObject>> nameObjects;
    return m_private->name(nameFrom, &nameObjects);
}

WebString WebAXObject::description(WebAXNameFrom nameFrom, WebAXDescriptionFrom& outDescriptionFrom, WebVector<WebAXObject>& outDescriptionObjects) const
{
    if (isDetached())
        return WebString();

    AXDescriptionFrom descriptionFrom = AXDescriptionFromUninitialized;
    HeapVector<Member<AXObject>> descriptionObjects;
    String result = m_private->description(static_cast<AXNameFrom>(nameFrom), descriptionFrom, &descriptionObjects);
    outDescriptionFrom = static_cast<WebAXDescriptionFrom>(descriptionFrom);

    WebVector<WebAXObject> webDescriptionObjects(descriptionObjects.size());
    for (size_t i = 0; i < descriptionObjects.size(); i++)
        webDescriptionObjects[i] = WebAXObject(descriptionObjects[i]);
    outDescriptionObjects.swap(webDescriptionObjects);

    return result;
}

WebString WebAXObject::placeholder(WebAXNameFrom nameFrom, WebAXDescriptionFrom descriptionFrom) const
{
    if (isDetached())
        return WebString();

    return m_private->placeholder(static_cast<AXNameFrom>(nameFrom), static_cast<AXDescriptionFrom>(descriptionFrom));
}

bool WebAXObject::supportsRangeValue() const
{
    if (isDetached())
        return false;

    return m_private->supportsRangeValue();
}

WebString WebAXObject::valueDescription() const
{
    if (isDetached())
        return WebString();

    return m_private->valueDescription();
}

float WebAXObject::valueForRange() const
{
    if (isDetached())
        return 0.0;

    return m_private->valueForRange();
}

float WebAXObject::maxValueForRange() const
{
    if (isDetached())
        return 0.0;

    return m_private->maxValueForRange();
}

float WebAXObject::minValueForRange() const
{
    if (isDetached())
        return 0.0;

    return m_private->minValueForRange();
}

WebNode WebAXObject::node() const
{
    if (isDetached())
        return WebNode();

    Node* node = m_private->getNode();
    if (!node)
        return WebNode();

    return WebNode(node);
}

WebDocument WebAXObject::document() const
{
    if (isDetached())
        return WebDocument();

    Document* document = m_private->getDocument();
    if (!document)
        return WebDocument();

    return WebDocument(document);
}

bool WebAXObject::hasComputedStyle() const
{
    if (isDetached())
        return false;

    Document* document = m_private->getDocument();
    if (document)
        document->updateStyleAndLayoutTree();

    Node* node = m_private->getNode();
    if (!node)
        return false;

    return node->ensureComputedStyle();
}

WebString WebAXObject::computedStyleDisplay() const
{
    if (isDetached())
        return WebString();

    Document* document = m_private->getDocument();
    if (document)
        document->updateStyleAndLayoutTree();

    Node* node = m_private->getNode();
    if (!node)
        return WebString();

    const ComputedStyle* computedStyle = node->ensureComputedStyle();
    if (!computedStyle)
        return WebString();

    return WebString(CSSPrimitiveValue::create(computedStyle->display())->cssText());
}

bool WebAXObject::accessibilityIsIgnored() const
{
    if (isDetached())
        return false;

    return m_private->accessibilityIsIgnored();
}

bool WebAXObject::lineBreaks(WebVector<int>& result) const
{
    if (isDetached())
        return false;

    Vector<int> lineBreaksVector;
    m_private->lineBreaks(lineBreaksVector);

    size_t vectorSize = lineBreaksVector.size();
    WebVector<int> lineBreaksWebVector(vectorSize);
    for (size_t i = 0; i< vectorSize; i++)
        lineBreaksWebVector[i] = lineBreaksVector[i];
    result.swap(lineBreaksWebVector);

    return true;
}

unsigned WebAXObject::columnCount() const
{
    if (isDetached())
        return false;

    if (!m_private->isAXTable())
        return 0;

    return toAXTable(m_private.get())->columnCount();
}

unsigned WebAXObject::rowCount() const
{
    if (isDetached())
        return false;

    if (!m_private->isAXTable())
        return 0;

    return toAXTable(m_private.get())->rowCount();
}

WebAXObject WebAXObject::cellForColumnAndRow(unsigned column, unsigned row) const
{
    if (isDetached())
        return WebAXObject();

    if (!m_private->isAXTable())
        return WebAXObject();

    AXTableCell* cell = toAXTable(m_private.get())->cellForColumnAndRow(column, row);
    return WebAXObject(static_cast<AXObject*>(cell));
}

WebAXObject WebAXObject::headerContainerObject() const
{
    if (isDetached())
        return WebAXObject();

    if (!m_private->isAXTable())
        return WebAXObject();

    return WebAXObject(toAXTable(m_private.get())->headerContainer());
}

WebAXObject WebAXObject::rowAtIndex(unsigned rowIndex) const
{
    if (isDetached())
        return WebAXObject();

    if (!m_private->isAXTable())
        return WebAXObject();

    const AXObject::AXObjectVector& rows = toAXTable(m_private.get())->rows();
    if (rowIndex < rows.size())
        return WebAXObject(rows[rowIndex]);

    return WebAXObject();
}

WebAXObject WebAXObject::columnAtIndex(unsigned columnIndex) const
{
    if (isDetached())
        return WebAXObject();

    if (!m_private->isAXTable())
        return WebAXObject();

    const AXObject::AXObjectVector& columns = toAXTable(m_private.get())->columns();
    if (columnIndex < columns.size())
        return WebAXObject(columns[columnIndex]);

    return WebAXObject();
}

unsigned WebAXObject::rowIndex() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableRow())
        return 0;

    return toAXTableRow(m_private.get())->rowIndex();
}

WebAXObject WebAXObject::rowHeader() const
{
    if (isDetached())
        return WebAXObject();

    if (!m_private->isTableRow())
        return WebAXObject();

    return WebAXObject(toAXTableRow(m_private.get())->headerObject());
}

void WebAXObject::rowHeaders(WebVector<WebAXObject>& rowHeaderElements) const
{
    if (isDetached())
        return;

    if (!m_private->isAXTable())
        return;

    AXObject::AXObjectVector headers;
    toAXTable(m_private.get())->rowHeaders(headers);

    size_t headerCount = headers.size();
    WebVector<WebAXObject> result(headerCount);

    for (size_t i = 0; i < headerCount; i++)
        result[i] = WebAXObject(headers[i]);

    rowHeaderElements.swap(result);
}

unsigned WebAXObject::columnIndex() const
{
    if (isDetached())
        return 0;

    if (m_private->roleValue() != ColumnRole)
        return 0;

    return toAXTableColumn(m_private.get())->columnIndex();
}

WebAXObject WebAXObject::columnHeader() const
{
    if (isDetached())
        return WebAXObject();

    if (m_private->roleValue() != ColumnRole)
        return WebAXObject();

    return WebAXObject(toAXTableColumn(m_private.get())->headerObject());
}

void WebAXObject::columnHeaders(WebVector<WebAXObject>& columnHeaderElements) const
{
    if (isDetached())
        return;

    if (!m_private->isAXTable())
        return;

    AXObject::AXObjectVector headers;
    toAXTable(m_private.get())->columnHeaders(headers);

    size_t headerCount = headers.size();
    WebVector<WebAXObject> result(headerCount);

    for (size_t i = 0; i < headerCount; i++)
        result[i] = WebAXObject(headers[i]);

    columnHeaderElements.swap(result);
}

unsigned WebAXObject::cellColumnIndex() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
        return 0;

    std::pair<unsigned, unsigned> columnRange;
    toAXTableCell(m_private.get())->columnIndexRange(columnRange);
    return columnRange.first;
}

unsigned WebAXObject::cellColumnSpan() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
        return 0;

    std::pair<unsigned, unsigned> columnRange;
    toAXTableCell(m_private.get())->columnIndexRange(columnRange);
    return columnRange.second;
}

unsigned WebAXObject::cellRowIndex() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
        return 0;

    std::pair<unsigned, unsigned> rowRange;
    toAXTableCell(m_private.get())->rowIndexRange(rowRange);
    return rowRange.first;
}

unsigned WebAXObject::cellRowSpan() const
{
    if (isDetached())
        return 0;

    if (!m_private->isTableCell())
        return 0;

    std::pair<unsigned, unsigned> rowRange;
    toAXTableCell(m_private.get())->rowIndexRange(rowRange);
    return rowRange.second;
}

WebAXSortDirection WebAXObject::sortDirection() const
{
    if (isDetached())
        return WebAXSortDirectionUndefined;

    return static_cast<WebAXSortDirection>(m_private->getSortDirection());
}

void WebAXObject::loadInlineTextBoxes() const
{
    if (isDetached())
        return;

    m_private->loadInlineTextBoxes();
}

WebAXObject WebAXObject::nextOnLine() const
{
    if (isDetached())
        return WebAXObject();

    return WebAXObject(m_private.get()->nextOnLine());
}

WebAXObject WebAXObject::previousOnLine() const
{
    if (isDetached())
        return WebAXObject();

    return WebAXObject(m_private.get()->previousOnLine());
}

void WebAXObject::markers(
    WebVector<WebAXMarkerType>& types,
    WebVector<int>& starts,
    WebVector<int>& ends) const
{
    if (isDetached())
        return;

    Vector<DocumentMarker::MarkerType> markerTypes;
    Vector<AXObject::AXRange> markerRanges;
    m_private->markers(markerTypes, markerRanges);
    DCHECK_EQ(markerTypes.size(), markerRanges.size());

    WebVector<WebAXMarkerType> webMarkerTypes(markerTypes.size());
    WebVector<int> startOffsets(markerRanges.size());
    WebVector<int> endOffsets(markerRanges.size());
    for (size_t i = 0; i < markerTypes.size(); ++i) {
        webMarkerTypes[i] = static_cast<WebAXMarkerType>(markerTypes[i]);
        DCHECK(markerRanges[i].isSimple());
        startOffsets[i] = markerRanges[i].anchorOffset;
        endOffsets[i] = markerRanges[i].focusOffset;
    }

    types.swap(webMarkerTypes);
    starts.swap(startOffsets);
    ends.swap(endOffsets);
}

void WebAXObject::characterOffsets(WebVector<int>& offsets) const
{
    if (isDetached())
        return;

    Vector<int> offsetsVector;
    m_private->textCharacterOffsets(offsetsVector);

    size_t vectorSize = offsetsVector.size();
    WebVector<int> offsetsWebVector(vectorSize);
    for (size_t i = 0; i < vectorSize; i++)
        offsetsWebVector[i] = offsetsVector[i];
    offsets.swap(offsetsWebVector);
}

void WebAXObject::wordBoundaries(WebVector<int>& starts, WebVector<int>& ends) const
{
    if (isDetached())
        return;

    Vector<AXObject::AXRange> wordBoundaries;
    m_private->wordBoundaries(wordBoundaries);

    WebVector<int> wordStartOffsets(wordBoundaries.size());
    WebVector<int> wordEndOffsets(wordBoundaries.size());
    for (size_t i = 0; i < wordBoundaries.size(); ++i) {
        DCHECK(wordBoundaries[i].isSimple());
        wordStartOffsets[i] = wordBoundaries[i].anchorOffset;
        wordEndOffsets[i] = wordBoundaries[i].focusOffset;
    }

    starts.swap(wordStartOffsets);
    ends.swap(wordEndOffsets);
}

bool WebAXObject::isScrollableContainer() const
{
    if (isDetached())
        return false;

    return m_private->isScrollableContainer();
}

WebPoint WebAXObject::scrollOffset() const
{
    if (isDetached())
        return WebPoint();

    return m_private->scrollOffset();
}

WebPoint WebAXObject::minimumScrollOffset() const
{
    if (isDetached())
        return WebPoint();

    return m_private->minimumScrollOffset();
}

WebPoint WebAXObject::maximumScrollOffset() const
{
    if (isDetached())
        return WebPoint();

    return m_private->maximumScrollOffset();
}

void WebAXObject::setScrollOffset(const WebPoint& offset) const
{
    if (isDetached())
        return;

    m_private->setScrollOffset(offset);
}

void WebAXObject::getRelativeBounds(WebAXObject& offsetContainer, WebFloatRect& boundsInContainer, SkMatrix44& containerTransform) const
{
    if (isDetached())
        return;

#if DCHECK_IS_ON()
    DCHECK(isLayoutClean(m_private->getDocument()));
#endif

    AXObject* container = nullptr;
    FloatRect bounds;
    m_private->getRelativeBounds(&container, bounds, containerTransform);
    offsetContainer = WebAXObject(container);
    boundsInContainer = WebFloatRect(bounds);
}

void WebAXObject::scrollToMakeVisible() const
{
    if (!isDetached())
        m_private->scrollToMakeVisible();
}

void WebAXObject::scrollToMakeVisibleWithSubFocus(const WebRect& subfocus) const
{
    if (!isDetached())
        m_private->scrollToMakeVisibleWithSubFocus(subfocus);
}

void WebAXObject::scrollToGlobalPoint(const WebPoint& point) const
{
    if (!isDetached())
        m_private->scrollToGlobalPoint(point);
}

SkMatrix44 WebAXObject::transformFromLocalParentFrame() const
{
    if (isDetached())
        return SkMatrix44();

    return m_private->transformFromLocalParentFrame();
}

WebAXObject::WebAXObject(AXObject* object)
    : m_private(object)
{
}

WebAXObject& WebAXObject::operator=(AXObject* object)
{
    m_private = object;
    return *this;
}

WebAXObject::operator AXObject*() const
{
    return m_private.get();
}

} // namespace blink
