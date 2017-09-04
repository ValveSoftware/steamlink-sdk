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

#include "modules/accessibility/AXLayoutObject.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/InputTypeNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Range.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/RenderedPosition.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/VisibleUnits.h"
#include "core/editing/iterators/CharacterIterator.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/frame/FrameOwner.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/LabelsNodeList.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutFileUploadControl.h"
#include "core/layout/LayoutHTMLCanvas.h"
#include "core/layout/LayoutImage.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutListMarker.h"
#include "core/layout/LayoutMenuList.h"
#include "core/layout/LayoutTextControl.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutAPIShim.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/loader/ProgressTracker.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/graphics/SVGImage.h"
#include "modules/accessibility/AXImageMapLink.h"
#include "modules/accessibility/AXInlineTextBox.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "modules/accessibility/AXSVGRoot.h"
#include "modules/accessibility/AXSpinButton.h"
#include "modules/accessibility/AXTable.h"
#include "platform/fonts/FontTraits.h"
#include "platform/geometry/TransformState.h"
#include "platform/text/PlatformLocale.h"
#include "platform/text/TextDirection.h"
#include "wtf/StdLibExtras.h"

using blink::WebLocalizedString;

namespace blink {

using namespace HTMLNames;

static inline LayoutObject* firstChildInContinuation(
    const LayoutInline& layoutObject) {
  LayoutBoxModelObject* r = layoutObject.continuation();

  while (r) {
    if (r->isLayoutBlock())
      return r;
    if (LayoutObject* child = r->slowFirstChild())
      return child;
    r = toLayoutInline(r)->continuation();
  }

  return 0;
}

static inline bool isInlineWithContinuation(LayoutObject* object) {
  if (!object->isBoxModelObject())
    return false;

  LayoutBoxModelObject* layoutObject = toLayoutBoxModelObject(object);
  if (!layoutObject->isLayoutInline())
    return false;

  return toLayoutInline(layoutObject)->continuation();
}

static inline LayoutObject* firstChildConsideringContinuation(
    LayoutObject* layoutObject) {
  LayoutObject* firstChild = layoutObject->slowFirstChild();

  if (!firstChild && isInlineWithContinuation(layoutObject))
    firstChild = firstChildInContinuation(toLayoutInline(*layoutObject));

  return firstChild;
}

static inline LayoutInline* startOfContinuations(LayoutObject* r) {
  if (r->isInlineElementContinuation()) {
    return toLayoutInline(r->node()->layoutObject());
  }

  // Blocks with a previous continuation always have a next continuation
  if (r->isLayoutBlockFlow() &&
      toLayoutBlockFlow(r)->inlineElementContinuation())
    return toLayoutInline(toLayoutBlockFlow(r)
                              ->inlineElementContinuation()
                              ->node()
                              ->layoutObject());

  return 0;
}

static inline LayoutObject* endOfContinuations(LayoutObject* layoutObject) {
  LayoutObject* prev = layoutObject;
  LayoutObject* cur = layoutObject;

  if (!cur->isLayoutInline() && !cur->isLayoutBlockFlow())
    return layoutObject;

  while (cur) {
    prev = cur;
    if (cur->isLayoutInline()) {
      cur = toLayoutInline(cur)->inlineElementContinuation();
      ASSERT(cur || !toLayoutInline(prev)->continuation());
    } else {
      cur = toLayoutBlockFlow(cur)->inlineElementContinuation();
    }
  }

  return prev;
}

static inline bool lastChildHasContinuation(LayoutObject* layoutObject) {
  LayoutObject* lastChild = layoutObject->slowLastChild();
  return lastChild && isInlineWithContinuation(lastChild);
}

static LayoutBoxModelObject* nextContinuation(LayoutObject* layoutObject) {
  ASSERT(layoutObject);
  if (layoutObject->isLayoutInline() && !layoutObject->isAtomicInlineLevel())
    return toLayoutInline(layoutObject)->continuation();
  if (layoutObject->isLayoutBlockFlow())
    return toLayoutBlockFlow(layoutObject)->inlineElementContinuation();
  return 0;
}

AXLayoutObject::AXLayoutObject(LayoutObject* layoutObject,
                               AXObjectCacheImpl& axObjectCache)
    : AXNodeObject(layoutObject->node(), axObjectCache),
      m_layoutObject(layoutObject) {
#if ENABLE(ASSERT)
  m_layoutObject->setHasAXObject(true);
#endif
}

AXLayoutObject* AXLayoutObject::create(LayoutObject* layoutObject,
                                       AXObjectCacheImpl& axObjectCache) {
  return new AXLayoutObject(layoutObject, axObjectCache);
}

AXLayoutObject::~AXLayoutObject() {
  ASSERT(isDetached());
}

LayoutBoxModelObject* AXLayoutObject::getLayoutBoxModelObject() const {
  if (!m_layoutObject || !m_layoutObject->isBoxModelObject())
    return 0;
  return toLayoutBoxModelObject(m_layoutObject);
}

ScrollableArea* AXLayoutObject::getScrollableAreaIfScrollable() const {
  if (isWebArea())
    return documentFrameView();

  if (!m_layoutObject || !m_layoutObject->isBox())
    return 0;

  LayoutBox* box = toLayoutBox(m_layoutObject);
  if (!box->canBeScrolledAndHasScrollableArea())
    return 0;

  return box->getScrollableArea();
}

static bool isImageOrAltText(LayoutBoxModelObject* box, Node* node) {
  if (box && box->isImage())
    return true;
  if (isHTMLImageElement(node))
    return true;
  if (isHTMLInputElement(node) &&
      toHTMLInputElement(node)->hasFallbackContent())
    return true;
  return false;
}

AccessibilityRole AXLayoutObject::nativeAccessibilityRoleIgnoringAria() const {
  Node* node = m_layoutObject->node();
  LayoutBoxModelObject* cssBox = getLayoutBoxModelObject();

  if ((cssBox && cssBox->isListItem()) || isHTMLLIElement(node))
    return ListItemRole;
  if (m_layoutObject->isListMarker())
    return ListMarkerRole;
  if (m_layoutObject->isBR())
    return LineBreakRole;
  if (m_layoutObject->isText())
    return StaticTextRole;
  if (cssBox && isImageOrAltText(cssBox, node)) {
    if (node && node->isLink())
      return ImageMapRole;
    if (isHTMLInputElement(node))
      return ariaHasPopup() ? PopUpButtonRole : ButtonRole;
    if (isSVGImage())
      return SVGRootRole;
    return ImageRole;
  }
  // Note: if JavaScript is disabled, the layoutObject won't be a
  // LayoutHTMLCanvas.
  if (isHTMLCanvasElement(node) && m_layoutObject->isCanvas())
    return CanvasRole;

  if (cssBox && cssBox->isLayoutView())
    return WebAreaRole;

  if (m_layoutObject->isSVGImage())
    return ImageRole;
  if (m_layoutObject->isSVGRoot())
    return SVGRootRole;

  // Table sections should be ignored.
  if (m_layoutObject->isTableSection())
    return IgnoredRole;

  if (m_layoutObject->isHR())
    return SplitterRole;

  return AXNodeObject::nativeAccessibilityRoleIgnoringAria();
}

AccessibilityRole AXLayoutObject::determineAccessibilityRole() {
  if (!m_layoutObject)
    return UnknownRole;

  if ((m_ariaRole = determineAriaRoleAttribute()) != UnknownRole)
    return m_ariaRole;

  AccessibilityRole role = nativeAccessibilityRoleIgnoringAria();
  if (role != UnknownRole)
    return role;

  if (m_layoutObject->isLayoutBlockFlow())
    return GroupRole;

  // If the element does not have role, but it has ARIA attributes,
  // accessibility should fallback to exposing it as a group.
  if (supportsARIAAttributes())
    return GroupRole;

  return UnknownRole;
}

void AXLayoutObject::init() {
  AXNodeObject::init();
}

void AXLayoutObject::detach() {
  AXNodeObject::detach();

  detachRemoteSVGRoot();

#if ENABLE(ASSERT)
  if (m_layoutObject)
    m_layoutObject->setHasAXObject(false);
#endif
  m_layoutObject = 0;
}

//
// Check object role or purpose.
//

static bool isLinkable(const AXObject& object) {
  if (!object.getLayoutObject())
    return false;

  // See https://wiki.mozilla.org/Accessibility/AT-Windows-API for the elements
  // Mozilla considers linkable.
  return object.isLink() || object.isImage() ||
         object.getLayoutObject()->isText();
}

// Requires layoutObject to be present because it relies on style
// user-modify. Don't move this logic to AXNodeObject.
bool AXLayoutObject::isEditable() const {
  if (getLayoutObject() && getLayoutObject()->isTextControl())
    return true;

  if (getNode() && hasEditableStyle(*getNode()))
    return true;

  if (isWebArea()) {
    Document& document = getLayoutObject()->document();
    HTMLElement* body = document.body();
    if (body && hasEditableStyle(*body))
      return true;

    return hasEditableStyle(document);
  }

  return AXNodeObject::isEditable();
}

// Requires layoutObject to be present because it relies on style
// user-modify. Don't move this logic to AXNodeObject.
bool AXLayoutObject::isRichlyEditable() const {
  if (getNode() && hasRichlyEditableStyle(*getNode()))
    return true;

  if (isWebArea()) {
    Document& document = m_layoutObject->document();
    HTMLElement* body = document.body();
    if (body && hasRichlyEditableStyle(*body))
      return true;

    return hasRichlyEditableStyle(document);
  }

  return AXNodeObject::isRichlyEditable();
}

bool AXLayoutObject::isLinked() const {
  if (!isLinkable(*this))
    return false;

  Element* anchor = anchorElement();
  if (!isHTMLAnchorElement(anchor))
    return false;

  return !toHTMLAnchorElement(*anchor).href().isEmpty();
}

bool AXLayoutObject::isLoaded() const {
  return !m_layoutObject->document().parser();
}

bool AXLayoutObject::isOffScreen() const {
  ASSERT(m_layoutObject);
  IntRect contentRect =
      pixelSnappedIntRect(m_layoutObject->absoluteVisualRect());
  FrameView* view = m_layoutObject->frame()->view();
  IntRect viewRect = view->visibleContentRect();
  viewRect.intersect(contentRect);
  return viewRect.isEmpty();
}

bool AXLayoutObject::isReadOnly() const {
  ASSERT(m_layoutObject);

  if (isWebArea()) {
    Document& document = m_layoutObject->document();
    HTMLElement* body = document.body();
    if (body && hasEditableStyle(*body))
      return false;

    return !hasEditableStyle(document);
  }

  return AXNodeObject::isReadOnly();
}

bool AXLayoutObject::isVisited() const {
  // FIXME: Is it a privacy violation to expose visited information to
  // accessibility APIs?
  return m_layoutObject->style()->isLink() &&
         m_layoutObject->style()->insideLink() == InsideVisitedLink;
}

//
// Check object state.
//

bool AXLayoutObject::isFocused() const {
  if (!getDocument())
    return false;

  Element* focusedElement = getDocument()->focusedElement();
  if (!focusedElement)
    return false;
  AXObject* focusedObject = axObjectCache().getOrCreate(focusedElement);
  if (!focusedObject || !focusedObject->isAXLayoutObject())
    return false;

  // A web area is represented by the Document node in the DOM tree, which isn't
  // focusable.  Check instead if the frame's selection controller is focused
  if (focusedObject == this ||
      (roleValue() == WebAreaRole &&
       getDocument()->frame()->selection().isFocusedAndActive()))
    return true;

  return false;
}

bool AXLayoutObject::isSelected() const {
  if (!getLayoutObject() || !getNode())
    return false;

  const AtomicString& ariaSelected = getAttribute(aria_selectedAttr);
  if (equalIgnoringCase(ariaSelected, "true"))
    return true;

  AXObject* focusedObject = axObjectCache().focusedObject();
  if (ariaRoleAttribute() == ListBoxOptionRole && focusedObject &&
      focusedObject->activeDescendant() == this) {
    return true;
  }

  if (isTabItem() && isTabItemSelected())
    return true;

  return false;
}

//
// Whether objects are ignored, i.e. not included in the tree.
//

AXObjectInclusion AXLayoutObject::defaultObjectInclusion(
    IgnoredReasons* ignoredReasons) const {
  // The following cases can apply to any element that's a subclass of
  // AXLayoutObject.

  if (!m_layoutObject) {
    if (ignoredReasons)
      ignoredReasons->append(IgnoredReason(AXNotRendered));
    return IgnoreObject;
  }

  if (m_layoutObject->style()->visibility() != EVisibility::Visible) {
    // aria-hidden is meant to override visibility as the determinant in AX
    // hierarchy inclusion.
    if (equalIgnoringCase(getAttribute(aria_hiddenAttr), "false"))
      return DefaultBehavior;

    if (ignoredReasons)
      ignoredReasons->append(IgnoredReason(AXNotVisible));
    return IgnoreObject;
  }

  return AXObject::defaultObjectInclusion(ignoredReasons);
}

bool AXLayoutObject::computeAccessibilityIsIgnored(
    IgnoredReasons* ignoredReasons) const {
#if ENABLE(ASSERT)
  ASSERT(m_initialized);
#endif

  if (!m_layoutObject)
    return true;

  // Check first if any of the common reasons cause this element to be ignored.
  // Then process other use cases that need to be applied to all the various
  // roles that AXLayoutObjects take on.
  AXObjectInclusion decision = defaultObjectInclusion(ignoredReasons);
  if (decision == IncludeObject)
    return false;
  if (decision == IgnoreObject)
    return true;

  if (m_layoutObject->isAnonymousBlock())
    return true;

  // If this element is within a parent that cannot have children, it should not
  // be exposed.
  if (isDescendantOfLeafNode()) {
    if (ignoredReasons)
      ignoredReasons->append(
          IgnoredReason(AXAncestorIsLeafNode, leafNodeAncestor()));
    return true;
  }

  if (roleValue() == IgnoredRole) {
    if (ignoredReasons)
      ignoredReasons->append(IgnoredReason(AXUninteresting));
    return true;
  }

  if (hasInheritedPresentationalRole()) {
    if (ignoredReasons) {
      const AXObject* inheritsFrom = inheritsPresentationalRoleFrom();
      if (inheritsFrom == this)
        ignoredReasons->append(IgnoredReason(AXPresentationalRole));
      else
        ignoredReasons->append(
            IgnoredReason(AXInheritsPresentation, inheritsFrom));
    }
    return true;
  }

  // An ARIA tree can only have tree items and static text as children.
  if (AXObject* treeAncestor = treeAncestorDisallowingChild()) {
    if (ignoredReasons)
      ignoredReasons->append(
          IgnoredReason(AXAncestorDisallowsChild, treeAncestor));
    return true;
  }

  // A LayoutPart is an iframe element or embedded object element or something
  // like that. We don't want to ignore those.
  if (m_layoutObject->isLayoutPart())
    return false;

  // Make sure renderers with layers stay in the tree.
  if (getLayoutObject() && getLayoutObject()->hasLayer() && getNode() &&
      getNode()->hasChildren())
    return false;

  // Find out if this element is inside of a label element.  If so, it may be
  // ignored because it's the label for a checkbox or radio button.
  AXObject* controlObject = correspondingControlForLabelElement();
  if (controlObject && controlObject->isCheckboxOrRadio() &&
      controlObject->nameFromLabelElement()) {
    if (ignoredReasons) {
      HTMLLabelElement* label = labelElementContainer();
      if (label && label != getNode()) {
        AXObject* labelAXObject = axObjectCache().getOrCreate(label);
        ignoredReasons->append(IgnoredReason(AXLabelContainer, labelAXObject));
      }

      ignoredReasons->append(IgnoredReason(AXLabelFor, controlObject));
    }
    return true;
  }

  if (m_layoutObject->isBR())
    return false;

  if (m_layoutObject->isText()) {
    // Static text beneath MenuItems and MenuButtons are just reported along
    // with the menu item, so it's ignored on an individual level.
    AXObject* parent = parentObjectUnignored();
    if (parent && (parent->ariaRoleAttribute() == MenuItemRole ||
                   parent->ariaRoleAttribute() == MenuButtonRole)) {
      if (ignoredReasons)
        ignoredReasons->append(
            IgnoredReason(AXStaticTextUsedAsNameFor, parent));
      return true;
    }
    LayoutText* layoutText = toLayoutText(m_layoutObject);
    if (!layoutText->hasTextBoxes()) {
      if (ignoredReasons)
        ignoredReasons->append(IgnoredReason(AXEmptyText));
      return true;
    }

    // Don't ignore static text in editable text controls.
    for (AXObject* parent = parentObject(); parent;
         parent = parent->parentObject()) {
      if (parent->roleValue() == TextFieldRole)
        return false;
    }

    // Text elements that are just empty whitespace should not be returned.
    // FIXME(dmazzoni): we probably shouldn't ignore this if the style is 'pre',
    // or similar...
    if (layoutText->text().impl()->containsOnlyWhitespace()) {
      if (ignoredReasons)
        ignoredReasons->append(IgnoredReason(AXEmptyText));
      return true;
    }
    return false;
  }

  if (isHeading())
    return false;

  if (isLandmarkRelated())
    return false;

  // Header and footer tags may also be exposed as landmark roles but not
  // always.
  if (getNode() &&
      (getNode()->hasTagName(headerTag) || getNode()->hasTagName(footerTag)))
    return false;

  if (isLink())
    return false;

  // all controls are accessible
  if (isControl())
    return false;

  if (ariaRoleAttribute() != UnknownRole)
    return false;

  // don't ignore labels, because they serve as TitleUIElements
  Node* node = m_layoutObject->node();
  if (isHTMLLabelElement(node))
    return false;

  // Anything that is content editable should not be ignored.
  // However, one cannot just call node->hasEditableStyle() since that will ask
  // if its parents are also editable. Only the top level content editable
  // region should be exposed.
  if (hasContentEditableAttributeSet())
    return false;

  if (roleValue() == AbbrRole)
    return false;

  // List items play an important role in defining the structure of lists. They
  // should not be ignored.
  if (roleValue() == ListItemRole)
    return false;

  if (roleValue() == BlockquoteRole)
    return false;

  if (roleValue() == DialogRole)
    return false;

  if (roleValue() == FigcaptionRole)
    return false;

  if (roleValue() == FigureRole)
    return false;

  if (roleValue() == DetailsRole)
    return false;

  if (roleValue() == MarkRole)
    return false;

  if (roleValue() == MathRole)
    return false;

  if (roleValue() == MeterRole)
    return false;

  if (roleValue() == RubyRole)
    return false;

  if (roleValue() == SplitterRole)
    return false;

  if (roleValue() == TimeRole)
    return false;

  // if this element has aria attributes on it, it should not be ignored.
  if (supportsARIAAttributes())
    return false;

  // <span> tags are inline tags and not meant to convey information if they
  // have no other aria information on them. If we don't ignore them, they may
  // emit signals expected to come from their parent. In addition, because
  // included spans are GroupRole objects, and GroupRole objects are often
  // containers with meaningful information, the inclusion of a span can have
  // the side effect of causing the immediate parent accessible to be ignored.
  // This is especially problematic for platforms which have distinct roles for
  // textual block elements.
  if (isHTMLSpanElement(node)) {
    if (ignoredReasons)
      ignoredReasons->append(IgnoredReason(AXUninteresting));
    return true;
  }

  if (isImage())
    return false;

  if (isCanvas()) {
    if (canvasHasFallbackContent())
      return false;
    LayoutHTMLCanvas* canvas = toLayoutHTMLCanvas(m_layoutObject);
    if (canvas->size().height() <= 1 || canvas->size().width() <= 1) {
      if (ignoredReasons)
        ignoredReasons->append(IgnoredReason(AXProbablyPresentational));
      return true;
    }
    // Otherwise fall through; use presence of help text, title, or description
    // to decide.
  }

  if (isWebArea() || m_layoutObject->isListMarker())
    return false;

  // Using the help text, title or accessibility description (so we
  // check if there's some kind of accessible name for the element)
  // to decide an element's visibility is not as definitive as
  // previous checks, so this should remain as one of the last.
  //
  // These checks are simplified in the interest of execution speed;
  // for example, any element having an alt attribute will make it
  // not ignored, rather than just images.
  if (!getAttribute(aria_helpAttr).isEmpty() ||
      !getAttribute(aria_describedbyAttr).isEmpty() ||
      !getAttribute(altAttr).isEmpty() || !getAttribute(titleAttr).isEmpty())
    return false;

  // Don't ignore generic focusable elements like <div tabindex=0>
  // unless they're completely empty, with no children.
  if (isGenericFocusableElement() && node->hasChildren())
    return false;

  if (isScrollableContainer())
    return false;

  // Ignore layout objects that are block flows with inline children. These
  // are usually dummy layout objects that pad out the tree, but there are
  // some exceptions below.
  if (m_layoutObject->isLayoutBlockFlow() && m_layoutObject->childrenInline() &&
      !canSetFocusAttribute()) {
    // If the layout object has any plain text in it, that text will be
    // inside a LineBox, so the layout object will have a first LineBox.
    bool hasAnyText = !!toLayoutBlockFlow(m_layoutObject)->firstLineBox();

    // Always include interesting-looking objects.
    if (hasAnyText || mouseButtonListener())
      return false;

    if (ignoredReasons)
      ignoredReasons->append(IgnoredReason(AXUninteresting));
    return true;
  }

  // By default, objects should be ignored so that the AX hierarchy is not
  // filled with unnecessary items.
  if (ignoredReasons)
    ignoredReasons->append(IgnoredReason(AXUninteresting));
  return true;
}

//
// Properties of static elements.
//

const AtomicString& AXLayoutObject::accessKey() const {
  Node* node = m_layoutObject->node();
  if (!node)
    return nullAtom;
  if (!node->isElementNode())
    return nullAtom;
  return toElement(node)->getAttribute(accesskeyAttr);
}

RGBA32 AXLayoutObject::computeBackgroundColor() const {
  if (!getLayoutObject())
    return AXNodeObject::backgroundColor();

  Color blendedColor = Color::transparent;
  // Color::blend should be called like this: background.blend(foreground).
  for (LayoutObject* layoutObject = getLayoutObject(); layoutObject;
       layoutObject = layoutObject->parent()) {
    const AXObject* axParent = axObjectCache().getOrCreate(layoutObject);
    if (axParent && axParent != this) {
      Color parentColor = axParent->backgroundColor();
      blendedColor = parentColor.blend(blendedColor);
      return blendedColor.rgb();
    }

    const ComputedStyle* style = layoutObject->style();
    if (!style || !style->hasBackground())
      continue;

    Color currentColor =
        style->visitedDependentColor(CSSPropertyBackgroundColor);
    blendedColor = currentColor.blend(blendedColor);
    // Continue blending until we get no transparency.
    if (!blendedColor.hasAlpha())
      break;
  }

  // If we still have some transparency, blend in the document base color.
  if (blendedColor.hasAlpha()) {
    FrameView* view = documentFrameView();
    if (view) {
      Color documentBaseColor = view->baseBackgroundColor();
      blendedColor = documentBaseColor.blend(blendedColor);
    } else {
      // Default to a white background.
      blendedColor.blendWithWhite();
    }
  }

  return blendedColor.rgb();
}

RGBA32 AXLayoutObject::color() const {
  if (!getLayoutObject() || isColorWell())
    return AXNodeObject::color();

  const ComputedStyle* style = getLayoutObject()->style();
  if (!style)
    return AXNodeObject::color();

  Color color = style->visitedDependentColor(CSSPropertyColor);
  return color.rgb();
}

String AXLayoutObject::fontFamily() const {
  if (!getLayoutObject())
    return AXNodeObject::fontFamily();

  const ComputedStyle* style = getLayoutObject()->style();
  if (!style)
    return AXNodeObject::fontFamily();

  FontDescription& fontDescription =
      const_cast<FontDescription&>(style->getFontDescription());
  return fontDescription.firstFamily().family();
}

// Font size is in pixels.
float AXLayoutObject::fontSize() const {
  if (!getLayoutObject())
    return AXNodeObject::fontSize();

  const ComputedStyle* style = getLayoutObject()->style();
  if (!style)
    return AXNodeObject::fontSize();

  return style->computedFontSize();
}

String AXLayoutObject::text() const {
  if (isPasswordFieldAndShouldHideValue()) {
    if (!getLayoutObject())
      return String();

    const ComputedStyle* style = getLayoutObject()->style();
    if (!style)
      return String();

    unsigned unmaskedTextLength = AXNodeObject::text().length();
    if (!unmaskedTextLength)
      return String();

    UChar maskCharacter = 0;
    switch (style->textSecurity()) {
      case TSNONE:
        break;  // Fall through to the non-password branch.
      case TSDISC:
        maskCharacter = bulletCharacter;
        break;
      case TSCIRCLE:
        maskCharacter = whiteBulletCharacter;
        break;
      case TSSQUARE:
        maskCharacter = blackSquareCharacter;
        break;
    }
    if (maskCharacter) {
      StringBuilder maskedText;
      maskedText.reserveCapacity(unmaskedTextLength);
      for (unsigned i = 0; i < unmaskedTextLength; ++i)
        maskedText.append(maskCharacter);
      return maskedText.toString();
    }
  }

  return AXNodeObject::text();
}

AccessibilityTextDirection AXLayoutObject::textDirection() const {
  if (!getLayoutObject())
    return AXNodeObject::textDirection();

  const ComputedStyle* style = getLayoutObject()->style();
  if (!style)
    return AXNodeObject::textDirection();

  if (style->isHorizontalWritingMode()) {
    switch (style->direction()) {
      case LTR:
        return AccessibilityTextDirectionLTR;
      case RTL:
        return AccessibilityTextDirectionRTL;
    }
  } else {
    switch (style->direction()) {
      case LTR:
        return AccessibilityTextDirectionTTB;
      case RTL:
        return AccessibilityTextDirectionBTT;
    }
  }

  return AXNodeObject::textDirection();
}

int AXLayoutObject::textLength() const {
  if (!isTextControl())
    return -1;

  return text().length();
}

TextStyle AXLayoutObject::getTextStyle() const {
  if (!getLayoutObject())
    return AXNodeObject::getTextStyle();

  const ComputedStyle* style = getLayoutObject()->style();
  if (!style)
    return AXNodeObject::getTextStyle();

  unsigned textStyle = TextStyleNone;
  if (style->fontWeight() == FontWeightBold)
    textStyle |= TextStyleBold;
  if (style->getFontDescription().style() == FontStyleItalic)
    textStyle |= TextStyleItalic;
  if (style->getTextDecoration() == TextDecorationUnderline)
    textStyle |= TextStyleUnderline;
  if (style->getTextDecoration() == TextDecorationLineThrough)
    textStyle |= TextStyleLineThrough;

  return static_cast<TextStyle>(textStyle);
}

KURL AXLayoutObject::url() const {
  if (isAnchor() && isHTMLAnchorElement(m_layoutObject->node())) {
    if (HTMLAnchorElement* anchor = toHTMLAnchorElement(anchorElement()))
      return anchor->href();
  }

  if (isWebArea())
    return m_layoutObject->document().url();

  if (isImage() && isHTMLImageElement(m_layoutObject->node()))
    return toHTMLImageElement(*m_layoutObject->node()).src();

  if (isInputImage())
    return toHTMLInputElement(m_layoutObject->node())->src();

  return KURL();
}

//
// Inline text boxes.
//

void AXLayoutObject::loadInlineTextBoxes() {
  if (!getLayoutObject() || !getLayoutObject()->isText())
    return;

  clearChildren();
  addInlineTextBoxChildren(true);
}

AXObject* AXLayoutObject::nextOnLine() const {
  if (!getLayoutObject())
    return nullptr;

  InlineBox* inlineBox = nullptr;
  if (getLayoutObject()->isLayoutInline())
    inlineBox = toLayoutInline(getLayoutObject())->lastLineBox();
  else if (getLayoutObject()->isText())
    inlineBox = toLayoutText(getLayoutObject())->lastTextBox();

  if (!inlineBox)
    return nullptr;

  AXObject* result = nullptr;
  for (InlineBox* next = inlineBox->nextOnLine(); next;
       next = next->nextOnLine()) {
    LayoutObject* layoutObject =
        LineLayoutAPIShim::layoutObjectFrom(next->getLineLayoutItem());
    result = axObjectCache().getOrCreate(layoutObject);
    if (result)
      break;
  }

  // A static text node might span multiple lines. Try to return the first
  // inline text box within that static text if possible.
  if (result && result->roleValue() == StaticTextRole &&
      result->children().size())
    result = result->children()[0].get();

  return result;
}

AXObject* AXLayoutObject::previousOnLine() const {
  if (!getLayoutObject())
    return nullptr;

  InlineBox* inlineBox = nullptr;
  if (getLayoutObject()->isLayoutInline())
    inlineBox = toLayoutInline(getLayoutObject())->firstLineBox();
  else if (getLayoutObject()->isText())
    inlineBox = toLayoutText(getLayoutObject())->firstTextBox();

  if (!inlineBox)
    return nullptr;

  AXObject* result = nullptr;
  for (InlineBox* prev = inlineBox->prevOnLine(); prev;
       prev = prev->prevOnLine()) {
    LayoutObject* layoutObject =
        LineLayoutAPIShim::layoutObjectFrom(prev->getLineLayoutItem());
    result = axObjectCache().getOrCreate(layoutObject);
    if (result)
      break;
  }

  // A static text node might span multiple lines. Try to return the last inline
  // text box within that static text if possible.
  if (result && result->roleValue() == StaticTextRole &&
      result->children().size())
    result = result->children()[result->children().size() - 1].get();

  return result;
}

//
// Properties of interactive elements.
//

String AXLayoutObject::stringValue() const {
  if (!m_layoutObject)
    return String();

  LayoutBoxModelObject* cssBox = getLayoutBoxModelObject();

  if (cssBox && cssBox->isMenuList()) {
    // LayoutMenuList will go straight to the text() of its selected item.
    // This has to be overridden in the case where the selected item has an ARIA
    // label.
    HTMLSelectElement* selectElement =
        toHTMLSelectElement(m_layoutObject->node());
    int selectedIndex = selectElement->selectedIndex();
    const HeapVector<Member<HTMLElement>>& listItems =
        selectElement->listItems();
    if (selectedIndex >= 0 &&
        static_cast<size_t>(selectedIndex) < listItems.size()) {
      const AtomicString& overriddenDescription =
          listItems[selectedIndex]->fastGetAttribute(aria_labelAttr);
      if (!overriddenDescription.isNull())
        return overriddenDescription;
    }
    return toLayoutMenuList(m_layoutObject)->text();
  }

  if (isWebArea()) {
    // FIXME: Why would a layoutObject exist when the Document isn't attached to
    // a frame?
    if (m_layoutObject->frame())
      return String();

    ASSERT_NOT_REACHED();
  }

  if (isTextControl())
    return text();

  if (m_layoutObject->isFileUploadControl())
    return toLayoutFileUploadControl(m_layoutObject)->fileTextValue();

  // Handle other HTML input elements that aren't text controls, like date and
  // time controls, by returning the string value, with the exception of
  // checkboxes and radio buttons (which would return "on").
  if (getNode() && isHTMLInputElement(getNode())) {
    HTMLInputElement* input = toHTMLInputElement(getNode());
    if (input->type() != InputTypeNames::checkbox &&
        input->type() != InputTypeNames::radio)
      return input->value();
  }

  // FIXME: We might need to implement a value here for more types
  // FIXME: It would be better not to advertise a value at all for the types for
  // which we don't implement one; this would require subclassing or making
  // accessibilityAttributeNames do something other than return a single static
  // array.
  return String();
}

String AXLayoutObject::textAlternative(bool recursive,
                                       bool inAriaLabelledByTraversal,
                                       AXObjectSet& visited,
                                       AXNameFrom& nameFrom,
                                       AXRelatedObjectVector* relatedObjects,
                                       NameSources* nameSources) const {
  if (m_layoutObject) {
    String textAlternative;
    bool foundTextAlternative = false;

    if (m_layoutObject->isBR()) {
      textAlternative = String("\n");
      foundTextAlternative = true;
    } else if (m_layoutObject->isText() &&
               (!recursive || !m_layoutObject->isCounter())) {
      LayoutText* layoutText = toLayoutText(m_layoutObject);
      String result = layoutText->plainText();
      if (!result.isEmpty() || layoutText->isAllCollapsibleWhitespace())
        textAlternative = result;
      else
        textAlternative = layoutText->text();
      foundTextAlternative = true;
    } else if (m_layoutObject->isListMarker() && !recursive) {
      textAlternative = toLayoutListMarker(m_layoutObject)->text();
      foundTextAlternative = true;
    }

    if (foundTextAlternative) {
      nameFrom = AXNameFromContents;
      if (nameSources) {
        nameSources->append(NameSource(false));
        nameSources->last().type = nameFrom;
        nameSources->last().text = textAlternative;
      }
      return textAlternative;
    }
  }

  return AXNodeObject::textAlternative(recursive, inAriaLabelledByTraversal,
                                       visited, nameFrom, relatedObjects,
                                       nameSources);
}

//
// ARIA attributes.
//

void AXLayoutObject::ariaFlowToElements(AXObjectVector& flowTo) const {
  accessibilityChildrenFromAttribute(aria_flowtoAttr, flowTo);
}

void AXLayoutObject::ariaControlsElements(AXObjectVector& controls) const {
  accessibilityChildrenFromAttribute(aria_controlsAttr, controls);
}

void AXLayoutObject::ariaOwnsElements(AXObjectVector& owns) const {
  accessibilityChildrenFromAttribute(aria_ownsAttr, owns);
}

void AXLayoutObject::ariaDescribedbyElements(
    AXObjectVector& describedby) const {
  accessibilityChildrenFromAttribute(aria_describedbyAttr, describedby);
}

void AXLayoutObject::ariaLabelledbyElements(AXObjectVector& labelledby) const {
  accessibilityChildrenFromAttribute(aria_labelledbyAttr, labelledby);
}

bool AXLayoutObject::ariaHasPopup() const {
  return elementAttributeValue(aria_haspopupAttr);
}

bool AXLayoutObject::ariaRoleHasPresentationalChildren() const {
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

AXObject* AXLayoutObject::ancestorForWhichThisIsAPresentationalChild() const {
  // Walk the parent chain looking for a parent that has presentational children
  AXObject* parent = parentObjectIfExists();
  while (parent) {
    if (parent->ariaRoleHasPresentationalChildren())
      break;

    // The descendants of a AXMenuList that are AXLayoutObjects are all
    // presentational. (The real descendants are an AXMenuListPopup and
    // AXMenuListOptions, which are not AXLayoutObjects.)
    if (parent->isMenuList())
      break;

    parent = parent->parentObjectIfExists();
  }

  return parent;
}

bool AXLayoutObject::supportsARIADragging() const {
  const AtomicString& grabbed = getAttribute(aria_grabbedAttr);
  return equalIgnoringCase(grabbed, "true") ||
         equalIgnoringCase(grabbed, "false");
}

bool AXLayoutObject::supportsARIADropping() const {
  const AtomicString& dropEffect = getAttribute(aria_dropeffectAttr);
  return !dropEffect.isEmpty();
}

bool AXLayoutObject::supportsARIAFlowTo() const {
  return !getAttribute(aria_flowtoAttr).isEmpty();
}

bool AXLayoutObject::supportsARIAOwns() const {
  if (!m_layoutObject)
    return false;
  const AtomicString& ariaOwns = getAttribute(aria_ownsAttr);

  return !ariaOwns.isEmpty();
}

//
// ARIA live-region features.
//

const AtomicString& AXLayoutObject::liveRegionStatus() const {
  DEFINE_STATIC_LOCAL(const AtomicString, liveRegionStatusAssertive,
                      ("assertive"));
  DEFINE_STATIC_LOCAL(const AtomicString, liveRegionStatusPolite, ("polite"));
  DEFINE_STATIC_LOCAL(const AtomicString, liveRegionStatusOff, ("off"));

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

const AtomicString& AXLayoutObject::liveRegionRelevant() const {
  DEFINE_STATIC_LOCAL(const AtomicString, defaultLiveRegionRelevant,
                      ("additions text"));
  const AtomicString& relevant = getAttribute(aria_relevantAttr);

  // Default aria-relevant = "additions text".
  if (relevant.isEmpty())
    return defaultLiveRegionRelevant;

  return relevant;
}

bool AXLayoutObject::liveRegionAtomic() const {
  // ARIA roles "alert" and "status" should have an implicit aria-atomic value
  // of true.
  if (getAttribute(aria_atomicAttr).isEmpty() &&
      (roleValue() == AlertRole || roleValue() == StatusRole)) {
    return true;
  }
  return elementAttributeValue(aria_atomicAttr);
}

bool AXLayoutObject::liveRegionBusy() const {
  return elementAttributeValue(aria_busyAttr);
}

//
// Hit testing.
//

AXObject* AXLayoutObject::accessibilityHitTest(const IntPoint& point) const {
  if (!m_layoutObject || !m_layoutObject->hasLayer())
    return nullptr;

  PaintLayer* layer = toLayoutBox(m_layoutObject)->layer();

  HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active);
  HitTestResult hitTestResult = HitTestResult(request, point);
  layer->hitTest(hitTestResult);

  Node* node = hitTestResult.innerNode();
  if (!node)
    return nullptr;

  if (isHTMLAreaElement(node))
    return accessibilityImageMapHitTest(toHTMLAreaElement(node), point);

  if (isHTMLOptionElement(node)) {
    node = toHTMLOptionElement(*node).ownerSelectElement();
    if (!node)
      return nullptr;
  }

  LayoutObject* obj = node->layoutObject();
  if (!obj)
    return nullptr;

  AXObject* result = axObjectCache().getOrCreate(obj);
  result->updateChildrenIfNecessary();

  // Allow the element to perform any hit-testing it might need to do to reach
  // non-layout children.
  result = result->elementAccessibilityHitTest(point);
  if (result && result->accessibilityIsIgnored()) {
    // If this element is the label of a control, a hit test should return the
    // control.
    if (result->isAXLayoutObject()) {
      AXObject* controlObject =
          toAXLayoutObject(result)->correspondingControlForLabelElement();
      if (controlObject && controlObject->nameFromLabelElement())
        return controlObject;
    }

    result = result->parentObjectUnignored();
  }

  return result;
}

AXObject* AXLayoutObject::elementAccessibilityHitTest(
    const IntPoint& point) const {
  if (isSVGImage())
    return remoteSVGElementHitTest(point);

  return AXObject::elementAccessibilityHitTest(point);
}

//
// High-level accessibility tree access.
//

AXObject* AXLayoutObject::computeParent() const {
  ASSERT(!isDetached());
  if (!m_layoutObject)
    return 0;

  if (ariaRoleAttribute() == MenuBarRole)
    return axObjectCache().getOrCreate(m_layoutObject->parent());

  // menuButton and its corresponding menu are DOM siblings, but Accessibility
  // needs them to be parent/child.
  if (ariaRoleAttribute() == MenuRole) {
    AXObject* parent = menuButtonForMenu();
    if (parent)
      return parent;
  }

  LayoutObject* parentObj = layoutParentObject();
  if (parentObj)
    return axObjectCache().getOrCreate(parentObj);

  // A WebArea's parent should be the page popup owner, if any, otherwise null.
  if (isWebArea()) {
    LocalFrame* frame = m_layoutObject->frame();
    return axObjectCache().getOrCreate(frame->pagePopupOwner());
  }

  return 0;
}

AXObject* AXLayoutObject::computeParentIfExists() const {
  if (!m_layoutObject)
    return 0;

  if (ariaRoleAttribute() == MenuBarRole)
    return axObjectCache().get(m_layoutObject->parent());

  // menuButton and its corresponding menu are DOM siblings, but Accessibility
  // needs them to be parent/child.
  if (ariaRoleAttribute() == MenuRole) {
    AXObject* parent = menuButtonForMenu();
    if (parent)
      return parent;
  }

  LayoutObject* parentObj = layoutParentObject();
  if (parentObj)
    return axObjectCache().get(parentObj);

  // A WebArea's parent should be the page popup owner, if any, otherwise null.
  if (isWebArea()) {
    LocalFrame* frame = m_layoutObject->frame();
    return axObjectCache().get(frame->pagePopupOwner());
  }

  return 0;
}

//
// Low-level accessibility tree exploration, only for use within the
// accessibility module.
//

AXObject* AXLayoutObject::rawFirstChild() const {
  if (!m_layoutObject)
    return 0;

  LayoutObject* firstChild = firstChildConsideringContinuation(m_layoutObject);

  if (!firstChild)
    return 0;

  return axObjectCache().getOrCreate(firstChild);
}

AXObject* AXLayoutObject::rawNextSibling() const {
  if (!m_layoutObject)
    return 0;

  LayoutObject* nextSibling = 0;

  LayoutInline* inlineContinuation =
      m_layoutObject->isLayoutBlockFlow()
          ? toLayoutBlockFlow(m_layoutObject)->inlineElementContinuation()
          : nullptr;
  if (inlineContinuation) {
    // Case 1: node is a block and has an inline continuation. Next sibling is
    // the inline continuation's first child.
    nextSibling = firstChildConsideringContinuation(inlineContinuation);
  } else if (m_layoutObject->isAnonymousBlock() &&
             lastChildHasContinuation(m_layoutObject)) {
    // Case 2: Anonymous block parent of the start of a continuation - skip all
    // the way to after the parent of the end, since everything in between will
    // be linked up via the continuation.
    LayoutObject* lastParent =
        endOfContinuations(toLayoutBlock(m_layoutObject)->lastChild())
            ->parent();
    while (lastChildHasContinuation(lastParent))
      lastParent = endOfContinuations(lastParent->slowLastChild())->parent();
    nextSibling = lastParent->nextSibling();
  } else if (LayoutObject* ns = m_layoutObject->nextSibling()) {
    // Case 3: node has an actual next sibling
    nextSibling = ns;
  } else if (isInlineWithContinuation(m_layoutObject)) {
    // Case 4: node is an inline with a continuation. Next sibling is the next
    // sibling of the end of the continuation chain.
    nextSibling = endOfContinuations(m_layoutObject)->nextSibling();
  } else if (m_layoutObject->parent() &&
             isInlineWithContinuation(m_layoutObject->parent())) {
    // Case 5: node has no next sibling, and its parent is an inline with a
    // continuation.
    LayoutObject* continuation =
        toLayoutInline(m_layoutObject->parent())->continuation();

    if (continuation->isLayoutBlock()) {
      // Case 5a: continuation is a block - in this case the block itself is the
      // next sibling.
      nextSibling = continuation;
    } else {
      // Case 5b: continuation is an inline - in this case the inline's first
      // child is the next sibling.
      nextSibling = firstChildConsideringContinuation(continuation);
    }
  }

  if (!nextSibling)
    return 0;

  return axObjectCache().getOrCreate(nextSibling);
}

void AXLayoutObject::addChildren() {
  ASSERT(!isDetached());
  // If the need to add more children in addition to existing children arises,
  // childrenChanged should have been called, leaving the object with no
  // children.
  ASSERT(!m_haveChildren);

  m_haveChildren = true;

  if (!canHaveChildren())
    return;

  HeapVector<Member<AXObject>> ownedChildren;
  computeAriaOwnsChildren(ownedChildren);

  for (AXObject* obj = rawFirstChild(); obj; obj = obj->rawNextSibling()) {
    if (!axObjectCache().isAriaOwned(obj)) {
      obj->setParent(this);
      addChild(obj);
    }
  }

  addHiddenChildren();
  addPopupChildren();
  addImageMapChildren();
  addTextFieldChildren();
  addCanvasChildren();
  addRemoteSVGChildren();
  addInlineTextBoxChildren(false);

  for (const auto& child : m_children) {
    if (!child->cachedParentObject())
      child->setParent(this);
  }

  for (const auto& ownedChild : ownedChildren)
    addChild(ownedChild);
}

bool AXLayoutObject::canHaveChildren() const {
  if (!m_layoutObject)
    return false;

  return AXNodeObject::canHaveChildren();
}

void AXLayoutObject::updateChildrenIfNecessary() {
  if (needsToUpdateChildren())
    clearChildren();

  AXObject::updateChildrenIfNecessary();
}

void AXLayoutObject::clearChildren() {
  AXObject::clearChildren();
  m_childrenDirty = false;
}

//
// Properties of the object's owning document or page.
//

double AXLayoutObject::estimatedLoadingProgress() const {
  if (!m_layoutObject)
    return 0;

  if (isLoaded())
    return 1.0;

  if (LocalFrame* frame = m_layoutObject->document().frame())
    return frame->loader().progress().estimatedProgress();
  return 0;
}

//
// DOM and layout tree access.
//

Node* AXLayoutObject::getNode() const {
  return getLayoutObject() ? getLayoutObject()->node() : nullptr;
}

Document* AXLayoutObject::getDocument() const {
  if (!getLayoutObject())
    return nullptr;
  return &getLayoutObject()->document();
}

FrameView* AXLayoutObject::documentFrameView() const {
  if (!getLayoutObject())
    return nullptr;

  // this is the LayoutObject's Document's LocalFrame's FrameView
  return getLayoutObject()->document().view();
}

Element* AXLayoutObject::anchorElement() const {
  if (!m_layoutObject)
    return 0;

  AXObjectCacheImpl& cache = axObjectCache();
  LayoutObject* currLayoutObject;

  // Search up the layout tree for a LayoutObject with a DOM node. Defer to an
  // earlier continuation, though.
  for (currLayoutObject = m_layoutObject;
       currLayoutObject && !currLayoutObject->node();
       currLayoutObject = currLayoutObject->parent()) {
    if (currLayoutObject->isAnonymousBlock() &&
        currLayoutObject->isLayoutBlockFlow()) {
      LayoutObject* continuation =
          toLayoutBlockFlow(currLayoutObject)->continuation();
      if (continuation)
        return cache.getOrCreate(continuation)->anchorElement();
    }
  }

  // bail if none found
  if (!currLayoutObject)
    return 0;

  // Search up the DOM tree for an anchor element.
  // NOTE: this assumes that any non-image with an anchor is an
  // HTMLAnchorElement
  Node* node = currLayoutObject->node();
  if (!node)
    return nullptr;
  for (Node& runner : NodeTraversal::inclusiveAncestorsOf(*node)) {
    if (isHTMLAnchorElement(runner) ||
        (runner.layoutObject() &&
         cache.getOrCreate(runner.layoutObject())->isAnchor()))
      return toElement(&runner);
  }

  return 0;
}

//
// Functions that retrieve the current selection.
//

AXObject::AXRange AXLayoutObject::selection() const {
  AXRange textSelection = textControlSelection();
  if (textSelection.isValid())
    return textSelection;

  if (!getLayoutObject() || !getLayoutObject()->frame())
    return AXRange();

  VisibleSelection selection =
      getLayoutObject()->frame()->selection().selection();
  if (selection.isNone())
    return AXRange();

  VisiblePosition visibleStart = selection.visibleStart();
  Position start = visibleStart.toParentAnchoredPosition();
  TextAffinity startAffinity = visibleStart.affinity();
  VisiblePosition visibleEnd = selection.visibleEnd();
  Position end = visibleEnd.toParentAnchoredPosition();
  TextAffinity endAffinity = visibleEnd.affinity();

  Node* anchorNode = start.anchorNode();
  ASSERT(anchorNode);

  AXLayoutObject* anchorObject = nullptr;
  // Find the closest node that has a corresponding AXObject.
  // This is because some nodes may be aria hidden or might not even have
  // a layout object if they are part of the shadow DOM.
  while (anchorNode) {
    anchorObject = getUnignoredObjectFromNode(*anchorNode);
    if (anchorObject)
      break;

    if (anchorNode->nextSibling())
      anchorNode = anchorNode->nextSibling();
    else
      anchorNode = anchorNode->parentNode();
  }

  Node* focusNode = end.anchorNode();
  ASSERT(focusNode);

  AXLayoutObject* focusObject = nullptr;
  while (focusNode) {
    focusObject = getUnignoredObjectFromNode(*focusNode);
    if (focusObject)
      break;

    if (focusNode->previousSibling())
      focusNode = focusNode->previousSibling();
    else
      focusNode = focusNode->parentNode();
  }

  if (!anchorObject || !focusObject)
    return AXRange();

  int anchorOffset = anchorObject->indexForVisiblePosition(visibleStart);
  ASSERT(anchorOffset >= 0);
  int focusOffset = focusObject->indexForVisiblePosition(visibleEnd);
  ASSERT(focusOffset >= 0);
  return AXRange(anchorObject, anchorOffset, startAffinity, focusObject,
                 focusOffset, endAffinity);
}

// Gets only the start and end offsets of the selection computed using the
// current object as the starting point. Returns a null selection if there is
// no selection in the subtree rooted at this object.
AXObject::AXRange AXLayoutObject::selectionUnderObject() const {
  AXRange textSelection = textControlSelection();
  if (textSelection.isValid())
    return textSelection;

  if (!getNode() || !getLayoutObject()->frame())
    return AXRange();

  VisibleSelection selection =
      getLayoutObject()->frame()->selection().selection();
  Range* selectionRange = firstRangeOf(selection);
  ContainerNode* parentNode = getNode()->parentNode();
  int nodeIndex = getNode()->nodeIndex();
  if (!selectionRange
      // Selection is contained in node.
      ||
      !(parentNode &&
        selectionRange->comparePoint(parentNode, nodeIndex, IGNORE_EXCEPTION) <
            0 &&
        selectionRange->comparePoint(parentNode, nodeIndex + 1,
                                     IGNORE_EXCEPTION) > 0)) {
    return AXRange();
  }

  int start = indexForVisiblePosition(selection.visibleStart());
  ASSERT(start >= 0);
  int end = indexForVisiblePosition(selection.visibleEnd());
  ASSERT(end >= 0);

  return AXRange(start, end);
}

AXObject::AXRange AXLayoutObject::textControlSelection() const {
  if (!getLayoutObject())
    return AXRange();

  LayoutObject* layout = nullptr;
  if (getLayoutObject()->isTextControl()) {
    layout = getLayoutObject();
  } else {
    Element* focusedElement = getDocument()->focusedElement();
    if (focusedElement && focusedElement->layoutObject() &&
        focusedElement->layoutObject()->isTextControl())
      layout = focusedElement->layoutObject();
  }

  if (!layout)
    return AXRange();

  AXObject* axObject = axObjectCache().getOrCreate(layout);
  if (!axObject || !axObject->isAXLayoutObject())
    return AXRange();

  VisibleSelection selection = layout->frame()->selection().selection();
  TextControlElement* textControl =
      toLayoutTextControl(layout)->textControlElement();
  ASSERT(textControl);
  int start = textControl->selectionStart();
  int end = textControl->selectionEnd();

  return AXRange(axObject, start, selection.visibleStart().affinity(), axObject,
                 end, selection.visibleEnd().affinity());
}

int AXLayoutObject::indexForVisiblePosition(
    const VisiblePosition& position) const {
  if (getLayoutObject() && getLayoutObject()->isTextControl()) {
    TextControlElement* textControl =
        toLayoutTextControl(getLayoutObject())->textControlElement();
    return textControl->indexForVisiblePosition(position);
  }

  if (!getNode())
    return 0;

  Position indexPosition = position.deepEquivalent();
  if (indexPosition.isNull())
    return 0;

  Range* range = Range::create(*getDocument());
  range->setStart(getNode(), 0, IGNORE_EXCEPTION);
  range->setEnd(indexPosition, IGNORE_EXCEPTION);

  return TextIterator::rangeLength(range->startPosition(),
                                   range->endPosition());
}

AXLayoutObject* AXLayoutObject::getUnignoredObjectFromNode(Node& node) const {
  if (isDetached())
    return nullptr;

  AXObject* axObject = axObjectCache().getOrCreate(&node);
  if (!axObject)
    return nullptr;

  if (axObject->isAXLayoutObject() && !axObject->accessibilityIsIgnored())
    return toAXLayoutObject(axObject);

  return nullptr;
}

//
// Modify or take an action on an object.
//

// Convert from an accessible object and offset to a VisiblePosition.
static VisiblePosition toVisiblePosition(AXObject* obj, int offset) {
  if (!obj->getNode())
    return VisiblePosition();

  Node* node = obj->getNode();
  if (!node->isTextNode()) {
    int childCount = obj->children().size();

    // Place position immediately before the container node, if there was no
    // children.
    if (childCount == 0) {
      if (!obj->parentObject())
        return VisiblePosition();
      return toVisiblePosition(obj->parentObject(), obj->indexInParent());
    }

    // The offsets are child offsets over the AX tree. Note that we allow
    // for the offset to equal the number of children as |Range| does.
    if (offset < 0 || offset > childCount)
      return VisiblePosition();

    // Clamp to between 0 and child count - 1.
    int clampedOffset =
        static_cast<unsigned>(offset) > (obj->children().size() - 1)
            ? offset - 1
            : offset;
    AXObject* childObj = obj->children()[clampedOffset];
    Node* childNode = childObj->getNode();
    if (!childNode || !childNode->parentNode())
      return VisiblePosition();

    // The index in parent.
    int adjustedOffset = childNode->nodeIndex();

    // If we had to clamp the offset above, the client wants to select the
    // end of the node.
    if (clampedOffset != offset)
      adjustedOffset++;

    return createVisiblePosition(
        Position::editingPositionOf(childNode->parentNode(), adjustedOffset));
  }

  // If it is a text node, we need to call some utility functions that use a
  // TextIterator to walk the characters of the node and figure out the position
  // corresponding to the visible character at position |offset|.
  ContainerNode* parent = node->parentNode();
  if (!parent)
    return VisiblePosition();

  VisiblePosition nodePosition = blink::visiblePositionBeforeNode(*node);
  int nodeIndex = blink::indexForVisiblePosition(nodePosition, parent);
  return blink::visiblePositionForIndex(nodeIndex + offset, parent);
}

void AXLayoutObject::setSelection(const AXRange& selection) {
  if (!getLayoutObject() || !selection.isValid())
    return;

  AXObject* anchorObject =
      selection.anchorObject ? selection.anchorObject.get() : this;
  AXObject* focusObject =
      selection.focusObject ? selection.focusObject.get() : this;

  if (!isValidSelectionBound(anchorObject) ||
      !isValidSelectionBound(focusObject)) {
    return;
  }

  // The selection offsets are offsets into the accessible value.
  if (anchorObject == focusObject &&
      anchorObject->getLayoutObject()->isTextControl()) {
    TextControlElement* textControl =
        toLayoutTextControl(anchorObject->getLayoutObject())
            ->textControlElement();
    if (selection.anchorOffset <= selection.focusOffset) {
      textControl->setSelectionRange(selection.anchorOffset,
                                     selection.focusOffset,
                                     SelectionHasForwardDirection);
    } else {
      textControl->setSelectionRange(selection.focusOffset,
                                     selection.anchorOffset,
                                     SelectionHasBackwardDirection);
    }
    return;
  }

  LocalFrame* frame = getLayoutObject()->frame();
  if (!frame)
    return;

  // TODO(dglazkov): The use of updateStyleAndLayoutIgnorePendingStylesheets
  // needs to be audited.  see http://crbug.com/590369 for more details.
  // This callsite should probably move up the stack.
  frame->document()->updateStyleAndLayoutIgnorePendingStylesheets();

  // Set the selection based on visible positions, because the offsets in
  // accessibility nodes are based on visible indexes, which often skips
  // redundant whitespace, for example.
  VisiblePosition anchorVisiblePosition =
      toVisiblePosition(anchorObject, selection.anchorOffset);
  VisiblePosition focusVisiblePosition =
      toVisiblePosition(focusObject, selection.focusOffset);
  if (anchorVisiblePosition.isNull() || focusVisiblePosition.isNull())
    return;

  frame->selection().setSelection(
      SelectionInDOMTree::Builder()
          .collapse(anchorVisiblePosition.toPositionWithAffinity())
          .extend(focusVisiblePosition.deepEquivalent())
          .build());
}

bool AXLayoutObject::isValidSelectionBound(const AXObject* boundObject) const {
  return getLayoutObject() && boundObject && !boundObject->isDetached() &&
         boundObject->isAXLayoutObject() && boundObject->getLayoutObject() &&
         boundObject->getLayoutObject()->frame() ==
             getLayoutObject()->frame() &&
         &boundObject->axObjectCache() == &axObjectCache();
}

void AXLayoutObject::setValue(const String& string) {
  if (!getNode() || !getNode()->isElementNode())
    return;
  if (!m_layoutObject || !m_layoutObject->isBoxModelObject())
    return;

  LayoutBoxModelObject* layoutObject = toLayoutBoxModelObject(m_layoutObject);
  if (layoutObject->isTextField() && isHTMLInputElement(*getNode()))
    toHTMLInputElement(*getNode())
        .setValue(string, DispatchInputAndChangeEvent);
  else if (layoutObject->isTextArea() && isHTMLTextAreaElement(*getNode()))
    toHTMLTextAreaElement(*getNode())
        .setValue(string, DispatchInputAndChangeEvent);
}

//
// Notifications that this object may have changed.
//

void AXLayoutObject::handleActiveDescendantChanged() {
  if (!getLayoutObject())
    return;

  AXObject* focusedObject = axObjectCache().focusedObject();
  if (focusedObject == this && supportsActiveDescendant()) {
    axObjectCache().postNotification(
        getLayoutObject(), AXObjectCacheImpl::AXActiveDescendantChanged);
  }
}

void AXLayoutObject::handleAriaExpandedChanged() {
  // Find if a parent of this object should handle aria-expanded changes.
  AXObject* containerParent = this->parentObject();
  while (containerParent) {
    bool foundParent = false;

    switch (containerParent->roleValue()) {
      case TreeRole:
      case TreeGridRole:
      case GridRole:
      case TableRole:
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
    axObjectCache().postNotification(containerParent,
                                     AXObjectCacheImpl::AXRowCountChanged);

  // Post that the specific row either collapsed or expanded.
  AccessibilityExpanded expanded = isExpanded();
  if (!expanded)
    return;

  if (roleValue() == RowRole || roleValue() == TreeItemRole) {
    AXObjectCacheImpl::AXNotification notification =
        AXObjectCacheImpl::AXRowExpanded;
    if (expanded == ExpandedCollapsed)
      notification = AXObjectCacheImpl::AXRowCollapsed;

    axObjectCache().postNotification(this, notification);
  } else {
    axObjectCache().postNotification(this,
                                     AXObjectCacheImpl::AXExpandedChanged);
  }
}

void AXLayoutObject::textChanged() {
  if (!m_layoutObject)
    return;

  Settings* settings = getDocument()->settings();
  if (settings && settings->inlineTextBoxAccessibilityEnabled() &&
      roleValue() == StaticTextRole)
    childrenChanged();

  // Do this last - AXNodeObject::textChanged posts live region announcements,
  // and we should update the inline text boxes first.
  AXNodeObject::textChanged();
}

//
// Text metrics. Most of these should be deprecated, needs major cleanup.
//

// NOTE: Consider providing this utility method as AX API
int AXLayoutObject::index(const VisiblePosition& position) const {
  if (position.isNull() || !isTextControl())
    return -1;

  if (layoutObjectContainsPosition(m_layoutObject, position.deepEquivalent()))
    return indexForVisiblePosition(position);

  return -1;
}

VisiblePosition AXLayoutObject::visiblePositionForIndex(int index) const {
  if (!m_layoutObject)
    return VisiblePosition();

  if (m_layoutObject->isTextControl())
    return toLayoutTextControl(m_layoutObject)
        ->textControlElement()
        ->visiblePositionForIndex(index);

  Node* node = m_layoutObject->node();
  if (!node)
    return VisiblePosition();

  if (index <= 0)
    return createVisiblePosition(firstPositionInOrBeforeNode(node));

  Position start, end;
  bool selected = Range::selectNodeContents(node, start, end);
  if (!selected)
    return VisiblePosition();

  CharacterIterator it(start, end);
  it.advance(index - 1);
  return createVisiblePosition(Position(it.currentContainer(), it.endOffset()),
                               TextAffinity::Upstream);
}

void AXLayoutObject::addInlineTextBoxChildren(bool force) {
  Settings* settings = getDocument()->settings();
  if (!force && (!settings || !settings->inlineTextBoxAccessibilityEnabled()))
    return;

  if (!getLayoutObject() || !getLayoutObject()->isText())
    return;

  if (getLayoutObject()->needsLayout()) {
    // If a LayoutText needs layout, its inline text boxes are either
    // nonexistent or invalid, so defer until the layout happens and
    // the layoutObject calls AXObjectCacheImpl::inlineTextBoxesUpdated.
    return;
  }

  LayoutText* layoutText = toLayoutText(getLayoutObject());
  for (RefPtr<AbstractInlineTextBox> box =
           layoutText->firstAbstractInlineTextBox();
       box.get(); box = box->nextInlineTextBox()) {
    AXObject* axObject = axObjectCache().getOrCreate(box.get());
    if (!axObject->accessibilityIsIgnored())
      m_children.append(axObject);
  }
}

void AXLayoutObject::lineBreaks(Vector<int>& lineBreaks) const {
  if (!isTextControl())
    return;

  VisiblePosition visiblePos = visiblePositionForIndex(0);
  VisiblePosition prevVisiblePos = visiblePos;
  visiblePos = nextLinePosition(visiblePos, LayoutUnit(), HasEditableAXRole);
  // nextLinePosition moves to the end of the current line when there are
  // no more lines.
  while (visiblePos.isNotNull() && !inSameLine(prevVisiblePos, visiblePos)) {
    lineBreaks.append(indexForVisiblePosition(visiblePos));
    prevVisiblePos = visiblePos;
    visiblePos = nextLinePosition(visiblePos, LayoutUnit(), HasEditableAXRole);

    // Make sure we always make forward progress.
    if (visiblePos.deepEquivalent().compareTo(prevVisiblePos.deepEquivalent()) <
        0)
      break;
  }
}

//
// Private.
//

AXObject* AXLayoutObject::treeAncestorDisallowingChild() const {
  // Determine if this is in a tree. If so, we apply special behavior to make it
  // work like an AXOutline.
  AXObject* axObj = parentObject();
  AXObject* treeAncestor = 0;
  while (axObj) {
    if (axObj->isTree()) {
      treeAncestor = axObj;
      break;
    }
    axObj = axObj->parentObject();
  }

  // If the object is in a tree, only tree items should be exposed (and the
  // children of tree items).
  if (treeAncestor) {
    AccessibilityRole role = roleValue();
    if (role != TreeItemRole && role != StaticTextRole)
      return treeAncestor;
  }
  return 0;
}

bool AXLayoutObject::isTabItemSelected() const {
  if (!isTabItem() || !getLayoutObject())
    return false;

  Node* node = getNode();
  if (!node || !node->isElementNode())
    return false;

  // The ARIA spec says a tab item can also be selected if it is aria-labeled by
  // a tabpanel that has keyboard focus inside of it, or if a tabpanel in its
  // aria-controls list has KB focus inside of it.
  AXObject* focusedElement = axObjectCache().focusedObject();
  if (!focusedElement)
    return false;

  HeapVector<Member<Element>> elements;
  elementsFromAttribute(elements, aria_controlsAttr);

  for (const auto& element : elements) {
    AXObject* tabPanel = axObjectCache().getOrCreate(element);

    // A tab item should only control tab panels.
    if (!tabPanel || tabPanel->roleValue() != TabPanelRole)
      continue;

    AXObject* checkFocusElement = focusedElement;
    // Check if the focused element is a descendant of the element controlled by
    // the tab item.
    while (checkFocusElement) {
      if (tabPanel == checkFocusElement)
        return true;
      checkFocusElement = checkFocusElement->parentObject();
    }
  }

  return false;
}

AXObject* AXLayoutObject::accessibilityImageMapHitTest(
    HTMLAreaElement* area,
    const IntPoint& point) const {
  if (!area)
    return 0;

  AXObject* parent = axObjectCache().getOrCreate(area->imageElement());
  if (!parent)
    return 0;

  for (const auto& child : parent->children()) {
    if (child->getBoundsInFrameCoordinates().contains(point))
      return child.get();
  }

  return 0;
}

LayoutObject* AXLayoutObject::layoutParentObject() const {
  if (!m_layoutObject)
    return 0;

  LayoutObject* startOfConts = m_layoutObject->isLayoutBlockFlow()
                                   ? startOfContinuations(m_layoutObject)
                                   : nullptr;
  if (startOfConts) {
    // Case 1: node is a block and is an inline's continuation. Parent
    // is the start of the continuation chain.
    return startOfConts;
  }

  LayoutObject* parent = m_layoutObject->parent();
  startOfConts =
      parent && parent->isLayoutInline() ? startOfContinuations(parent) : 0;
  if (startOfConts) {
    // Case 2: node's parent is an inline which is some node's continuation;
    // parent is the earliest node in the continuation chain.
    return startOfConts;
  }

  LayoutObject* firstChild = parent ? parent->slowFirstChild() : 0;
  if (firstChild && firstChild->node()) {
    // Case 3: The first sibling is the beginning of a continuation chain. Find
    // the origin of that continuation.  Get the node's layoutObject and follow
    // that continuation chain until the first child is found.
    for (LayoutObject* nodeLayoutFirstChild =
             firstChild->node()->layoutObject();
         nodeLayoutFirstChild != firstChild;
         nodeLayoutFirstChild = firstChild->node()->layoutObject()) {
      for (LayoutObject* contsTest = nodeLayoutFirstChild; contsTest;
           contsTest = nextContinuation(contsTest)) {
        if (contsTest == firstChild) {
          parent = nodeLayoutFirstChild->parent();
          break;
        }
      }
      LayoutObject* newFirstChild = parent->slowFirstChild();
      if (firstChild == newFirstChild)
        break;
      firstChild = newFirstChild;
      if (!firstChild->node())
        break;
    }
  }

  return parent;
}

bool AXLayoutObject::isSVGImage() const {
  return remoteSVGRootElement();
}

void AXLayoutObject::detachRemoteSVGRoot() {
  if (AXSVGRoot* root = remoteSVGRootElement())
    root->setParent(0);
}

AXSVGRoot* AXLayoutObject::remoteSVGRootElement() const {
  // FIXME(dmazzoni): none of this code properly handled multiple references to
  // the same remote SVG document. I'm disabling this support until it can be
  // fixed properly.
  return 0;
}

AXObject* AXLayoutObject::remoteSVGElementHitTest(const IntPoint& point) const {
  AXObject* remote = remoteSVGRootElement();
  if (!remote)
    return 0;

  IntSize offset =
      point - roundedIntPoint(getBoundsInFrameCoordinates().location());
  return remote->accessibilityHitTest(IntPoint(offset));
}

// The boundingBox for elements within the remote SVG element needs to be offset
// by its position within the parent page, otherwise they are in relative
// coordinates only.
void AXLayoutObject::offsetBoundingBoxForRemoteSVGElement(
    LayoutRect& rect) const {
  for (AXObject* parent = parentObject(); parent;
       parent = parent->parentObject()) {
    if (parent->isAXSVGRoot()) {
      rect.moveBy(
          parent->parentObject()->getBoundsInFrameCoordinates().location());
      break;
    }
  }
}

// Hidden children are those that are not laid out or visible, but are
// specifically marked as aria-hidden=false,
// meaning that they should be exposed to the AX hierarchy.
void AXLayoutObject::addHiddenChildren() {
  Node* node = this->getNode();
  if (!node)
    return;

  // First do a quick run through to determine if we have any hidden nodes (most
  // often we will not).  If we do have hidden nodes, we need to determine where
  // to insert them so they match DOM order as close as possible.
  bool shouldInsertHiddenNodes = false;
  for (Node& child : NodeTraversal::childrenOf(*node)) {
    if (!child.layoutObject() && isNodeAriaVisible(&child)) {
      shouldInsertHiddenNodes = true;
      break;
    }
  }

  if (!shouldInsertHiddenNodes)
    return;

  // Iterate through all of the children, including those that may have already
  // been added, and try to insert hidden nodes in the correct place in the DOM
  // order.
  unsigned insertionIndex = 0;
  for (Node& child : NodeTraversal::childrenOf(*node)) {
    if (child.layoutObject()) {
      // Find out where the last layout sibling is located within m_children.
      if (AXObject* childObject = axObjectCache().get(child.layoutObject())) {
        if (childObject->accessibilityIsIgnored()) {
          const auto& children = childObject->children();
          childObject = children.size() ? children.last().get() : 0;
        }
        if (childObject)
          insertionIndex = m_children.find(childObject) + 1;
        continue;
      }
    }

    if (!isNodeAriaVisible(&child))
      continue;

    unsigned previousSize = m_children.size();
    if (insertionIndex > previousSize)
      insertionIndex = previousSize;

    insertChild(axObjectCache().getOrCreate(&child), insertionIndex);
    insertionIndex += (m_children.size() - previousSize);
  }
}

void AXLayoutObject::addTextFieldChildren() {
  Node* node = this->getNode();
  if (!isHTMLInputElement(node))
    return;

  HTMLInputElement& input = toHTMLInputElement(*node);
  Element* spinButtonElement = input.userAgentShadowRoot()->getElementById(
      ShadowElementNames::spinButton());
  if (!spinButtonElement || !spinButtonElement->isSpinButtonElement())
    return;

  AXSpinButton* axSpinButton =
      toAXSpinButton(axObjectCache().getOrCreate(SpinButtonRole));
  axSpinButton->setSpinButtonElement(toSpinButtonElement(spinButtonElement));
  axSpinButton->setParent(this);
  m_children.append(axSpinButton);
}

void AXLayoutObject::addImageMapChildren() {
  LayoutBoxModelObject* cssBox = getLayoutBoxModelObject();
  if (!cssBox || !cssBox->isLayoutImage())
    return;

  HTMLMapElement* map = toLayoutImage(cssBox)->imageMap();
  if (!map)
    return;

  for (HTMLAreaElement& area :
       Traversal<HTMLAreaElement>::descendantsOf(*map)) {
    // add an <area> element for this child if it has a link
    AXObject* obj = axObjectCache().getOrCreate(&area);
    if (obj) {
      AXImageMapLink* areaObject = toAXImageMapLink(obj);
      areaObject->setParent(this);
      ASSERT(areaObject->axObjectID() != 0);
      if (!areaObject->accessibilityIsIgnored())
        m_children.append(areaObject);
      else
        axObjectCache().remove(areaObject->axObjectID());
    }
  }
}

void AXLayoutObject::addCanvasChildren() {
  if (!isHTMLCanvasElement(getNode()))
    return;

  // If it's a canvas, it won't have laid out children, but it might have
  // accessible fallback content.  Clear m_haveChildren because
  // AXNodeObject::addChildren will expect it to be false.
  ASSERT(!m_children.size());
  m_haveChildren = false;
  AXNodeObject::addChildren();
}

void AXLayoutObject::addPopupChildren() {
  if (!isHTMLInputElement(getNode()))
    return;
  if (AXObject* axPopup = toHTMLInputElement(getNode())->popupRootAXObject())
    m_children.append(axPopup);
}

void AXLayoutObject::addRemoteSVGChildren() {
  AXSVGRoot* root = remoteSVGRootElement();
  if (!root)
    return;

  root->setParent(this);

  if (root->accessibilityIsIgnored()) {
    for (const auto& child : root->children())
      m_children.append(child);
  } else {
    m_children.append(root);
  }
}

bool AXLayoutObject::elementAttributeValue(
    const QualifiedName& attributeName) const {
  if (!m_layoutObject)
    return false;

  return equalIgnoringCase(getAttribute(attributeName), "true");
}

}  // namespace blink
