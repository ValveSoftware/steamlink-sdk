/*
* Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "core/accessibility/AXRenderObject.h"

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/accessibility/AXImageMapLink.h"
#include "core/accessibility/AXInlineTextBox.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/accessibility/AXSVGRoot.h"
#include "core/accessibility/AXSpinButton.h"
#include "core/accessibility/AXTable.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/RenderedPosition.h"
#include "core/editing/TextIterator.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/htmlediting.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/loader/ProgressTracker.h"
#include "core/page/Page.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderFieldset.h"
#include "core/rendering/RenderFileUploadControl.h"
#include "core/rendering/RenderHTMLCanvas.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderListMarker.h"
#include "core/rendering/RenderMenuList.h"
#include "core/rendering/RenderTextControlSingleLine.h"
#include "core/rendering/RenderTextFragment.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderWidget.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/graphics/SVGImage.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/StdLibExtras.h"

using blink::WebLocalizedString;

namespace WebCore {

using namespace HTMLNames;

static inline RenderObject* firstChildInContinuation(const RenderInline& renderer)
{
    RenderBoxModelObject* r = renderer.continuation();

    while (r) {
        if (r->isRenderBlock())
            return r;
        if (RenderObject* child = r->slowFirstChild())
            return child;
        r = toRenderInline(r)->continuation();
    }

    return 0;
}

static inline bool isInlineWithContinuation(RenderObject* object)
{
    if (!object->isBoxModelObject())
        return false;

    RenderBoxModelObject* renderer = toRenderBoxModelObject(object);
    if (!renderer->isRenderInline())
        return false;

    return toRenderInline(renderer)->continuation();
}

static inline RenderObject* firstChildConsideringContinuation(RenderObject* renderer)
{
    RenderObject* firstChild = renderer->slowFirstChild();

    if (!firstChild && isInlineWithContinuation(renderer))
        firstChild = firstChildInContinuation(toRenderInline(*renderer));

    return firstChild;
}

static inline RenderInline* startOfContinuations(RenderObject* r)
{
    if (r->isInlineElementContinuation()) {
        return toRenderInline(r->node()->renderer());
    }

    // Blocks with a previous continuation always have a next continuation
    if (r->isRenderBlock() && toRenderBlock(r)->inlineElementContinuation())
        return toRenderInline(toRenderBlock(r)->inlineElementContinuation()->node()->renderer());

    return 0;
}

static inline RenderObject* endOfContinuations(RenderObject* renderer)
{
    RenderObject* prev = renderer;
    RenderObject* cur = renderer;

    if (!cur->isRenderInline() && !cur->isRenderBlock())
        return renderer;

    while (cur) {
        prev = cur;
        if (cur->isRenderInline()) {
            cur = toRenderInline(cur)->inlineElementContinuation();
            ASSERT(cur || !toRenderInline(prev)->continuation());
        } else {
            cur = toRenderBlock(cur)->inlineElementContinuation();
        }
    }

    return prev;
}

static inline bool lastChildHasContinuation(RenderObject* renderer)
{
    RenderObject* lastChild = renderer->slowLastChild();
    return lastChild && isInlineWithContinuation(lastChild);
}

static RenderBoxModelObject* nextContinuation(RenderObject* renderer)
{
    ASSERT(renderer);
    if (renderer->isRenderInline() && !renderer->isReplaced())
        return toRenderInline(renderer)->continuation();
    if (renderer->isRenderBlock())
        return toRenderBlock(renderer)->inlineElementContinuation();
    return 0;
}

AXRenderObject::AXRenderObject(RenderObject* renderer)
    : AXNodeObject(renderer->node())
    , m_renderer(renderer)
    , m_cachedElementRectDirty(true)
{
#ifndef NDEBUG
    m_renderer->setHasAXObject(true);
#endif
}

PassRefPtr<AXRenderObject> AXRenderObject::create(RenderObject* renderer)
{
    return adoptRef(new AXRenderObject(renderer));
}

AXRenderObject::~AXRenderObject()
{
    ASSERT(isDetached());
}

LayoutRect AXRenderObject::elementRect() const
{
    if (!m_explicitElementRect.isEmpty())
        return m_explicitElementRect;
    if (!m_renderer)
        return LayoutRect();
    if (!m_renderer->isBox())
        return computeElementRect();

    for (const AXObject* obj = this; obj; obj = obj->parentObject()) {
        if (obj->isAXRenderObject())
            toAXRenderObject(obj)->checkCachedElementRect();
    }
    for (const AXObject* obj = this; obj; obj = obj->parentObject()) {
        if (obj->isAXRenderObject())
            toAXRenderObject(obj)->updateCachedElementRect();
    }

    return m_cachedElementRect;
}

void AXRenderObject::setRenderer(RenderObject* renderer)
{
    m_renderer = renderer;
    setNode(renderer->node());
}

RenderBoxModelObject* AXRenderObject::renderBoxModelObject() const
{
    if (!m_renderer || !m_renderer->isBoxModelObject())
        return 0;
    return toRenderBoxModelObject(m_renderer);
}

Document* AXRenderObject::topDocument() const
{
    if (!document())
        return 0;
    return &document()->topDocument();
}

bool AXRenderObject::shouldNotifyActiveDescendant() const
{
    // We want to notify that the combo box has changed its active descendant,
    // but we do not want to change the focus, because focus should remain with the combo box.
    if (isComboBox())
        return true;

    return shouldFocusActiveDescendant();
}

ScrollableArea* AXRenderObject::getScrollableAreaIfScrollable() const
{
    // If the parent is a scroll view, then this object isn't really scrollable, the parent ScrollView should handle the scrolling.
    if (parentObject() && parentObject()->isAXScrollView())
        return 0;

    if (!m_renderer || !m_renderer->isBox())
        return 0;

    RenderBox* box = toRenderBox(m_renderer);
    if (!box->canBeScrolledAndHasScrollableArea())
        return 0;

    return box->scrollableArea();
}

AccessibilityRole AXRenderObject::determineAccessibilityRole()
{
    if (!m_renderer)
        return UnknownRole;

    m_ariaRole = determineAriaRoleAttribute();

    Node* node = m_renderer->node();
    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (ariaRole != UnknownRole)
        return ariaRole;

    RenderBoxModelObject* cssBox = renderBoxModelObject();

    if (node && node->isLink()) {
        if (cssBox && cssBox->isImage())
            return ImageMapRole;
        return LinkRole;
    }
    if (cssBox && cssBox->isListItem())
        return ListItemRole;
    if (m_renderer->isListMarker())
        return ListMarkerRole;
    if (isHTMLButtonElement(node))
        return buttonRoleType();
    if (isHTMLLegendElement(node))
        return LegendRole;
    if (m_renderer->isText())
        return StaticTextRole;
    if (cssBox && cssBox->isImage()) {
        if (isHTMLInputElement(node))
            return ariaHasPopup() ? PopUpButtonRole : ButtonRole;
        if (isSVGImage())
            return SVGRootRole;
        return ImageRole;
    }

    // Note: if JavaScript is disabled, the renderer won't be a RenderHTMLCanvas.
    if (isHTMLCanvasElement(node) && m_renderer->isCanvas())
        return CanvasRole;

    if (cssBox && cssBox->isRenderView())
        return WebAreaRole;

    if (cssBox && cssBox->isTextField())
        return TextFieldRole;

    if (cssBox && cssBox->isTextArea())
        return TextAreaRole;

    if (isHTMLInputElement(node)) {
        HTMLInputElement& input = toHTMLInputElement(*node);
        if (input.isCheckbox())
            return CheckBoxRole;
        if (input.isRadioButton())
            return RadioButtonRole;
        if (input.isTextButton())
            return buttonRoleType();

        const AtomicString& type = input.getAttribute(typeAttr);
        if (equalIgnoringCase(type, "color"))
            return ColorWellRole;
    }

    if (isFileUploadButton())
        return ButtonRole;

    if (cssBox && cssBox->isMenuList())
        return PopUpButtonRole;

    if (headingLevel())
        return HeadingRole;

    if (m_renderer->isSVGImage())
        return ImageRole;
    if (m_renderer->isSVGRoot())
        return SVGRootRole;

    if (node && node->hasTagName(ddTag))
        return DescriptionListDetailRole;

    if (node && node->hasTagName(dtTag))
        return DescriptionListTermRole;

    if (node && (node->hasTagName(rpTag) || node->hasTagName(rtTag)))
        return AnnotationRole;

    // Table sections should be ignored.
    if (m_renderer->isTableSection())
        return IgnoredRole;

    if (m_renderer->isHR())
        return HorizontalRuleRole;

    if (isHTMLParagraphElement(node))
        return ParagraphRole;

    if (isHTMLLabelElement(node))
        return LabelRole;

    if (isHTMLDivElement(node))
        return DivRole;

    if (isHTMLFormElement(node))
        return FormRole;

    if (node && node->hasTagName(articleTag))
        return ArticleRole;

    if (node && node->hasTagName(mainTag))
        return MainRole;

    if (node && node->hasTagName(navTag))
        return NavigationRole;

    if (node && node->hasTagName(asideTag))
        return ComplementaryRole;

    if (node && node->hasTagName(sectionTag))
        return RegionRole;

    if (node && node->hasTagName(addressTag))
        return ContentInfoRole;

    if (node && node->hasTagName(dialogTag))
        return DialogRole;

    // The HTML element should not be exposed as an element. That's what the RenderView element does.
    if (isHTMLHtmlElement(node))
        return IgnoredRole;

    if (node && node->hasTagName(iframeTag))
        return IframeRole;

    if (isEmbeddedObject())
        return EmbeddedObjectRole;

    // There should only be one banner/contentInfo per page. If header/footer are being used within an article or section
    // then it should not be exposed as whole page's banner/contentInfo
    if (node && node->hasTagName(headerTag) && !isDescendantOfElementType(articleTag) && !isDescendantOfElementType(sectionTag))
        return BannerRole;
    if (node && node->hasTagName(footerTag) && !isDescendantOfElementType(articleTag) && !isDescendantOfElementType(sectionTag))
        return FooterRole;

    if (isHTMLAnchorElement(node) && isClickable())
        return LinkRole;

    if (m_renderer->isRenderBlockFlow())
        return GroupRole;

    // If the element does not have role, but it has ARIA attributes, accessibility should fallback to exposing it as a group.
    if (supportsARIAAttributes())
        return GroupRole;

    return UnknownRole;
}

void AXRenderObject::init()
{
    AXNodeObject::init();
}

void AXRenderObject::detach()
{
    AXNodeObject::detach();

    detachRemoteSVGRoot();

#ifndef NDEBUG
    if (m_renderer)
        m_renderer->setHasAXObject(false);
#endif
    m_renderer = 0;
}

//
// Check object role or purpose.
//

bool AXRenderObject::isAttachment() const
{
    RenderBoxModelObject* renderer = renderBoxModelObject();
    if (!renderer)
        return false;
    // Widgets are the replaced elements that we represent to AX as attachments
    bool isWidget = renderer->isWidget();
    ASSERT(!isWidget || (renderer->isReplaced() && !isImage()));
    return isWidget;
}

bool AXRenderObject::isFileUploadButton() const
{
    if (m_renderer && isHTMLInputElement(m_renderer->node())) {
        HTMLInputElement& input = toHTMLInputElement(*m_renderer->node());
        return input.isFileUpload();
    }

    return false;
}

static bool isLinkable(const AXObject& object)
{
    if (!object.renderer())
        return false;

    // See https://wiki.mozilla.org/Accessibility/AT-Windows-API for the elements
    // Mozilla considers linkable.
    return object.isLink() || object.isImage() || object.renderer()->isText();
}

bool AXRenderObject::isLinked() const
{
    if (!isLinkable(*this))
        return false;

    Element* anchor = anchorElement();
    if (!isHTMLAnchorElement(anchor))
        return false;

    return !toHTMLAnchorElement(*anchor).href().isEmpty();
}

bool AXRenderObject::isLoaded() const
{
    return !m_renderer->document().parser();
}

bool AXRenderObject::isOffScreen() const
{
    ASSERT(m_renderer);
    IntRect contentRect = pixelSnappedIntRect(m_renderer->absoluteClippedOverflowRect());
    FrameView* view = m_renderer->frame()->view();
    IntRect viewRect = view->visibleContentRect();
    viewRect.intersect(contentRect);
    return viewRect.isEmpty();
}

bool AXRenderObject::isReadOnly() const
{
    ASSERT(m_renderer);

    if (isWebArea()) {
        Document& document = m_renderer->document();
        HTMLElement* body = document.body();
        if (body && body->rendererIsEditable())
            return false;

        return !document.rendererIsEditable();
    }

    return AXNodeObject::isReadOnly();
}

bool AXRenderObject::isVisited() const
{
    // FIXME: Is it a privacy violation to expose visited information to accessibility APIs?
    return m_renderer->style()->isLink() && m_renderer->style()->insideLink() == InsideVisitedLink;
}

//
// Check object state.
//

bool AXRenderObject::isFocused() const
{
    if (!m_renderer)
        return false;

    Document& document = m_renderer->document();
    Element* focusedElement = document.focusedElement();
    if (!focusedElement)
        return false;

    // A web area is represented by the Document node in the DOM tree, which isn't focusable.
    // Check instead if the frame's selection controller is focused
    if (focusedElement == m_renderer->node()
        || (roleValue() == WebAreaRole && document.frame()->selection().isFocusedAndActive()))
        return true;

    return false;
}

bool AXRenderObject::isSelected() const
{
    if (!m_renderer)
        return false;

    Node* node = m_renderer->node();
    if (!node)
        return false;

    const AtomicString& ariaSelected = getAttribute(aria_selectedAttr);
    if (equalIgnoringCase(ariaSelected, "true"))
        return true;

    if (isTabItem() && isTabItemSelected())
        return true;

    return false;
}

//
// Whether objects are ignored, i.e. not included in the tree.
//

AXObjectInclusion AXRenderObject::defaultObjectInclusion() const
{
    // The following cases can apply to any element that's a subclass of AXRenderObject.

    if (!m_renderer)
        return IgnoreObject;

    if (m_renderer->style()->visibility() != VISIBLE) {
        // aria-hidden is meant to override visibility as the determinant in AX hierarchy inclusion.
        if (equalIgnoringCase(getAttribute(aria_hiddenAttr), "false"))
            return DefaultBehavior;

        return IgnoreObject;
    }

    return AXObject::defaultObjectInclusion();
}

bool AXRenderObject::computeAccessibilityIsIgnored() const
{
#ifndef NDEBUG
    ASSERT(m_initialized);
#endif

    // Check first if any of the common reasons cause this element to be ignored.
    // Then process other use cases that need to be applied to all the various roles
    // that AXRenderObjects take on.
    AXObjectInclusion decision = defaultObjectInclusion();
    if (decision == IncludeObject)
        return false;
    if (decision == IgnoreObject)
        return true;

    // If this element is within a parent that cannot have children, it should not be exposed.
    if (isDescendantOfBarrenParent())
        return true;

    if (roleValue() == IgnoredRole)
        return true;

    if (roleValue() == PresentationalRole || inheritsPresentationalRole())
        return true;

    // An ARIA tree can only have tree items and static text as children.
    if (!isAllowedChildOfTree())
        return true;

    // TODO: we should refactor this - but right now this is necessary to make
    // sure scroll areas stay in the tree.
    if (isAttachment())
        return false;

    // ignore popup menu items because AppKit does
    for (RenderObject* parent = m_renderer->parent(); parent; parent = parent->parent()) {
        if (parent->isBoxModelObject() && toRenderBoxModelObject(parent)->isMenuList())
            return true;
    }

    // find out if this element is inside of a label element.
    // if so, it may be ignored because it's the label for a checkbox or radio button
    AXObject* controlObject = correspondingControlForLabelElement();
    if (controlObject && !controlObject->exposesTitleUIElement() && controlObject->isCheckboxOrRadio())
        return true;

    // NOTE: BRs always have text boxes now, so the text box check here can be removed
    if (m_renderer->isText()) {
        // static text beneath MenuItems and MenuButtons are just reported along with the menu item, so it's ignored on an individual level
        AXObject* parent = parentObjectUnignored();
        if (parent && (parent->ariaRoleAttribute() == MenuItemRole || parent->ariaRoleAttribute() == MenuButtonRole))
            return true;
        RenderText* renderText = toRenderText(m_renderer);
        if (m_renderer->isBR() || !renderText->firstTextBox())
            return true;

        // Don't ignore static text in editable text controls.
        for (AXObject* parent = parentObject(); parent; parent = parent->parentObject()) {
            if (parent->roleValue() == TextFieldRole || parent->roleValue() == TextAreaRole)
                return false;
        }

        // text elements that are just empty whitespace should not be returned
        // FIXME(dmazzoni): we probably shouldn't ignore this if the style is 'pre', or similar...
        return renderText->text().impl()->containsOnlyWhitespace();
    }

    if (isHeading())
        return false;

    if (isLandmarkRelated())
        return false;

    if (isLink())
        return false;

    // all controls are accessible
    if (isControl())
        return false;

    if (ariaRoleAttribute() != UnknownRole)
        return false;

    // don't ignore labels, because they serve as TitleUIElements
    Node* node = m_renderer->node();
    if (isHTMLLabelElement(node))
        return false;

    // Anything that is content editable should not be ignored.
    // However, one cannot just call node->rendererIsEditable() since that will ask if its parents
    // are also editable. Only the top level content editable region should be exposed.
    if (hasContentEditableAttributeSet())
        return false;

    // List items play an important role in defining the structure of lists. They should not be ignored.
    if (roleValue() == ListItemRole)
        return false;

    if (roleValue() == DialogRole)
        return false;

    // if this element has aria attributes on it, it should not be ignored.
    if (supportsARIAAttributes())
        return false;

    // <span> tags are inline tags and not meant to convey information if they have no other aria
    // information on them. If we don't ignore them, they may emit signals expected to come from
    // their parent. In addition, because included spans are GroupRole objects, and GroupRole
    // objects are often containers with meaningful information, the inclusion of a span can have
    // the side effect of causing the immediate parent accessible to be ignored. This is especially
    // problematic for platforms which have distinct roles for textual block elements.
    if (isHTMLSpanElement(node))
        return true;

    if (m_renderer->isRenderBlockFlow() && m_renderer->childrenInline() && !canSetFocusAttribute())
        return !toRenderBlock(m_renderer)->firstLineBox() && !mouseButtonListener();

    // ignore images seemingly used as spacers
    if (isImage()) {

        // If the image can take focus, it should not be ignored, lest the user not be able to interact with something important.
        if (canSetFocusAttribute())
            return false;

        if (node && node->isElementNode()) {
            Element* elt = toElement(node);
            const AtomicString& alt = elt->getAttribute(altAttr);
            // don't ignore an image that has an alt tag
            if (!alt.string().containsOnlyWhitespace())
                return false;
            // informal standard is to ignore images with zero-length alt strings
            if (!alt.isNull())
                return true;
        }

        if (isNativeImage() && m_renderer->isImage()) {
            // check for one-dimensional image
            RenderImage* image = toRenderImage(m_renderer);
            if (image->height() <= 1 || image->width() <= 1)
                return true;

            // check whether rendered image was stretched from one-dimensional file image
            if (image->cachedImage()) {
                LayoutSize imageSize = image->cachedImage()->imageSizeForRenderer(m_renderer, image->view()->zoomFactor());
                return imageSize.height() <= 1 || imageSize.width() <= 1;
            }
        }
        return false;
    }

    if (isCanvas()) {
        if (canvasHasFallbackContent())
            return false;
        RenderHTMLCanvas* canvas = toRenderHTMLCanvas(m_renderer);
        if (canvas->height() <= 1 || canvas->width() <= 1)
            return true;
        // Otherwise fall through; use presence of help text, title, or description to decide.
    }

    if (isWebArea() || m_renderer->isListMarker())
        return false;

    // Using the help text, title or accessibility description (so we
    // check if there's some kind of accessible name for the element)
    // to decide an element's visibility is not as definitive as
    // previous checks, so this should remain as one of the last.
    //
    // These checks are simplified in the interest of execution speed;
    // for example, any element having an alt attribute will make it
    // not ignored, rather than just images.
    if (!getAttribute(aria_helpAttr).isEmpty() || !getAttribute(aria_describedbyAttr).isEmpty() || !getAttribute(altAttr).isEmpty() || !getAttribute(titleAttr).isEmpty())
        return false;

    // Don't ignore generic focusable elements like <div tabindex=0>
    // unless they're completely empty, with no children.
    if (isGenericFocusableElement() && node->firstChild())
        return false;

    if (!ariaAccessibilityDescription().isEmpty())
        return false;

    // By default, objects should be ignored so that the AX hierarchy is not
    // filled with unnecessary items.
    return true;
}

//
// Properties of static elements.
//

const AtomicString& AXRenderObject::accessKey() const
{
    Node* node = m_renderer->node();
    if (!node)
        return nullAtom;
    if (!node->isElementNode())
        return nullAtom;
    return toElement(node)->getAttribute(accesskeyAttr);
}

AccessibilityOrientation AXRenderObject::orientation() const
{
    const AtomicString& ariaOrientation = getAttribute(aria_orientationAttr);
    if (equalIgnoringCase(ariaOrientation, "horizontal"))
        return AccessibilityOrientationHorizontal;
    if (equalIgnoringCase(ariaOrientation, "vertical"))
        return AccessibilityOrientationVertical;

    return AXObject::orientation();
}

String AXRenderObject::text() const
{
    if (isPasswordField())
        return String();

    return AXNodeObject::text();
}

int AXRenderObject::textLength() const
{
    if (!isTextControl())
        return -1;

    if (isPasswordField())
        return -1; // need to return something distinct from 0

    return text().length();
}

KURL AXRenderObject::url() const
{
    if (isAnchor() && isHTMLAnchorElement(m_renderer->node())) {
        if (HTMLAnchorElement* anchor = toHTMLAnchorElement(anchorElement()))
            return anchor->href();
    }

    if (isWebArea())
        return m_renderer->document().url();

    if (isImage() && isHTMLImageElement(m_renderer->node()))
        return toHTMLImageElement(*m_renderer->node()).src();

    if (isInputImage())
        return toHTMLInputElement(m_renderer->node())->src();

    return KURL();
}

//
// Properties of interactive elements.
//

static String queryString(WebLocalizedString::Name name)
{
    return Locale::defaultLocale().queryString(name);
}

String AXRenderObject::actionVerb() const
{
    switch (roleValue()) {
    case ButtonRole:
    case ToggleButtonRole:
        return queryString(WebLocalizedString::AXButtonActionVerb);
    case TextFieldRole:
    case TextAreaRole:
        return queryString(WebLocalizedString::AXTextFieldActionVerb);
    case RadioButtonRole:
        return queryString(WebLocalizedString::AXRadioButtonActionVerb);
    case CheckBoxRole:
        return queryString(isChecked() ? WebLocalizedString::AXCheckedCheckBoxActionVerb : WebLocalizedString::AXUncheckedCheckBoxActionVerb);
    case LinkRole:
        return queryString(WebLocalizedString::AXLinkActionVerb);
    default:
        return emptyString();
    }
}

String AXRenderObject::stringValue() const
{
    if (!m_renderer)
        return String();

    if (isPasswordField())
        return String();

    RenderBoxModelObject* cssBox = renderBoxModelObject();

    if (ariaRoleAttribute() == StaticTextRole) {
        String staticText = text();
        if (!staticText.length())
            staticText = textUnderElement();
        return staticText;
    }

    if (m_renderer->isText())
        return textUnderElement();

    if (cssBox && cssBox->isMenuList()) {
        // RenderMenuList will go straight to the text() of its selected item.
        // This has to be overridden in the case where the selected item has an ARIA label.
        HTMLSelectElement* selectElement = toHTMLSelectElement(m_renderer->node());
        int selectedIndex = selectElement->selectedIndex();
        const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement->listItems();
        if (selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < listItems.size()) {
            const AtomicString& overriddenDescription = listItems[selectedIndex]->fastGetAttribute(aria_labelAttr);
            if (!overriddenDescription.isNull())
                return overriddenDescription;
        }
        return toRenderMenuList(m_renderer)->text();
    }

    if (m_renderer->isListMarker())
        return toRenderListMarker(m_renderer)->text();

    if (isWebArea()) {
        // FIXME: Why would a renderer exist when the Document isn't attached to a frame?
        if (m_renderer->frame())
            return String();

        ASSERT_NOT_REACHED();
    }

    if (isTextControl())
        return text();

    if (m_renderer->isFileUploadControl())
        return toRenderFileUploadControl(m_renderer)->fileTextValue();

    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return String();
}

//
// ARIA attributes.
//

AXObject* AXRenderObject::activeDescendant() const
{
    if (!m_renderer)
        return 0;

    if (m_renderer->node() && !m_renderer->node()->isElementNode())
        return 0;

    Element* element = toElement(m_renderer->node());
    if (!element)
        return 0;

    const AtomicString& activeDescendantAttrStr = element->getAttribute(aria_activedescendantAttr);
    if (activeDescendantAttrStr.isNull() || activeDescendantAttrStr.isEmpty())
        return 0;

    Element* target = element->treeScope().getElementById(activeDescendantAttrStr);
    if (!target)
        return 0;

    AXObject* obj = axObjectCache()->getOrCreate(target);

    // An activedescendant is only useful if it has a renderer, because that's what's needed to post the notification.
    if (obj && obj->isAXRenderObject())
        return obj;

    return 0;
}

void AXRenderObject::accessibilityChildrenFromAttribute(QualifiedName attr, AccessibilityChildrenVector& children) const
{
    Vector<Element*> elements;
    elementsFromAttribute(elements, attr);

    AXObjectCache* cache = axObjectCache();
    unsigned count = elements.size();
    for (unsigned k = 0; k < count; ++k) {
        Element* element = elements[k];
        AXObject* child = cache->getOrCreate(element);
        if (child)
            children.append(child);
    }
}

void AXRenderObject::ariaFlowToElements(AccessibilityChildrenVector& flowTo) const
{
    accessibilityChildrenFromAttribute(aria_flowtoAttr, flowTo);
}

void AXRenderObject::ariaControlsElements(AccessibilityChildrenVector& controls) const
{
    accessibilityChildrenFromAttribute(aria_controlsAttr, controls);
}

void AXRenderObject::ariaDescribedbyElements(AccessibilityChildrenVector& describedby) const
{
    accessibilityChildrenFromAttribute(aria_describedbyAttr, describedby);
}

void AXRenderObject::ariaLabelledbyElements(AccessibilityChildrenVector& labelledby) const
{
    accessibilityChildrenFromAttribute(aria_labelledbyAttr, labelledby);
}

void AXRenderObject::ariaOwnsElements(AccessibilityChildrenVector& owns) const
{
    accessibilityChildrenFromAttribute(aria_ownsAttr, owns);
}

bool AXRenderObject::ariaHasPopup() const
{
    return elementAttributeValue(aria_haspopupAttr);
}

bool AXRenderObject::ariaRoleHasPresentationalChildren() const
{
    switch (m_ariaRole) {
    case ButtonRole:
    case SliderRole:
    case ImageRole:
    case ProgressIndicatorRole:
    case SpinButtonRole:
    // case SeparatorRole:
        return true;
    default:
        return false;
    }
}

bool AXRenderObject::isPresentationalChildOfAriaRole() const
{
    // Walk the parent chain looking for a parent that has presentational children
    AXObject* parent;
    for (parent = parentObject(); parent && !parent->ariaRoleHasPresentationalChildren(); parent = parent->parentObject())
    { }

    return parent;
}

bool AXRenderObject::shouldFocusActiveDescendant() const
{
    switch (ariaRoleAttribute()) {
    case ComboBoxRole:
    case GridRole:
    case GroupRole:
    case ListBoxRole:
    case MenuRole:
    case MenuBarRole:
    case OutlineRole:
    case PopUpButtonRole:
    case ProgressIndicatorRole:
    case RadioGroupRole:
    case RowRole:
    case TabListRole:
    case ToolbarRole:
    case TreeRole:
    case TreeGridRole:
        return true;
    default:
        return false;
    }
}

bool AXRenderObject::supportsARIADragging() const
{
    const AtomicString& grabbed = getAttribute(aria_grabbedAttr);
    return equalIgnoringCase(grabbed, "true") || equalIgnoringCase(grabbed, "false");
}

bool AXRenderObject::supportsARIADropping() const
{
    const AtomicString& dropEffect = getAttribute(aria_dropeffectAttr);
    return !dropEffect.isEmpty();
}

bool AXRenderObject::supportsARIAFlowTo() const
{
    return !getAttribute(aria_flowtoAttr).isEmpty();
}

bool AXRenderObject::supportsARIAOwns() const
{
    if (!m_renderer)
        return false;
    const AtomicString& ariaOwns = getAttribute(aria_ownsAttr);

    return !ariaOwns.isEmpty();
}

//
// ARIA live-region features.
//

const AtomicString& AXRenderObject::ariaLiveRegionStatus() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, liveRegionStatusAssertive, ("assertive", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, liveRegionStatusPolite, ("polite", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, liveRegionStatusOff, ("off", AtomicString::ConstructFromLiteral));

    const AtomicString& liveRegionStatus = getAttribute(aria_liveAttr);
    // These roles have implicit live region status.
    if (liveRegionStatus.isEmpty()) {
        switch (roleValue()) {
        case AlertDialogRole:
        case AlertRole:
            return liveRegionStatusAssertive;
        case LogRole:
        case StatusRole:
            return liveRegionStatusPolite;
        case TimerRole:
        case MarqueeRole:
            return liveRegionStatusOff;
        default:
            break;
        }
    }

    return liveRegionStatus;
}

const AtomicString& AXRenderObject::ariaLiveRegionRelevant() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, defaultLiveRegionRelevant, ("additions text", AtomicString::ConstructFromLiteral));
    const AtomicString& relevant = getAttribute(aria_relevantAttr);

    // Default aria-relevant = "additions text".
    if (relevant.isEmpty())
        return defaultLiveRegionRelevant;

    return relevant;
}

bool AXRenderObject::ariaLiveRegionAtomic() const
{
    return elementAttributeValue(aria_atomicAttr);
}

bool AXRenderObject::ariaLiveRegionBusy() const
{
    return elementAttributeValue(aria_busyAttr);
}

//
// Accessibility Text.
//

String AXRenderObject::textUnderElement() const
{
    if (!m_renderer)
        return String();

    if (m_renderer->isFileUploadControl())
        return toRenderFileUploadControl(m_renderer)->buttonValue();

    if (m_renderer->isText())
        return toRenderText(m_renderer)->plainText();

    return AXNodeObject::textUnderElement();
}

//
// Accessibility Text - (To be deprecated).
//

String AXRenderObject::helpText() const
{
    if (!m_renderer)
        return String();

    const AtomicString& ariaHelp = getAttribute(aria_helpAttr);
    if (!ariaHelp.isEmpty())
        return ariaHelp;

    String describedBy = ariaDescribedByAttribute();
    if (!describedBy.isEmpty())
        return describedBy;

    String description = accessibilityDescription();
    for (RenderObject* curr = m_renderer; curr; curr = curr->parent()) {
        if (curr->node() && curr->node()->isHTMLElement()) {
            const AtomicString& summary = toElement(curr->node())->getAttribute(summaryAttr);
            if (!summary.isEmpty())
                return summary;

            // The title attribute should be used as help text unless it is already being used as descriptive text.
            const AtomicString& title = toElement(curr->node())->getAttribute(titleAttr);
            if (!title.isEmpty() && description != title)
                return title;
        }

        // Only take help text from an ancestor element if its a group or an unknown role. If help was
        // added to those kinds of elements, it is likely it was meant for a child element.
        AXObject* axObj = axObjectCache()->getOrCreate(curr);
        if (axObj) {
            AccessibilityRole role = axObj->roleValue();
            if (role != GroupRole && role != UnknownRole)
                break;
        }
    }

    return String();
}

//
// Position and size.
//

void AXRenderObject::checkCachedElementRect() const
{
    if (m_cachedElementRectDirty)
        return;

    if (!m_renderer)
        return;

    if (!m_renderer->isBox())
        return;

    bool dirty = false;
    RenderBox* box = toRenderBox(m_renderer);
    if (box->frameRect() != m_cachedFrameRect)
        dirty = true;

    if (box->canBeScrolledAndHasScrollableArea()) {
        ScrollableArea* scrollableArea = box->scrollableArea();
        if (scrollableArea && scrollableArea->scrollPosition() != m_cachedScrollPosition)
            dirty = true;
    }

    if (dirty)
        markCachedElementRectDirty();
}

void AXRenderObject::updateCachedElementRect() const
{
    if (!m_cachedElementRectDirty)
        return;

    if (!m_renderer)
        return;

    if (!m_renderer->isBox())
        return;

    RenderBox* box = toRenderBox(m_renderer);
    m_cachedFrameRect = box->frameRect();

    if (box->canBeScrolledAndHasScrollableArea()) {
        ScrollableArea* scrollableArea = box->scrollableArea();
        if (scrollableArea)
            m_cachedScrollPosition = scrollableArea->scrollPosition();
    }

    m_cachedElementRect = computeElementRect();
    m_cachedElementRectDirty = false;
}

void AXRenderObject::markCachedElementRectDirty() const
{
    if (m_cachedElementRectDirty)
        return;

    // Marks children recursively, if this element changed.
    m_cachedElementRectDirty = true;
    for (AXObject* child = firstChild(); child; child = child->nextSibling())
        child->markCachedElementRectDirty();
}

IntPoint AXRenderObject::clickPoint()
{
    // Headings are usually much wider than their textual content. If the mid point is used, often it can be wrong.
    if (isHeading() && children().size() == 1)
        return children()[0]->clickPoint();

    // use the default position unless this is an editable web area, in which case we use the selection bounds.
    if (!isWebArea() || isReadOnly())
        return AXObject::clickPoint();

    IntRect bounds = pixelSnappedIntRect(elementRect());
    return IntPoint(bounds.x() + (bounds.width() / 2), bounds.y() - (bounds.height() / 2));
}

//
// Hit testing.
//

AXObject* AXRenderObject::accessibilityHitTest(const IntPoint& point) const
{
    if (!m_renderer || !m_renderer->hasLayer())
        return 0;

    RenderLayer* layer = toRenderBox(m_renderer)->layer();

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
    HitTestResult hitTestResult = HitTestResult(point);
    layer->hitTest(request, hitTestResult);
    if (!hitTestResult.innerNode())
        return 0;
    Node* node = hitTestResult.innerNode()->deprecatedShadowAncestorNode();

    if (isHTMLAreaElement(node))
        return accessibilityImageMapHitTest(toHTMLAreaElement(node), point);

    if (isHTMLOptionElement(node))
        node = toHTMLOptionElement(*node).ownerSelectElement();

    RenderObject* obj = node->renderer();
    if (!obj)
        return 0;

    AXObject* result = obj->document().axObjectCache()->getOrCreate(obj);
    result->updateChildrenIfNecessary();

    // Allow the element to perform any hit-testing it might need to do to reach non-render children.
    result = result->elementAccessibilityHitTest(point);

    if (result && result->accessibilityIsIgnored()) {
        // If this element is the label of a control, a hit test should return the control.
        if (result->isAXRenderObject()) {
            AXObject* controlObject = toAXRenderObject(result)->correspondingControlForLabelElement();
            if (controlObject && !controlObject->exposesTitleUIElement())
                return controlObject;
        }

        result = result->parentObjectUnignored();
    }

    return result;
}

AXObject* AXRenderObject::elementAccessibilityHitTest(const IntPoint& point) const
{
    if (isSVGImage())
        return remoteSVGElementHitTest(point);

    return AXObject::elementAccessibilityHitTest(point);
}

//
// High-level accessibility tree access.
//

AXObject* AXRenderObject::parentObject() const
{
    if (!m_renderer)
        return 0;

    if (ariaRoleAttribute() == MenuBarRole)
        return axObjectCache()->getOrCreate(m_renderer->parent());

    // menuButton and its corresponding menu are DOM siblings, but Accessibility needs them to be parent/child
    if (ariaRoleAttribute() == MenuRole) {
        AXObject* parent = menuButtonForMenu();
        if (parent)
            return parent;
    }

    RenderObject* parentObj = renderParentObject();
    if (parentObj)
        return axObjectCache()->getOrCreate(parentObj);

    // WebArea's parent should be the scroll view containing it.
    if (isWebArea())
        return axObjectCache()->getOrCreate(m_renderer->frame()->view());

    return 0;
}

AXObject* AXRenderObject::parentObjectIfExists() const
{
    // WebArea's parent should be the scroll view containing it.
    if (isWebArea())
        return axObjectCache()->get(m_renderer->frame()->view());

    return axObjectCache()->get(renderParentObject());
}

//
// Low-level accessibility tree exploration, only for use within the accessibility module.
//

AXObject* AXRenderObject::firstChild() const
{
    if (!m_renderer)
        return 0;

    RenderObject* firstChild = firstChildConsideringContinuation(m_renderer);

    if (!firstChild)
        return 0;

    return axObjectCache()->getOrCreate(firstChild);
}

AXObject* AXRenderObject::nextSibling() const
{
    if (!m_renderer)
        return 0;

    RenderObject* nextSibling = 0;

    RenderInline* inlineContinuation;
    if (m_renderer->isRenderBlock() && (inlineContinuation = toRenderBlock(m_renderer)->inlineElementContinuation())) {
        // Case 1: node is a block and has an inline continuation. Next sibling is the inline continuation's first child.
        nextSibling = firstChildConsideringContinuation(inlineContinuation);
    } else if (m_renderer->isAnonymousBlock() && lastChildHasContinuation(m_renderer)) {
        // Case 2: Anonymous block parent of the start of a continuation - skip all the way to
        // after the parent of the end, since everything in between will be linked up via the continuation.
        RenderObject* lastParent = endOfContinuations(toRenderBlock(m_renderer)->lastChild())->parent();
        while (lastChildHasContinuation(lastParent))
            lastParent = endOfContinuations(lastParent->slowLastChild())->parent();
        nextSibling = lastParent->nextSibling();
    } else if (RenderObject* ns = m_renderer->nextSibling()) {
        // Case 3: node has an actual next sibling
        nextSibling = ns;
    } else if (isInlineWithContinuation(m_renderer)) {
        // Case 4: node is an inline with a continuation. Next sibling is the next sibling of the end
        // of the continuation chain.
        nextSibling = endOfContinuations(m_renderer)->nextSibling();
    } else if (isInlineWithContinuation(m_renderer->parent())) {
        // Case 5: node has no next sibling, and its parent is an inline with a continuation.
        RenderObject* continuation = toRenderInline(m_renderer->parent())->continuation();

        if (continuation->isRenderBlock()) {
            // Case 5a: continuation is a block - in this case the block itself is the next sibling.
            nextSibling = continuation;
        } else {
            // Case 5b: continuation is an inline - in this case the inline's first child is the next sibling.
            nextSibling = firstChildConsideringContinuation(continuation);
        }
    }

    if (!nextSibling)
        return 0;

    return axObjectCache()->getOrCreate(nextSibling);
}

void AXRenderObject::addChildren()
{
    // If the need to add more children in addition to existing children arises,
    // childrenChanged should have been called, leaving the object with no children.
    ASSERT(!m_haveChildren);

    m_haveChildren = true;

    if (!canHaveChildren())
        return;

    for (RefPtr<AXObject> obj = firstChild(); obj; obj = obj->nextSibling())
        addChild(obj.get());

    addHiddenChildren();
    addAttachmentChildren();
    addImageMapChildren();
    addTextFieldChildren();
    addCanvasChildren();
    addRemoteSVGChildren();
    addInlineTextBoxChildren();
}

bool AXRenderObject::canHaveChildren() const
{
    if (!m_renderer)
        return false;

    return AXNodeObject::canHaveChildren();
}

void AXRenderObject::updateChildrenIfNecessary()
{
    if (needsToUpdateChildren())
        clearChildren();

    AXObject::updateChildrenIfNecessary();
}

void AXRenderObject::clearChildren()
{
    AXObject::clearChildren();
    m_childrenDirty = false;
}

AXObject* AXRenderObject::observableObject() const
{
    // Find the object going up the parent chain that is used in accessibility to monitor certain notifications.
    for (RenderObject* renderer = m_renderer; renderer && renderer->node(); renderer = renderer->parent()) {
        if (renderObjectIsObservable(renderer))
            return axObjectCache()->getOrCreate(renderer);
    }

    return 0;
}

//
// Properties of the object's owning document or page.
//

double AXRenderObject::estimatedLoadingProgress() const
{
    if (!m_renderer)
        return 0;

    if (isLoaded())
        return 1.0;

    if (LocalFrame* frame = m_renderer->document().frame())
        return frame->loader().progress().estimatedProgress();
    return 0;
}

//
// DOM and Render tree access.
//

Node* AXRenderObject::node() const
{
    return m_renderer ? m_renderer->node() : 0;
}

Document* AXRenderObject::document() const
{
    if (!m_renderer)
        return 0;
    return &m_renderer->document();
}

FrameView* AXRenderObject::documentFrameView() const
{
    if (!m_renderer)
        return 0;

    // this is the RenderObject's Document's LocalFrame's FrameView
    return m_renderer->document().view();
}

Element* AXRenderObject::anchorElement() const
{
    if (!m_renderer)
        return 0;

    AXObjectCache* cache = axObjectCache();
    RenderObject* currRenderer;

    // Search up the render tree for a RenderObject with a DOM node. Defer to an earlier continuation, though.
    for (currRenderer = m_renderer; currRenderer && !currRenderer->node(); currRenderer = currRenderer->parent()) {
        if (currRenderer->isAnonymousBlock()) {
            RenderObject* continuation = toRenderBlock(currRenderer)->continuation();
            if (continuation)
                return cache->getOrCreate(continuation)->anchorElement();
        }
    }

    // bail if none found
    if (!currRenderer)
        return 0;

    // search up the DOM tree for an anchor element
    // NOTE: this assumes that any non-image with an anchor is an HTMLAnchorElement
    Node* node = currRenderer->node();
    for ( ; node; node = node->parentNode()) {
        if (isHTMLAnchorElement(*node) || (node->renderer() && cache->getOrCreate(node->renderer())->isAnchor()))
            return toElement(node);
    }

    return 0;
}

Widget* AXRenderObject::widgetForAttachmentView() const
{
    if (!isAttachment())
        return 0;
    return toRenderWidget(m_renderer)->widget();
}

//
// Selected text.
//

AXObject::PlainTextRange AXRenderObject::selectedTextRange() const
{
    if (!isTextControl())
        return PlainTextRange();

    if (isPasswordField())
        return PlainTextRange();

    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (isNativeTextControl() && ariaRole == UnknownRole && m_renderer->isTextControl()) {
        HTMLTextFormControlElement* textControl = toRenderTextControl(m_renderer)->textFormControlElement();
        return PlainTextRange(textControl->selectionStart(), textControl->selectionEnd() - textControl->selectionStart());
    }

    if (ariaRole == UnknownRole)
        return PlainTextRange();

    return ariaSelectedTextRange();
}

VisibleSelection AXRenderObject::selection() const
{
    return m_renderer->frame()->selection().selection();
}

//
// Modify or take an action on an object.
//

void AXRenderObject::setSelectedTextRange(const PlainTextRange& range)
{
    if (isNativeTextControl() && m_renderer->isTextControl()) {
        HTMLTextFormControlElement* textControl = toRenderTextControl(m_renderer)->textFormControlElement();
        textControl->setSelectionRange(range.start, range.start + range.length);
        return;
    }

    Document& document = m_renderer->document();
    LocalFrame* frame = document.frame();
    if (!frame)
        return;
    Node* node = m_renderer->node();
    frame->selection().setSelection(VisibleSelection(Position(node, range.start, Position::PositionIsOffsetInAnchor),
        Position(node, range.start + range.length, Position::PositionIsOffsetInAnchor), DOWNSTREAM));
}

void AXRenderObject::setValue(const String& string)
{
    if (!node() || !node()->isElementNode())
        return;
    if (!m_renderer || !m_renderer->isBoxModelObject())
        return;

    RenderBoxModelObject* renderer = toRenderBoxModelObject(m_renderer);
    if (renderer->isTextField() && isHTMLInputElement(*node()))
        toHTMLInputElement(*node()).setValue(string);
    else if (renderer->isTextArea() && isHTMLTextAreaElement(*node()))
        toHTMLTextAreaElement(*node()).setValue(string);
}

// FIXME: This function should use an IntSize to avoid the conversion below.
void AXRenderObject::scrollTo(const IntPoint& point) const
{
    if (!m_renderer || !m_renderer->isBox())
        return;

    RenderBox* box = toRenderBox(m_renderer);
    if (!box->canBeScrolledAndHasScrollableArea())
        return;

    box->scrollToOffset(IntSize(point.x(), point.y()));
}

//
// Notifications that this object may have changed.
//

void AXRenderObject::handleActiveDescendantChanged()
{
    Element* element = toElement(renderer()->node());
    if (!element)
        return;
    Document& doc = renderer()->document();
    if (!doc.frame()->selection().isFocusedAndActive() || doc.focusedElement() != element)
        return;
    AXRenderObject* activedescendant = toAXRenderObject(activeDescendant());

    if (activedescendant && shouldNotifyActiveDescendant())
        doc.axObjectCache()->postNotification(m_renderer, AXObjectCache::AXActiveDescendantChanged, true);
}

void AXRenderObject::handleAriaExpandedChanged()
{
    // Find if a parent of this object should handle aria-expanded changes.
    AXObject* containerParent = this->parentObject();
    while (containerParent) {
        bool foundParent = false;

        switch (containerParent->roleValue()) {
        case TreeRole:
        case TreeGridRole:
        case GridRole:
        case TableRole:
        case BrowserRole:
            foundParent = true;
            break;
        default:
            break;
        }

        if (foundParent)
            break;

        containerParent = containerParent->parentObject();
    }

    // Post that the row count changed.
    if (containerParent)
        axObjectCache()->postNotification(containerParent, document(), AXObjectCache::AXRowCountChanged, true);

    // Post that the specific row either collapsed or expanded.
    if (roleValue() == RowRole || roleValue() == TreeItemRole)
        axObjectCache()->postNotification(this, document(), isExpanded() ? AXObjectCache::AXRowExpanded : AXObjectCache::AXRowCollapsed, true);
}

void AXRenderObject::textChanged()
{
    if (!m_renderer)
        return;

    if (AXObjectCache::inlineTextBoxAccessibility() && roleValue() == StaticTextRole)
        childrenChanged();

    // Do this last - AXNodeObject::textChanged posts live region announcements,
    // and we should update the inline text boxes first.
    AXNodeObject::textChanged();
}

//
// Text metrics. Most of these should be deprecated, needs major cleanup.
//

// NOTE: Consider providing this utility method as AX API
int AXRenderObject::index(const VisiblePosition& position) const
{
    if (position.isNull() || !isTextControl())
        return -1;

    if (renderObjectContainsPosition(m_renderer, position.deepEquivalent()))
        return indexForVisiblePosition(position);

    return -1;
}

VisiblePosition AXRenderObject::visiblePositionForIndex(int index) const
{
    if (!m_renderer)
        return VisiblePosition();

    if (isNativeTextControl() && m_renderer->isTextControl())
        return toRenderTextControl(m_renderer)->textFormControlElement()->visiblePositionForIndex(index);

    if (!allowsTextRanges() && !m_renderer->isText())
        return VisiblePosition();

    Node* node = m_renderer->node();
    if (!node)
        return VisiblePosition();

    if (index <= 0)
        return VisiblePosition(firstPositionInOrBeforeNode(node), DOWNSTREAM);

    RefPtrWillBeRawPtr<Range> range = Range::create(m_renderer->document());
    range->selectNodeContents(node, IGNORE_EXCEPTION);
    CharacterIterator it(range.get());
    it.advance(index - 1);
    return VisiblePosition(Position(it.range()->endContainer(), it.range()->endOffset(), Position::PositionIsOffsetInAnch\
or), UPSTREAM);
}

int AXRenderObject::indexForVisiblePosition(const VisiblePosition& pos) const
{
    if (isNativeTextControl() && m_renderer->isTextControl()) {
        HTMLTextFormControlElement* textControl = toRenderTextControl(m_renderer)->textFormControlElement();
        return textControl->indexForVisiblePosition(pos);
    }

    if (!isTextControl())
        return 0;

    Node* node = m_renderer->node();
    if (!node)
        return 0;

    Position indexPosition = pos.deepEquivalent();
    if (indexPosition.isNull() || highestEditableRoot(indexPosition, HasEditableAXRole) != node)
        return 0;

    RefPtrWillBeRawPtr<Range> range = Range::create(m_renderer->document());
    range->setStart(node, 0, IGNORE_EXCEPTION);
    range->setEnd(indexPosition, IGNORE_EXCEPTION);

    return TextIterator::rangeLength(range.get());
}

void AXRenderObject::addInlineTextBoxChildren()
{
    if (!axObjectCache()->inlineTextBoxAccessibility())
        return;

    if (!renderer() || !renderer()->isText())
        return;

    if (renderer()->needsLayout()) {
        // If a RenderText needs layout, its inline text boxes are either
        // nonexistent or invalid, so defer until the layout happens and
        // the renderer calls AXObjectCache::inlineTextBoxesUpdated.
        return;
    }

    RenderText* renderText = toRenderText(renderer());
    for (RefPtr<AbstractInlineTextBox> box = renderText->firstAbstractInlineTextBox(); box.get(); box = box->nextInlineTextBox()) {
        AXObject* axObject = axObjectCache()->getOrCreate(box.get());
        if (!axObject->accessibilityIsIgnored())
            m_children.append(axObject);
    }
}

void AXRenderObject::lineBreaks(Vector<int>& lineBreaks) const
{
    if (!isTextControl())
        return;

    VisiblePosition visiblePos = visiblePositionForIndex(0);
    VisiblePosition savedVisiblePos = visiblePos;
    visiblePos = nextLinePosition(visiblePos, 0);
    while (!visiblePos.isNull() && visiblePos != savedVisiblePos) {
        lineBreaks.append(indexForVisiblePosition(visiblePos));
        savedVisiblePos = visiblePos;
        visiblePos = nextLinePosition(visiblePos, 0);
    }
}

//
// Private.
//

bool AXRenderObject::isAllowedChildOfTree() const
{
    // Determine if this is in a tree. If so, we apply special behavior to make it work like an AXOutline.
    AXObject* axObj = parentObject();
    bool isInTree = false;
    while (axObj) {
        if (axObj->isTree()) {
            isInTree = true;
            break;
        }
        axObj = axObj->parentObject();
    }

    // If the object is in a tree, only tree items should be exposed (and the children of tree items).
    if (isInTree) {
        AccessibilityRole role = roleValue();
        if (role != TreeItemRole && role != StaticTextRole)
            return false;
    }
    return true;
}

void AXRenderObject::ariaListboxSelectedChildren(AccessibilityChildrenVector& result)
{
    bool isMulti = isMultiSelectable();

    AccessibilityChildrenVector childObjects = children();
    unsigned childrenSize = childObjects.size();
    for (unsigned k = 0; k < childrenSize; ++k) {
        // Every child should have aria-role option, and if so, check for selected attribute/state.
        AXObject* child = childObjects[k].get();
        if (child->isSelected() && child->ariaRoleAttribute() == ListBoxOptionRole) {
            result.append(child);
            if (!isMulti)
                return;
        }
    }
}

AXObject::PlainTextRange AXRenderObject::ariaSelectedTextRange() const
{
    Node* node = m_renderer->node();
    if (!node)
        return PlainTextRange();

    VisibleSelection visibleSelection = selection();
    RefPtrWillBeRawPtr<Range> currentSelectionRange = visibleSelection.toNormalizedRange();
    if (!currentSelectionRange || !currentSelectionRange->intersectsNode(node, IGNORE_EXCEPTION))
        return PlainTextRange();

    int start = indexForVisiblePosition(visibleSelection.visibleStart());
    int end = indexForVisiblePosition(visibleSelection.visibleEnd());

    return PlainTextRange(start, end - start);
}

bool AXRenderObject::nodeIsTextControl(const Node* node) const
{
    if (!node)
        return false;

    const AXObject* axObjectForNode = axObjectCache()->getOrCreate(const_cast<Node*>(node));
    if (!axObjectForNode)
        return false;

    return axObjectForNode->isTextControl();
}

bool AXRenderObject::isTabItemSelected() const
{
    if (!isTabItem() || !m_renderer)
        return false;

    Node* node = m_renderer->node();
    if (!node || !node->isElementNode())
        return false;

    // The ARIA spec says a tab item can also be selected if it is aria-labeled by a tabpanel
    // that has keyboard focus inside of it, or if a tabpanel in its aria-controls list has KB
    // focus inside of it.
    AXObject* focusedElement = focusedUIElement();
    if (!focusedElement)
        return false;

    Vector<Element*> elements;
    elementsFromAttribute(elements, aria_controlsAttr);

    unsigned count = elements.size();
    for (unsigned k = 0; k < count; ++k) {
        Element* element = elements[k];
        AXObject* tabPanel = axObjectCache()->getOrCreate(element);

        // A tab item should only control tab panels.
        if (!tabPanel || tabPanel->roleValue() != TabPanelRole)
            continue;

        AXObject* checkFocusElement = focusedElement;
        // Check if the focused element is a descendant of the element controlled by the tab item.
        while (checkFocusElement) {
            if (tabPanel == checkFocusElement)
                return true;
            checkFocusElement = checkFocusElement->parentObject();
        }
    }

    return false;
}

AXObject* AXRenderObject::accessibilityImageMapHitTest(HTMLAreaElement* area, const IntPoint& point) const
{
    if (!area)
        return 0;

    AXObject* parent = axObjectCache()->getOrCreate(area->imageElement());
    if (!parent)
        return 0;

    AXObject::AccessibilityChildrenVector children = parent->children();
    unsigned count = children.size();
    for (unsigned k = 0; k < count; ++k) {
        if (children[k]->elementRect().contains(point))
            return children[k].get();
    }

    return 0;
}

bool AXRenderObject::renderObjectIsObservable(RenderObject* renderer) const
{
    // AX clients will listen for AXValueChange on a text control.
    if (renderer->isTextControl())
        return true;

    // AX clients will listen for AXSelectedChildrenChanged on listboxes.
    Node* node = renderer->node();
    if (nodeHasRole(node, "listbox") || (renderer->isBoxModelObject() && toRenderBoxModelObject(renderer)->isListBox()))
        return true;

    // Textboxes should send out notifications.
    if (nodeHasRole(node, "textbox"))
        return true;

    return false;
}

RenderObject* AXRenderObject::renderParentObject() const
{
    if (!m_renderer)
        return 0;

    RenderObject* parent = m_renderer->parent();

    RenderObject* startOfConts = 0;
    RenderObject* firstChild = 0;
    if (m_renderer->isRenderBlock() && (startOfConts = startOfContinuations(m_renderer))) {
        // Case 1: node is a block and is an inline's continuation. Parent
        // is the start of the continuation chain.
        parent = startOfConts;
    } else if (parent && parent->isRenderInline() && (startOfConts = startOfContinuations(parent))) {
        // Case 2: node's parent is an inline which is some node's continuation; parent is
        // the earliest node in the continuation chain.
        parent = startOfConts;
    } else if (parent && (firstChild = parent->slowFirstChild()) && firstChild->node()) {
        // Case 3: The first sibling is the beginning of a continuation chain. Find the origin of that continuation.
        // Get the node's renderer and follow that continuation chain until the first child is found.
        RenderObject* nodeRenderFirstChild = firstChild->node()->renderer();
        while (nodeRenderFirstChild != firstChild) {
            for (RenderObject* contsTest = nodeRenderFirstChild; contsTest; contsTest = nextContinuation(contsTest)) {
                if (contsTest == firstChild) {
                    parent = nodeRenderFirstChild->parent();
                    break;
                }
            }
            RenderObject* newFirstChild = parent->slowFirstChild();
            if (firstChild == newFirstChild)
                break;
            firstChild = newFirstChild;
            if (!firstChild->node())
                break;
            nodeRenderFirstChild = firstChild->node()->renderer();
        }
    }

    return parent;
}

bool AXRenderObject::isDescendantOfElementType(const QualifiedName& tagName) const
{
    for (RenderObject* parent = m_renderer->parent(); parent; parent = parent->parent()) {
        if (parent->node() && parent->node()->hasTagName(tagName))
            return true;
    }
    return false;
}

bool AXRenderObject::isSVGImage() const
{
    return remoteSVGRootElement();
}

void AXRenderObject::detachRemoteSVGRoot()
{
    if (AXSVGRoot* root = remoteSVGRootElement())
        root->setParent(0);
}

AXSVGRoot* AXRenderObject::remoteSVGRootElement() const
{
    if (!m_renderer || !m_renderer->isRenderImage())
        return 0;

    ImageResource* cachedImage = toRenderImage(m_renderer)->cachedImage();
    if (!cachedImage)
        return 0;

    Image* image = cachedImage->image();
    if (!image || !image->isSVGImage())
        return 0;

    FrameView* frameView = toSVGImage(image)->frameView();
    if (!frameView)
        return 0;
    Document* doc = frameView->frame().document();
    if (!doc || !doc->isSVGDocument())
        return 0;

    SVGSVGElement* rootElement = doc->accessSVGExtensions().rootElement();
    if (!rootElement)
        return 0;
    RenderObject* rendererRoot = rootElement->renderer();
    if (!rendererRoot)
        return 0;

    AXObject* rootSVGObject = doc->axObjectCache()->getOrCreate(rendererRoot);

    // In order to connect the AX hierarchy from the SVG root element from the loaded resource
    // the parent must be set, because there's no other way to get back to who created the image.
    ASSERT(rootSVGObject && rootSVGObject->isAXSVGRoot());
    if (!rootSVGObject->isAXSVGRoot())
        return 0;

    return toAXSVGRoot(rootSVGObject);
}

AXObject* AXRenderObject::remoteSVGElementHitTest(const IntPoint& point) const
{
    AXObject* remote = remoteSVGRootElement();
    if (!remote)
        return 0;

    IntSize offset = point - roundedIntPoint(elementRect().location());
    return remote->accessibilityHitTest(IntPoint(offset));
}

// The boundingBox for elements within the remote SVG element needs to be offset by its position
// within the parent page, otherwise they are in relative coordinates only.
void AXRenderObject::offsetBoundingBoxForRemoteSVGElement(LayoutRect& rect) const
{
    for (AXObject* parent = parentObject(); parent; parent = parent->parentObject()) {
        if (parent->isAXSVGRoot()) {
            rect.moveBy(parent->parentObject()->elementRect().location());
            break;
        }
    }
}

// Hidden children are those that are not rendered or visible, but are specifically marked as aria-hidden=false,
// meaning that they should be exposed to the AX hierarchy.
void AXRenderObject::addHiddenChildren()
{
    Node* node = this->node();
    if (!node)
        return;

    // First do a quick run through to determine if we have any hidden nodes (most often we will not).
    // If we do have hidden nodes, we need to determine where to insert them so they match DOM order as close as possible.
    bool shouldInsertHiddenNodes = false;
    for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
        if (!child->renderer() && isNodeAriaVisible(child)) {
            shouldInsertHiddenNodes = true;
            break;
        }
    }

    if (!shouldInsertHiddenNodes)
        return;

    // Iterate through all of the children, including those that may have already been added, and
    // try to insert hidden nodes in the correct place in the DOM order.
    unsigned insertionIndex = 0;
    for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
        if (child->renderer()) {
            // Find out where the last render sibling is located within m_children.
            AXObject* childObject = axObjectCache()->get(child->renderer());
            if (childObject && childObject->accessibilityIsIgnored()) {
                AccessibilityChildrenVector children = childObject->children();
                if (children.size())
                    childObject = children.last().get();
                else
                    childObject = 0;
            }

            if (childObject)
                insertionIndex = m_children.find(childObject) + 1;
            continue;
        }

        if (!isNodeAriaVisible(child))
            continue;

        unsigned previousSize = m_children.size();
        if (insertionIndex > previousSize)
            insertionIndex = previousSize;

        insertChild(axObjectCache()->getOrCreate(child), insertionIndex);
        insertionIndex += (m_children.size() - previousSize);
    }
}

void AXRenderObject::addTextFieldChildren()
{
    Node* node = this->node();
    if (!isHTMLInputElement(node))
        return;

    HTMLInputElement& input = toHTMLInputElement(*node);
    Element* spinButtonElement = input.userAgentShadowRoot()->getElementById(ShadowElementNames::spinButton());
    if (!spinButtonElement || !spinButtonElement->isSpinButtonElement())
        return;

    AXSpinButton* axSpinButton = toAXSpinButton(axObjectCache()->getOrCreate(SpinButtonRole));
    axSpinButton->setSpinButtonElement(toSpinButtonElement(spinButtonElement));
    axSpinButton->setParent(this);
    m_children.append(axSpinButton);
}

void AXRenderObject::addImageMapChildren()
{
    RenderBoxModelObject* cssBox = renderBoxModelObject();
    if (!cssBox || !cssBox->isRenderImage())
        return;

    HTMLMapElement* map = toRenderImage(cssBox)->imageMap();
    if (!map)
        return;

    for (HTMLAreaElement* area = Traversal<HTMLAreaElement>::firstWithin(*map); area; area = Traversal<HTMLAreaElement>::next(*area, map)) {
        // add an <area> element for this child if it has a link
        if (area->isLink()) {
            AXImageMapLink* areaObject = toAXImageMapLink(axObjectCache()->getOrCreate(ImageMapLinkRole));
            areaObject->setHTMLAreaElement(area);
            areaObject->setHTMLMapElement(map);
            areaObject->setParent(this);
            if (!areaObject->accessibilityIsIgnored())
                m_children.append(areaObject);
            else
                axObjectCache()->remove(areaObject->axObjectID());
        }
    }
}

void AXRenderObject::addCanvasChildren()
{
    if (!isHTMLCanvasElement(node()))
        return;

    // If it's a canvas, it won't have rendered children, but it might have accessible fallback content.
    // Clear m_haveChildren because AXNodeObject::addChildren will expect it to be false.
    ASSERT(!m_children.size());
    m_haveChildren = false;
    AXNodeObject::addChildren();
}

void AXRenderObject::addAttachmentChildren()
{
    if (!isAttachment())
        return;

    // FrameView's need to be inserted into the AX hierarchy when encountered.
    Widget* widget = widgetForAttachmentView();
    if (!widget || !widget->isFrameView())
        return;

    AXObject* axWidget = axObjectCache()->getOrCreate(widget);
    if (!axWidget->accessibilityIsIgnored())
        m_children.append(axWidget);
}

void AXRenderObject::addRemoteSVGChildren()
{
    AXSVGRoot* root = remoteSVGRootElement();
    if (!root)
        return;

    root->setParent(this);

    if (root->accessibilityIsIgnored()) {
        AccessibilityChildrenVector children = root->children();
        unsigned length = children.size();
        for (unsigned i = 0; i < length; ++i)
            m_children.append(children[i]);
    } else {
        m_children.append(root);
    }
}

void AXRenderObject::ariaSelectedRows(AccessibilityChildrenVector& result)
{
    // Get all the rows.
    AccessibilityChildrenVector allRows;
    if (isTree())
        ariaTreeRows(allRows);
    else if (isAXTable() && toAXTable(this)->supportsSelectedRows())
        allRows = toAXTable(this)->rows();

    // Determine which rows are selected.
    bool isMulti = isMultiSelectable();

    // Prefer active descendant over aria-selected.
    AXObject* activeDesc = activeDescendant();
    if (activeDesc && (activeDesc->isTreeItem() || activeDesc->isTableRow())) {
        result.append(activeDesc);
        if (!isMulti)
            return;
    }

    unsigned count = allRows.size();
    for (unsigned k = 0; k < count; ++k) {
        if (allRows[k]->isSelected()) {
            result.append(allRows[k]);
            if (!isMulti)
                break;
        }
    }
}

bool AXRenderObject::elementAttributeValue(const QualifiedName& attributeName) const
{
    if (!m_renderer)
        return false;

    return equalIgnoringCase(getAttribute(attributeName), "true");
}

bool AXRenderObject::inheritsPresentationalRole() const
{
    // ARIA states if an item can get focus, it should not be presentational.
    if (canSetFocusAttribute())
        return false;

    // ARIA spec says that when a parent object is presentational, and it has required child elements,
    // those child elements are also presentational. For example, <li> becomes presentational from <ul>.
    // http://www.w3.org/WAI/PF/aria/complete#presentation
    if (roleValue() != ListItemRole && roleValue() != ListMarkerRole)
        return false;

    AXObject* parent = parentObject();
    if (!parent->isAXRenderObject())
        return false;

    Node* elementNode = toAXRenderObject(parent)->node();
    if (!elementNode || !elementNode->isElementNode())
        return false;

    QualifiedName tagName = toElement(elementNode)->tagQName();
    if (tagName == ulTag || tagName == olTag || tagName == dlTag)
        return parent->roleValue() == PresentationalRole;

    return false;
}

LayoutRect AXRenderObject::computeElementRect() const
{
    RenderObject* obj = m_renderer;

    if (!obj)
        return LayoutRect();

    if (obj->node()) // If we are a continuation, we want to make sure to use the primary renderer.
        obj = obj->node()->renderer();

    // absoluteFocusRingQuads will query the hierarchy below this element, which for large webpages can be very slow.
    // For a web area, which will have the most elements of any element, absoluteQuads should be used.
    // We should also use absoluteQuads for SVG elements, otherwise transforms won't be applied.
    Vector<FloatQuad> quads;

    if (obj->isText())
        toRenderText(obj)->absoluteQuads(quads, 0, RenderText::ClipToEllipsis);
    else if (isWebArea() || obj->isSVGRoot())
        obj->absoluteQuads(quads);
    else
        obj->absoluteFocusRingQuads(quads);

    LayoutRect result = boundingBoxForQuads(obj, quads);

    Document* document = this->document();
    if (document && document->isSVGDocument())
        offsetBoundingBoxForRemoteSVGElement(result);

    // The size of the web area should be the content size, not the clipped size.
    if (isWebArea() && obj->frame()->view())
        result.setSize(obj->frame()->view()->contentsSize());

    // Checkboxes and radio buttons include their label as part of their rect.
    if (isCheckboxOrRadio()) {
        HTMLLabelElement* label = labelForElement(toElement(m_renderer->node()));
        if (label && label->renderer()) {
            LayoutRect labelRect = axObjectCache()->getOrCreate(label)->elementRect();
            result.unite(labelRect);
        }
    }

    return result;
}

} // namespace WebCore
