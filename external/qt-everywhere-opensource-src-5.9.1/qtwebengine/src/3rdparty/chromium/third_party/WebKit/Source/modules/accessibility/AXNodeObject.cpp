/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#include "modules/accessibility/AXNodeObject.h"

#include "core/InputTypeNames.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/Element.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/FlatTreeTraversal.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDListElement.h"
#include "core/html/HTMLFieldSetElement.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLLegendElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLMeterElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/HTMLTableCaptionElement.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTableElement.h"
#include "core/html/HTMLTableRowElement.h"
#include "core/html/HTMLTableSectionElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/LabelsNodeList.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/MediaControlElements.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutObject.h"
#include "core/svg/SVGElement.h"
#include "modules/accessibility/AXObjectCacheImpl.h"
#include "platform/UserGestureIndicator.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

AXNodeObject::AXNodeObject(Node* node, AXObjectCacheImpl& axObjectCache)
    : AXObject(axObjectCache),
      m_ariaRole(UnknownRole),
      m_childrenDirty(false),
#if ENABLE(ASSERT)
      m_initialized(false),
#endif
      m_node(node) {
}

AXNodeObject* AXNodeObject::create(Node* node,
                                   AXObjectCacheImpl& axObjectCache) {
  return new AXNodeObject(node, axObjectCache);
}

AXNodeObject::~AXNodeObject() {
  ASSERT(!m_node);
}

void AXNodeObject::alterSliderValue(bool increase) {
  if (roleValue() != SliderRole)
    return;

  float value = valueForRange();
  float step = stepValueForRange();

  value += increase ? step : -step;

  setValue(String::number(value));
  axObjectCache().postNotification(getNode(),
                                   AXObjectCacheImpl::AXValueChanged);
}

AXObject* AXNodeObject::activeDescendant() {
  if (!getNode() || !getNode()->isElementNode())
    return nullptr;

  const AtomicString& activeDescendantAttr =
      getAttribute(aria_activedescendantAttr);
  if (activeDescendantAttr.isNull() || activeDescendantAttr.isEmpty())
    return nullptr;

  Element* element = toElement(getNode());
  Element* descendant =
      element->treeScope().getElementById(activeDescendantAttr);
  if (!descendant)
    return nullptr;

  AXObject* axDescendant = axObjectCache().getOrCreate(descendant);
  return axDescendant;
}

bool AXNodeObject::computeAccessibilityIsIgnored(
    IgnoredReasons* ignoredReasons) const {
#if ENABLE(ASSERT)
  // Double-check that an AXObject is never accessed before
  // it's been initialized.
  ASSERT(m_initialized);
#endif

  // If this element is within a parent that cannot have children, it should not
  // be exposed.
  if (isDescendantOfLeafNode()) {
    if (ignoredReasons)
      ignoredReasons->append(
          IgnoredReason(AXAncestorIsLeafNode, leafNodeAncestor()));
    return true;
  }

  // Ignore labels that are already referenced by a control.
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

  Element* element = getNode()->isElementNode() ? toElement(getNode())
                                                : getNode()->parentElement();
  if (!getLayoutObject() && (!element || !element->isInCanvasSubtree()) &&
      !equalIgnoringCase(getAttribute(aria_hiddenAttr), "false")) {
    if (ignoredReasons)
      ignoredReasons->append(IgnoredReason(AXNotRendered));
    return true;
  }

  if (m_role == UnknownRole) {
    if (ignoredReasons)
      ignoredReasons->append(IgnoredReason(AXUninteresting));
    return true;
  }
  return false;
}

static bool isListElement(Node* node) {
  return isHTMLUListElement(*node) || isHTMLOListElement(*node) ||
         isHTMLDListElement(*node);
}

static bool isPresentationalInTable(AXObject* parent,
                                    HTMLElement* currentElement) {
  if (!currentElement)
    return false;

  Node* parentNode = parent->getNode();
  if (!parentNode || !parentNode->isHTMLElement())
    return false;

  // AXTable determines the role as checking isTableXXX.
  // If Table has explicit role including presentation, AXTable doesn't assign
  // implicit Role to a whole Table. That's why we should check it based on
  // node.
  // Normal Table Tree is that
  // cell(its role)-> tr(tr role)-> tfoot, tbody, thead(ignored role) ->
  //     table(table role).
  // If table has presentation role, it will be like
  // cell(group)-> tr(unknown) -> tfoot, tbody, thead(ignored) ->
  //     table(presentation).
  if (isHTMLTableCellElement(*currentElement) &&
      isHTMLTableRowElement(*parentNode))
    return parent->hasInheritedPresentationalRole();

  if (isHTMLTableRowElement(*currentElement) &&
      isHTMLTableSectionElement(toHTMLElement(*parentNode))) {
    // Because TableSections have ignored role, presentation should be checked
    // with its parent node.
    AXObject* tableObject = parent->parentObject();
    Node* tableNode = tableObject ? tableObject->getNode() : 0;
    return isHTMLTableElement(tableNode) &&
           tableObject->hasInheritedPresentationalRole();
  }
  return false;
}

static bool isRequiredOwnedElement(AXObject* parent,
                                   AccessibilityRole currentRole,
                                   HTMLElement* currentElement) {
  Node* parentNode = parent->getNode();
  if (!parentNode || !parentNode->isHTMLElement())
    return false;

  if (currentRole == ListItemRole)
    return isListElement(parentNode);
  if (currentRole == ListMarkerRole)
    return isHTMLLIElement(*parentNode);
  if (currentRole == MenuItemCheckBoxRole || currentRole == MenuItemRole ||
      currentRole == MenuItemRadioRole)
    return isHTMLMenuElement(*parentNode);

  if (!currentElement)
    return false;
  if (isHTMLTableCellElement(*currentElement))
    return isHTMLTableRowElement(*parentNode);
  if (isHTMLTableRowElement(*currentElement))
    return isHTMLTableSectionElement(toHTMLElement(*parentNode));

  // In case of ListboxRole and its child, ListBoxOptionRole, inheritance of
  // presentation role is handled in AXListBoxOption because ListBoxOption Role
  // doesn't have any child.
  // If it's just ignored because of presentation, we can't see any AX tree
  // related to ListBoxOption.
  return false;
}

const AXObject* AXNodeObject::inheritsPresentationalRoleFrom() const {
  // ARIA states if an item can get focus, it should not be presentational.
  if (canSetFocusAttribute())
    return 0;

  if (isPresentational())
    return this;

  // http://www.w3.org/TR/wai-aria/complete#presentation
  // ARIA spec says that the user agent MUST apply an inherited role of
  // presentation
  // to any owned elements that do not have an explicit role defined.
  if (ariaRoleAttribute() != UnknownRole)
    return 0;

  AXObject* parent = parentObject();
  if (!parent)
    return 0;

  HTMLElement* element = nullptr;
  if (getNode() && getNode()->isHTMLElement())
    element = toHTMLElement(getNode());
  if (!parent->hasInheritedPresentationalRole()) {
    if (!getLayoutObject() || !getLayoutObject()->isBoxModelObject())
      return 0;

    LayoutBoxModelObject* cssBox = toLayoutBoxModelObject(getLayoutObject());
    if (!cssBox->isTableCell() && !cssBox->isTableRow())
      return 0;

    if (!isPresentationalInTable(parent, element))
      return 0;
  }
  // ARIA spec says that when a parent object is presentational and this object
  // is a required owned element of that parent, then this object is also
  // presentational.
  if (isRequiredOwnedElement(parent, roleValue(), element))
    return parent;
  return 0;
}

bool AXNodeObject::isDescendantOfElementType(
    const HTMLQualifiedName& tagName) const {
  if (!getNode())
    return false;

  for (Element* parent = getNode()->parentElement(); parent;
       parent = parent->parentElement()) {
    if (parent->hasTagName(tagName))
      return true;
  }
  return false;
}

AccessibilityRole AXNodeObject::nativeAccessibilityRoleIgnoringAria() const {
  if (!getNode())
    return UnknownRole;

  // HTMLAnchorElement sets isLink only when it has hrefAttr.
  // We assume that it is also LinkRole if it has event listners even though it
  // doesn't have hrefAttr.
  if (getNode()->isLink() || (isHTMLAnchorElement(*getNode()) && isClickable()))
    return LinkRole;

  if (isHTMLButtonElement(*getNode()))
    return buttonRoleType();

  if (isHTMLDetailsElement(*getNode()))
    return DetailsRole;

  if (isHTMLSummaryElement(*getNode())) {
    ContainerNode* parent = FlatTreeTraversal::parent(*getNode());
    if (parent && isHTMLDetailsElement(parent))
      return DisclosureTriangleRole;
    return UnknownRole;
  }

  if (isHTMLInputElement(*getNode())) {
    HTMLInputElement& input = toHTMLInputElement(*getNode());
    const AtomicString& type = input.type();
    if (input.dataList())
      return ComboBoxRole;
    if (type == InputTypeNames::button) {
      if ((getNode()->parentNode() &&
           isHTMLMenuElement(getNode()->parentNode())) ||
          (parentObject() && parentObject()->roleValue() == MenuRole))
        return MenuItemRole;
      return buttonRoleType();
    }
    if (type == InputTypeNames::checkbox) {
      if ((getNode()->parentNode() &&
           isHTMLMenuElement(getNode()->parentNode())) ||
          (parentObject() && parentObject()->roleValue() == MenuRole))
        return MenuItemCheckBoxRole;
      return CheckBoxRole;
    }
    if (type == InputTypeNames::date)
      return DateRole;
    if (type == InputTypeNames::datetime ||
        type == InputTypeNames::datetime_local ||
        type == InputTypeNames::month || type == InputTypeNames::week)
      return DateTimeRole;
    if (type == InputTypeNames::file)
      return ButtonRole;
    if (type == InputTypeNames::radio) {
      if ((getNode()->parentNode() &&
           isHTMLMenuElement(getNode()->parentNode())) ||
          (parentObject() && parentObject()->roleValue() == MenuRole))
        return MenuItemRadioRole;
      return RadioButtonRole;
    }
    if (type == InputTypeNames::number)
      return SpinButtonRole;
    if (input.isTextButton())
      return buttonRoleType();
    if (type == InputTypeNames::range)
      return SliderRole;
    if (type == InputTypeNames::color)
      return ColorWellRole;
    if (type == InputTypeNames::time)
      return InputTimeRole;
    return TextFieldRole;
  }

  if (isHTMLSelectElement(*getNode())) {
    HTMLSelectElement& selectElement = toHTMLSelectElement(*getNode());
    return selectElement.isMultiple() ? ListBoxRole : PopUpButtonRole;
  }

  if (isHTMLTextAreaElement(*getNode()))
    return TextFieldRole;

  if (headingLevel())
    return HeadingRole;

  if (isHTMLDivElement(*getNode()))
    return DivRole;

  if (isHTMLMeterElement(*getNode()))
    return MeterRole;

  if (isHTMLOutputElement(*getNode()))
    return StatusRole;

  if (isHTMLParagraphElement(*getNode()))
    return ParagraphRole;

  if (isHTMLLabelElement(*getNode()))
    return LabelRole;

  if (isHTMLLegendElement(*getNode()))
    return LegendRole;

  if (isHTMLRubyElement(*getNode()))
    return RubyRole;

  if (isHTMLDListElement(*getNode()))
    return DescriptionListRole;

  if (isHTMLAudioElement(*getNode()))
    return AudioRole;
  if (isHTMLVideoElement(*getNode()))
    return VideoRole;

  if (getNode()->hasTagName(ddTag))
    return DescriptionListDetailRole;

  if (getNode()->hasTagName(dtTag))
    return DescriptionListTermRole;

  if (getNode()->nodeName() == "math")
    return MathRole;

  if (getNode()->hasTagName(rpTag) || getNode()->hasTagName(rtTag))
    return AnnotationRole;

  if (isHTMLFormElement(*getNode()))
    return FormRole;

  if (getNode()->hasTagName(abbrTag))
    return AbbrRole;

  if (getNode()->hasTagName(articleTag))
    return ArticleRole;

  if (getNode()->hasTagName(mainTag))
    return MainRole;

  if (getNode()->hasTagName(markTag))
    return MarkRole;

  if (getNode()->hasTagName(navTag))
    return NavigationRole;

  if (getNode()->hasTagName(asideTag))
    return ComplementaryRole;

  if (getNode()->hasTagName(preTag))
    return PreRole;

  if (getNode()->hasTagName(sectionTag))
    return RegionRole;

  if (getNode()->hasTagName(addressTag))
    return ContentInfoRole;

  if (isHTMLDialogElement(*getNode()))
    return DialogRole;

  // The HTML element should not be exposed as an element. That's what the
  // LayoutView element does.
  if (isHTMLHtmlElement(*getNode()))
    return IgnoredRole;

  if (isHTMLIFrameElement(*getNode())) {
    const AtomicString& ariaRole = getAttribute(roleAttr);
    if (ariaRole == "none" || ariaRole == "presentation")
      return IframePresentationalRole;
    return IframeRole;
  }

  // There should only be one banner/contentInfo per page. If header/footer are
  // being used within an article or section then it should not be exposed as
  // whole page's banner/contentInfo but as a group role.
  if (getNode()->hasTagName(headerTag)) {
    if (isDescendantOfElementType(articleTag) ||
        isDescendantOfElementType(sectionTag) ||
        (getNode()->parentElement() &&
         getNode()->parentElement()->hasTagName(mainTag))) {
      return GroupRole;
    }
    return BannerRole;
  }

  if (getNode()->hasTagName(footerTag)) {
    if (isDescendantOfElementType(articleTag) ||
        isDescendantOfElementType(sectionTag) ||
        (getNode()->parentElement() &&
         getNode()->parentElement()->hasTagName(mainTag))) {
      return GroupRole;
    }
    return FooterRole;
  }

  if (getNode()->hasTagName(blockquoteTag))
    return BlockquoteRole;

  if (getNode()->hasTagName(captionTag))
    return CaptionRole;

  if (getNode()->hasTagName(figcaptionTag))
    return FigcaptionRole;

  if (getNode()->hasTagName(figureTag))
    return FigureRole;

  if (getNode()->nodeName() == "TIME")
    return TimeRole;

  if (isEmbeddedObject())
    return EmbeddedObjectRole;

  if (isHTMLHRElement(*getNode()))
    return SplitterRole;

  if (isFieldset()) {
    return GroupRole;
  }

  return UnknownRole;
}

AccessibilityRole AXNodeObject::determineAccessibilityRole() {
  if (!getNode())
    return UnknownRole;

  if ((m_ariaRole = determineAriaRoleAttribute()) != UnknownRole)
    return m_ariaRole;
  if (getNode()->isTextNode())
    return StaticTextRole;

  AccessibilityRole role = nativeAccessibilityRoleIgnoringAria();
  if (role != UnknownRole)
    return role;
  if (getNode()->isElementNode()) {
    Element* element = toElement(getNode());
    // A generic element with tabIndex explicitly set gets GroupRole.
    // The layout checks for focusability aren't critical here; a false
    // positive would be harmless.
    if (element->isInCanvasSubtree() && element->supportsFocus())
      return GroupRole;
  }
  return UnknownRole;
}

AccessibilityRole AXNodeObject::determineAriaRoleAttribute() const {
  const AtomicString& ariaRole = getAttribute(roleAttr);
  if (ariaRole.isNull() || ariaRole.isEmpty())
    return UnknownRole;

  AccessibilityRole role = ariaRoleToWebCoreRole(ariaRole);

  // ARIA states if an item can get focus, it should not be presentational.
  if ((role == NoneRole || role == PresentationalRole) &&
      canSetFocusAttribute())
    return UnknownRole;

  if (role == ButtonRole)
    role = buttonRoleType();

  role = remapAriaRoleDueToParent(role);

  if (role)
    return role;

  return UnknownRole;
}

void AXNodeObject::accessibilityChildrenFromAttribute(
    QualifiedName attr,
    AXObject::AXObjectVector& children) const {
  HeapVector<Member<Element>> elements;
  elementsFromAttribute(elements, attr);

  AXObjectCacheImpl& cache = axObjectCache();
  for (const auto& element : elements) {
    if (AXObject* child = cache.getOrCreate(element)) {
      // Only aria-labelledby and aria-describedby can target hidden elements.
      if (child->accessibilityIsIgnored() && attr != aria_labelledbyAttr &&
          attr != aria_labeledbyAttr && attr != aria_describedbyAttr) {
        continue;
      }
      children.append(child);
    }
  }
}

// This only returns true if this is the element that actually has the
// contentEditable attribute set, unlike node->hasEditableStyle() which will
// also return true if an ancestor is editable.
bool AXNodeObject::hasContentEditableAttributeSet() const {
  const AtomicString& contentEditableValue = getAttribute(contenteditableAttr);
  if (contentEditableValue.isNull())
    return false;
  // Both "true" (case-insensitive) and the empty string count as true.
  return contentEditableValue.isEmpty() ||
         equalIgnoringCase(contentEditableValue, "true");
}

bool AXNodeObject::isTextControl() const {
  if (hasContentEditableAttributeSet())
    return true;

  switch (roleValue()) {
    case TextFieldRole:
    case ComboBoxRole:
    case SearchBoxRole:
    case SpinButtonRole:
      return true;
    default:
      return false;
  }
}

bool AXNodeObject::isGenericFocusableElement() const {
  if (!canSetFocusAttribute())
    return false;

  // If it's a control, it's not generic.
  if (isControl())
    return false;

  // If it has an aria role, it's not generic.
  if (m_ariaRole != UnknownRole)
    return false;

  // If the content editable attribute is set on this element, that's the reason
  // it's focusable, and existing logic should handle this case already - so
  // it's not a generic focusable element.

  if (hasContentEditableAttributeSet())
    return false;

  // The web area and body element are both focusable, but existing logic
  // handles these cases already, so we don't need to include them here.
  if (roleValue() == WebAreaRole)
    return false;
  if (isHTMLBodyElement(getNode()))
    return false;

  // An SVG root is focusable by default, but it's probably not interactive, so
  // don't include it. It can still be made accessible by giving it an ARIA
  // role.
  if (roleValue() == SVGRootRole)
    return false;

  return true;
}

AXObject* AXNodeObject::menuButtonForMenu() const {
  Element* menuItem = menuItemElementForMenu();

  if (menuItem) {
    // ARIA just has generic menu items. AppKit needs to know if this is a top
    // level items like MenuBarButton or MenuBarItem
    AXObject* menuItemAX = axObjectCache().getOrCreate(menuItem);
    if (menuItemAX && menuItemAX->isMenuButton())
      return menuItemAX;
  }
  return 0;
}

static Element* siblingWithAriaRole(String role, Node* node) {
  Node* parent = node->parentNode();
  if (!parent)
    return 0;

  for (Element* sibling = ElementTraversal::firstChild(*parent); sibling;
       sibling = ElementTraversal::nextSibling(*sibling)) {
    const AtomicString& siblingAriaRole = sibling->getAttribute(roleAttr);
    if (equalIgnoringCase(siblingAriaRole, role))
      return sibling;
  }

  return 0;
}

Element* AXNodeObject::menuItemElementForMenu() const {
  if (ariaRoleAttribute() != MenuRole)
    return 0;

  return siblingWithAriaRole("menuitem", getNode());
}

Element* AXNodeObject::mouseButtonListener() const {
  Node* node = this->getNode();
  if (!node)
    return 0;

  if (!node->isElementNode())
    node = node->parentElement();

  if (!node)
    return 0;

  for (Element* element = toElement(node); element;
       element = element->parentElement()) {
    // It's a pretty common practice to put click listeners on the body or
    // document, but that's almost never what the user wants when clicking on an
    // accessible element.
    if (isHTMLBodyElement(element))
      break;

    if (element->hasEventListeners(EventTypeNames::click) ||
        element->hasEventListeners(EventTypeNames::mousedown) ||
        element->hasEventListeners(EventTypeNames::mouseup) ||
        element->hasEventListeners(EventTypeNames::DOMActivate))
      return element;
  }

  return 0;
}

AccessibilityRole AXNodeObject::remapAriaRoleDueToParent(
    AccessibilityRole role) const {
  // Some objects change their role based on their parent.
  // However, asking for the unignoredParent calls accessibilityIsIgnored(),
  // which can trigger a loop.  While inside the call stack of creating an
  // element, we need to avoid accessibilityIsIgnored().
  // https://bugs.webkit.org/show_bug.cgi?id=65174

  if (role != ListBoxOptionRole && role != MenuItemRole)
    return role;

  for (AXObject* parent = parentObject();
       parent && !parent->accessibilityIsIgnored();
       parent = parent->parentObject()) {
    AccessibilityRole parentAriaRole = parent->ariaRoleAttribute();

    // Selects and listboxes both have options as child roles, but they map to
    // different roles within WebCore.
    if (role == ListBoxOptionRole && parentAriaRole == MenuRole)
      return MenuItemRole;
    // An aria "menuitem" may map to MenuButton or MenuItem depending on its
    // parent.
    if (role == MenuItemRole && parentAriaRole == GroupRole)
      return MenuButtonRole;

    // If the parent had a different role, then we don't need to continue
    // searching up the chain.
    if (parentAriaRole)
      break;
  }

  return role;
}

void AXNodeObject::init() {
#if ENABLE(ASSERT)
  ASSERT(!m_initialized);
  m_initialized = true;
#endif
  m_role = determineAccessibilityRole();
}

void AXNodeObject::detach() {
  AXObject::detach();
  m_node = nullptr;
}

bool AXNodeObject::isAnchor() const {
  return !isNativeImage() && isLink();
}

bool AXNodeObject::isControl() const {
  Node* node = this->getNode();
  if (!node)
    return false;

  return ((node->isElementNode() && toElement(node)->isFormControlElement()) ||
          AXObject::isARIAControl(ariaRoleAttribute()));
}

bool AXNodeObject::isControllingVideoElement() const {
  Node* node = this->getNode();
  if (!node)
    return true;

  return isHTMLVideoElement(toParentMediaElement(node));
}

bool AXNodeObject::isEmbeddedObject() const {
  return isHTMLPlugInElement(getNode());
}

bool AXNodeObject::isFieldset() const {
  return isHTMLFieldSetElement(getNode());
}

bool AXNodeObject::isHeading() const {
  return roleValue() == HeadingRole;
}

bool AXNodeObject::isHovered() const {
  if (Node* node = this->getNode())
    return node->isHovered();
  return false;
}

bool AXNodeObject::isImage() const {
  return roleValue() == ImageRole;
}

bool AXNodeObject::isImageButton() const {
  return isNativeImage() && isButton();
}

bool AXNodeObject::isInputImage() const {
  Node* node = this->getNode();
  if (roleValue() == ButtonRole && isHTMLInputElement(node))
    return toHTMLInputElement(*node).type() == InputTypeNames::image;

  return false;
}

bool AXNodeObject::isLink() const {
  return roleValue() == LinkRole;
}

bool AXNodeObject::isMenu() const {
  return roleValue() == MenuRole;
}

bool AXNodeObject::isMenuButton() const {
  return roleValue() == MenuButtonRole;
}

bool AXNodeObject::isMeter() const {
  return roleValue() == MeterRole;
}

bool AXNodeObject::isMultiSelectable() const {
  const AtomicString& ariaMultiSelectable =
      getAttribute(aria_multiselectableAttr);
  if (equalIgnoringCase(ariaMultiSelectable, "true"))
    return true;
  if (equalIgnoringCase(ariaMultiSelectable, "false"))
    return false;

  return isHTMLSelectElement(getNode()) &&
         toHTMLSelectElement(*getNode()).isMultiple();
}

bool AXNodeObject::isNativeCheckboxOrRadio() const {
  Node* node = this->getNode();
  if (!isHTMLInputElement(node))
    return false;

  HTMLInputElement* input = toHTMLInputElement(node);
  return input->type() == InputTypeNames::checkbox ||
         input->type() == InputTypeNames::radio;
}

bool AXNodeObject::isNativeImage() const {
  Node* node = this->getNode();
  if (!node)
    return false;

  if (isHTMLImageElement(*node))
    return true;

  if (isHTMLPlugInElement(*node))
    return true;

  if (isHTMLInputElement(*node))
    return toHTMLInputElement(*node).type() == InputTypeNames::image;

  return false;
}

bool AXNodeObject::isNativeTextControl() const {
  Node* node = this->getNode();
  if (!node)
    return false;

  if (isHTMLTextAreaElement(*node))
    return true;

  if (isHTMLInputElement(*node))
    return toHTMLInputElement(node)->isTextField();

  return false;
}

bool AXNodeObject::isNonNativeTextControl() const {
  if (isNativeTextControl())
    return false;

  if (hasContentEditableAttributeSet())
    return true;

  if (isARIATextControl())
    return true;

  return false;
}

bool AXNodeObject::isPasswordField() const {
  Node* node = this->getNode();
  if (!isHTMLInputElement(node))
    return false;

  AccessibilityRole ariaRole = ariaRoleAttribute();
  if (ariaRole != TextFieldRole && ariaRole != UnknownRole)
    return false;

  return toHTMLInputElement(node)->type() == InputTypeNames::password;
}

bool AXNodeObject::isProgressIndicator() const {
  return roleValue() == ProgressIndicatorRole;
}

bool AXNodeObject::isRichlyEditable() const {
  return hasContentEditableAttributeSet();
}

bool AXNodeObject::isSlider() const {
  return roleValue() == SliderRole;
}

bool AXNodeObject::isNativeSlider() const {
  Node* node = this->getNode();
  if (!node)
    return false;

  if (!isHTMLInputElement(node))
    return false;

  return toHTMLInputElement(node)->type() == InputTypeNames::range;
}

bool AXNodeObject::isChecked() const {
  Node* node = this->getNode();
  if (!node)
    return false;

  // First test for native checkedness semantics
  if (isHTMLInputElement(*node))
    return toHTMLInputElement(*node).shouldAppearChecked();

  // Else, if this is an ARIA role checkbox or radio or menuitemcheckbox
  // or menuitemradio or switch, respect the aria-checked attribute
  switch (ariaRoleAttribute()) {
    case CheckBoxRole:
    case MenuItemCheckBoxRole:
    case MenuItemRadioRole:
    case RadioButtonRole:
    case SwitchRole:
      if (equalIgnoringCase(getAttribute(aria_checkedAttr), "true"))
        return true;
      return false;
    default:
      break;
  }

  // Otherwise it's not checked
  return false;
}

bool AXNodeObject::isClickable() const {
  if (getNode()) {
    if (getNode()->isElementNode() &&
        toElement(getNode())->isDisabledFormControl())
      return false;

    // Note: we can't call getNode()->willRespondToMouseClickEvents() because
    // that triggers a style recalc and can delete this.
    if (getNode()->hasEventListeners(EventTypeNames::mouseup) ||
        getNode()->hasEventListeners(EventTypeNames::mousedown) ||
        getNode()->hasEventListeners(EventTypeNames::click) ||
        getNode()->hasEventListeners(EventTypeNames::DOMActivate))
      return true;
  }

  return AXObject::isClickable();
}

bool AXNodeObject::isEnabled() const {
  if (isDescendantOfDisabledNode())
    return false;

  Node* node = this->getNode();
  if (!node || !node->isElementNode())
    return true;

  return !toElement(node)->isDisabledFormControl();
}

AccessibilityExpanded AXNodeObject::isExpanded() const {
  if (getNode() && isHTMLSummaryElement(*getNode())) {
    if (getNode()->parentNode() &&
        isHTMLDetailsElement(getNode()->parentNode()))
      return toElement(getNode()->parentNode())->hasAttribute(openAttr)
                 ? ExpandedExpanded
                 : ExpandedCollapsed;
  }

  const AtomicString& expanded = getAttribute(aria_expandedAttr);
  if (equalIgnoringCase(expanded, "true"))
    return ExpandedExpanded;
  if (equalIgnoringCase(expanded, "false"))
    return ExpandedCollapsed;

  return ExpandedUndefined;
}

bool AXNodeObject::isPressed() const {
  if (!isButton())
    return false;

  Node* node = this->getNode();
  if (!node)
    return false;

  // ARIA button with aria-pressed not undefined, then check for aria-pressed
  // attribute rather than getNode()->active()
  if (ariaRoleAttribute() == ToggleButtonRole) {
    if (equalIgnoringCase(getAttribute(aria_pressedAttr), "true") ||
        equalIgnoringCase(getAttribute(aria_pressedAttr), "mixed"))
      return true;
    return false;
  }

  return node->isActive();
}

bool AXNodeObject::isReadOnly() const {
  Node* node = this->getNode();
  if (!node)
    return true;

  if (isHTMLTextAreaElement(*node))
    return toHTMLTextAreaElement(*node).isReadOnly();

  if (isHTMLInputElement(*node)) {
    HTMLInputElement& input = toHTMLInputElement(*node);
    if (input.isTextField())
      return input.isReadOnly();
  }

  return !hasEditableStyle(*node);
}

bool AXNodeObject::isRequired() const {
  Node* n = this->getNode();
  if (n && (n->isElementNode() && toElement(n)->isFormControlElement()) &&
      hasAttribute(requiredAttr))
    return toHTMLFormControlElement(n)->isRequired();

  if (equalIgnoringCase(getAttribute(aria_requiredAttr), "true"))
    return true;

  return false;
}

bool AXNodeObject::canSetFocusAttribute() const {
  Node* node = getNode();
  if (!node)
    return false;

  if (isWebArea())
    return true;

  // Children of elements with an aria-activedescendant attribute should be
  // focusable if they have a (non-presentational) ARIA role.
  if (!isPresentational() && ariaRoleAttribute() != UnknownRole &&
      ancestorExposesActiveDescendant())
    return true;

  // NOTE: It would be more accurate to ask the document whether
  // setFocusedNode() would do anything. For example, setFocusedNode() will do
  // nothing if the current focused node will not relinquish the focus.
  if (isDisabledFormControl(node))
    return false;

  return node->isElementNode() && toElement(node)->supportsFocus();
}

bool AXNodeObject::canSetValueAttribute() const {
  if (equalIgnoringCase(getAttribute(aria_readonlyAttr), "true"))
    return false;

  if (isProgressIndicator() || isSlider())
    return true;

  if (isTextControl() && !isNativeTextControl())
    return true;

  // Any node could be contenteditable, so isReadOnly should be relied upon
  // for this information for all elements.
  return !isReadOnly();
}

bool AXNodeObject::canSetSelectedAttribute() const {
  // ARIA list box options can be selected if they are children of an element
  // with an aria-activedescendant attribute.
  if (ariaRoleAttribute() == ListBoxOptionRole &&
      ancestorExposesActiveDescendant())
    return true;
  return AXObject::canSetSelectedAttribute();
}

bool AXNodeObject::canvasHasFallbackContent() const {
  Node* node = this->getNode();
  if (!isHTMLCanvasElement(node))
    return false;

  // If it has any children that are elements, we'll assume it might be fallback
  // content. If it has no children or its only children are not elements
  // (e.g. just text nodes), it doesn't have fallback content.
  return ElementTraversal::firstChild(*node);
}

int AXNodeObject::headingLevel() const {
  // headings can be in block flow and non-block flow
  Node* node = this->getNode();
  if (!node)
    return 0;

  if (roleValue() == HeadingRole) {
    String levelStr = getAttribute(aria_levelAttr);
    if (!levelStr.isEmpty()) {
      int level = levelStr.toInt();
      if (level >= 1 && level <= 9)
        return level;
      return 1;
    }
  }

  if (!node->isHTMLElement())
    return 0;

  HTMLElement& element = toHTMLElement(*node);
  if (element.hasTagName(h1Tag))
    return 1;

  if (element.hasTagName(h2Tag))
    return 2;

  if (element.hasTagName(h3Tag))
    return 3;

  if (element.hasTagName(h4Tag))
    return 4;

  if (element.hasTagName(h5Tag))
    return 5;

  if (element.hasTagName(h6Tag))
    return 6;

  return 0;
}

unsigned AXNodeObject::hierarchicalLevel() const {
  Node* node = this->getNode();
  if (!node || !node->isElementNode())
    return 0;

  Element* element = toElement(node);
  String levelStr = element->getAttribute(aria_levelAttr);
  if (!levelStr.isEmpty()) {
    int level = levelStr.toInt();
    if (level > 0)
      return level;
    return 1;
  }

  // Only tree item will calculate its level through the DOM currently.
  if (roleValue() != TreeItemRole)
    return 0;

  // Hierarchy leveling starts at 1, to match the aria-level spec.
  // We measure tree hierarchy by the number of groups that the item is within.
  unsigned level = 1;
  for (AXObject* parent = parentObject(); parent;
       parent = parent->parentObject()) {
    AccessibilityRole parentRole = parent->roleValue();
    if (parentRole == GroupRole)
      level++;
    else if (parentRole == TreeRole)
      break;
  }

  return level;
}

String AXNodeObject::ariaAutoComplete() const {
  if (roleValue() != ComboBoxRole)
    return String();

  const AtomicString& ariaAutoComplete =
      getAttribute(aria_autocompleteAttr).lower();

  if (ariaAutoComplete == "inline" || ariaAutoComplete == "list" ||
      ariaAutoComplete == "both")
    return ariaAutoComplete;

  return String();
}

void AXNodeObject::markers(Vector<DocumentMarker::MarkerType>& markerTypes,
                           Vector<AXRange>& markerRanges) const {
  if (!getNode() || !getDocument() || !getDocument()->view())
    return;

  DocumentMarkerController& markerController = getDocument()->markers();
  DocumentMarkerVector markers = markerController.markersFor(getNode());
  for (size_t i = 0; i < markers.size(); ++i) {
    DocumentMarker* marker = markers[i];
    switch (marker->type()) {
      case DocumentMarker::Spelling:
      case DocumentMarker::Grammar:
      case DocumentMarker::TextMatch:
        markerTypes.append(marker->type());
        markerRanges.append(
            AXRange(marker->startOffset(), marker->endOffset()));
        break;
      case DocumentMarker::InvisibleSpellcheck:
      case DocumentMarker::Composition:
        // No need for accessibility to know about these marker types.
        break;
    }
  }
}

AccessibilityOrientation AXNodeObject::orientation() const {
  const AtomicString& ariaOrientation = getAttribute(aria_orientationAttr);
  AccessibilityOrientation orientation = AccessibilityOrientationUndefined;
  if (equalIgnoringCase(ariaOrientation, "horizontal"))
    orientation = AccessibilityOrientationHorizontal;
  else if (equalIgnoringCase(ariaOrientation, "vertical"))
    orientation = AccessibilityOrientationVertical;

  switch (roleValue()) {
    case ComboBoxRole:
    case ListBoxRole:
    case MenuRole:
    case ScrollBarRole:
    case TreeRole:
      if (orientation == AccessibilityOrientationUndefined)
        orientation = AccessibilityOrientationVertical;

      return orientation;
    case MenuBarRole:
    case SliderRole:
    case SplitterRole:
    case TabListRole:
    case ToolbarRole:
      if (orientation == AccessibilityOrientationUndefined)
        orientation = AccessibilityOrientationHorizontal;

      return orientation;
    case RadioGroupRole:
    case TreeGridRole:
    // TODO(nektar): Fix bug 532670 and remove table role.
    case TableRole:
      return orientation;
    default:
      return AXObject::orientation();
  }
}

String AXNodeObject::text() const {
  // If this is a user defined static text, use the accessible name computation.
  if (ariaRoleAttribute() == StaticTextRole)
    return computedName();

  if (!isTextControl())
    return String();

  Node* node = this->getNode();
  if (!node)
    return String();

  if (isNativeTextControl() &&
      (isHTMLTextAreaElement(*node) || isHTMLInputElement(*node)))
    return toTextControlElement(*node).value();

  if (!node->isElementNode())
    return String();

  return toElement(node)->innerText();
}

AccessibilityButtonState AXNodeObject::checkboxOrRadioValue() const {
  if (isNativeCheckboxInMixedState())
    return ButtonStateMixed;

  if (isNativeCheckboxOrRadio())
    return isChecked() ? ButtonStateOn : ButtonStateOff;

  return AXObject::checkboxOrRadioValue();
}

RGBA32 AXNodeObject::colorValue() const {
  if (!isHTMLInputElement(getNode()) || !isColorWell())
    return AXObject::colorValue();

  HTMLInputElement* input = toHTMLInputElement(getNode());
  const AtomicString& type = input->getAttribute(typeAttr);
  if (!equalIgnoringCase(type, "color"))
    return AXObject::colorValue();

  // HTMLInputElement::value always returns a string parseable by Color.
  Color color;
  bool success = color.setFromString(input->value());
  DCHECK(success);
  return color.rgb();
}

AriaCurrentState AXNodeObject::ariaCurrentState() const {
  if (!hasAttribute(aria_currentAttr))
    return AXObject::ariaCurrentState();

  const AtomicString& attributeValue = getAttribute(aria_currentAttr);
  if (attributeValue.isEmpty() || equalIgnoringCase(attributeValue, "false"))
    return AriaCurrentStateFalse;
  if (equalIgnoringCase(attributeValue, "true"))
    return AriaCurrentStateTrue;
  if (equalIgnoringCase(attributeValue, "page"))
    return AriaCurrentStatePage;
  if (equalIgnoringCase(attributeValue, "step"))
    return AriaCurrentStateStep;
  if (equalIgnoringCase(attributeValue, "location"))
    return AriaCurrentStateLocation;
  if (equalIgnoringCase(attributeValue, "date"))
    return AriaCurrentStateDate;
  if (equalIgnoringCase(attributeValue, "time"))
    return AriaCurrentStateTime;
  // An unknown value should return true.
  if (!attributeValue.isEmpty())
    return AriaCurrentStateTrue;

  return AXObject::ariaCurrentState();
}

InvalidState AXNodeObject::getInvalidState() const {
  if (hasAttribute(aria_invalidAttr)) {
    const AtomicString& attributeValue = getAttribute(aria_invalidAttr);
    if (equalIgnoringCase(attributeValue, "false"))
      return InvalidStateFalse;
    if (equalIgnoringCase(attributeValue, "true"))
      return InvalidStateTrue;
    if (equalIgnoringCase(attributeValue, "spelling"))
      return InvalidStateSpelling;
    if (equalIgnoringCase(attributeValue, "grammar"))
      return InvalidStateGrammar;
    // A yet unknown value.
    if (!attributeValue.isEmpty())
      return InvalidStateOther;
  }

  if (getNode() && getNode()->isElementNode() &&
      toElement(getNode())->isFormControlElement()) {
    HTMLFormControlElement* element = toHTMLFormControlElement(getNode());
    HeapVector<Member<HTMLFormControlElement>> invalidControls;
    bool isInvalid =
        !element->checkValidity(&invalidControls, CheckValidityDispatchNoEvent);
    return isInvalid ? InvalidStateTrue : InvalidStateFalse;
  }

  return AXObject::getInvalidState();
}

int AXNodeObject::posInSet() const {
  if (supportsSetSizeAndPosInSet()) {
    String posInSetStr = getAttribute(aria_posinsetAttr);
    if (!posInSetStr.isEmpty()) {
      int posInSet = posInSetStr.toInt();
      if (posInSet > 0)
        return posInSet;
      return 1;
    }

    return AXObject::indexInParent() + 1;
  }

  return 0;
}

int AXNodeObject::setSize() const {
  if (supportsSetSizeAndPosInSet()) {
    String setSizeStr = getAttribute(aria_setsizeAttr);
    if (!setSizeStr.isEmpty()) {
      int setSize = setSizeStr.toInt();
      if (setSize > 0)
        return setSize;
      return 1;
    }

    if (parentObject()) {
      const auto& siblings = parentObject()->children();
      return siblings.size();
    }
  }

  return 0;
}

String AXNodeObject::ariaInvalidValue() const {
  if (getInvalidState() == InvalidStateOther)
    return getAttribute(aria_invalidAttr);

  return String();
}

String AXNodeObject::valueDescription() const {
  if (!supportsRangeValue())
    return String();

  return getAttribute(aria_valuetextAttr).getString();
}

float AXNodeObject::valueForRange() const {
  if (hasAttribute(aria_valuenowAttr))
    return getAttribute(aria_valuenowAttr).toFloat();

  if (isNativeSlider())
    return toHTMLInputElement(*getNode()).valueAsNumber();

  if (isHTMLMeterElement(getNode()))
    return toHTMLMeterElement(*getNode()).value();

  return 0.0;
}

float AXNodeObject::maxValueForRange() const {
  if (hasAttribute(aria_valuemaxAttr))
    return getAttribute(aria_valuemaxAttr).toFloat();

  if (isNativeSlider())
    return toHTMLInputElement(*getNode()).maximum();

  if (isHTMLMeterElement(getNode()))
    return toHTMLMeterElement(*getNode()).max();

  return 0.0;
}

float AXNodeObject::minValueForRange() const {
  if (hasAttribute(aria_valueminAttr))
    return getAttribute(aria_valueminAttr).toFloat();

  if (isNativeSlider())
    return toHTMLInputElement(*getNode()).minimum();

  if (isHTMLMeterElement(getNode()))
    return toHTMLMeterElement(*getNode()).min();

  return 0.0;
}

float AXNodeObject::stepValueForRange() const {
  if (!isNativeSlider())
    return 0.0;

  Decimal step =
      toHTMLInputElement(*getNode()).createStepRange(RejectAny).step();
  return step.toString().toFloat();
}

String AXNodeObject::stringValue() const {
  Node* node = this->getNode();
  if (!node)
    return String();

  if (isHTMLSelectElement(*node)) {
    HTMLSelectElement& selectElement = toHTMLSelectElement(*node);
    int selectedIndex = selectElement.selectedIndex();
    const HeapVector<Member<HTMLElement>>& listItems =
        selectElement.listItems();
    if (selectedIndex >= 0 &&
        static_cast<size_t>(selectedIndex) < listItems.size()) {
      const AtomicString& overriddenDescription =
          listItems[selectedIndex]->fastGetAttribute(aria_labelAttr);
      if (!overriddenDescription.isNull())
        return overriddenDescription;
    }
    if (!selectElement.isMultiple())
      return selectElement.value();
    return String();
  }

  if (isNativeTextControl())
    return text();

  // Handle other HTML input elements that aren't text controls, like date and
  // time controls, by returning the string value, with the exception of
  // checkboxes and radio buttons (which would return "on").
  if (isHTMLInputElement(node)) {
    HTMLInputElement* input = toHTMLInputElement(node);
    if (input->type() != InputTypeNames::checkbox &&
        input->type() != InputTypeNames::radio)
      return input->value();
  }

  return String();
}

AccessibilityRole AXNodeObject::ariaRoleAttribute() const {
  return m_ariaRole;
}

// Returns the nearest LayoutBlockFlow ancestor which does not have an
// inlineBoxWrapper - i.e. is not itself an inline object.
static LayoutBlockFlow* nonInlineBlockFlow(LayoutObject* object) {
  LayoutObject* current = object;
  while (current) {
    if (current->isLayoutBlockFlow()) {
      LayoutBlockFlow* blockFlow = toLayoutBlockFlow(current);
      if (!blockFlow->inlineBoxWrapper())
        return blockFlow;
    }
    current = current->parent();
  }

  ASSERT_NOT_REACHED();
  return nullptr;
}

// Returns true if |r1| and |r2| are both non-null, both inline, and are
// contained within the same non-inline LayoutBlockFlow.
static bool isInSameNonInlineBlockFlow(LayoutObject* r1, LayoutObject* r2) {
  if (!r1 || !r2)
    return false;
  if (!r1->isInline() || !r2->isInline())
    return false;
  LayoutBlockFlow* b1 = nonInlineBlockFlow(r1);
  LayoutBlockFlow* b2 = nonInlineBlockFlow(r2);
  return b1 && b2 && b1 == b2;
}

bool AXNodeObject::isNativeCheckboxInMixedState() const {
  if (!isHTMLInputElement(m_node))
    return false;

  HTMLInputElement* input = toHTMLInputElement(m_node);
  return input->type() == InputTypeNames::checkbox &&
         input->shouldAppearIndeterminate();
}

//
// New AX name calculation.
//

String AXNodeObject::textAlternative(bool recursive,
                                     bool inAriaLabelledByTraversal,
                                     AXObjectSet& visited,
                                     AXNameFrom& nameFrom,
                                     AXRelatedObjectVector* relatedObjects,
                                     NameSources* nameSources) const {
  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  if (nameSources)
    ASSERT(relatedObjects);

  bool foundTextAlternative = false;

  if (!getNode() && !getLayoutObject())
    return String();

  String textAlternative = ariaTextAlternative(
      recursive, inAriaLabelledByTraversal, visited, nameFrom, relatedObjects,
      nameSources, &foundTextAlternative);
  if (foundTextAlternative && !nameSources)
    return textAlternative;

  // Step 2E from: http://www.w3.org/TR/accname-aam-1.1
  if (recursive && !inAriaLabelledByTraversal && isControl() && !isButton()) {
    // No need to set any name source info in a recursive call.
    if (isTextControl())
      return text();

    if (isRange()) {
      const AtomicString& ariaValuetext = getAttribute(aria_valuetextAttr);
      if (!ariaValuetext.isNull())
        return ariaValuetext.getString();
      return String::number(valueForRange());
    }

    return stringValue();
  }

  // Step 2D from: http://www.w3.org/TR/accname-aam-1.1
  textAlternative = nativeTextAlternative(visited, nameFrom, relatedObjects,
                                          nameSources, &foundTextAlternative);
  if (!textAlternative.isEmpty() && !nameSources)
    return textAlternative;

  // Step 2F / 2G from: http://www.w3.org/TR/accname-aam-1.1
  if (recursive || nameFromContents()) {
    nameFrom = AXNameFromContents;
    if (nameSources) {
      nameSources->append(NameSource(foundTextAlternative));
      nameSources->last().type = nameFrom;
    }

    Node* node = this->getNode();
    if (node && node->isTextNode())
      textAlternative = toText(node)->wholeText();
    else if (isHTMLBRElement(node))
      textAlternative = String("\n");
    else
      textAlternative = textFromDescendants(visited, false);

    if (!textAlternative.isEmpty()) {
      if (nameSources) {
        foundTextAlternative = true;
        nameSources->last().text = textAlternative;
      } else {
        return textAlternative;
      }
    }
  }

  // Step 2H from: http://www.w3.org/TR/accname-aam-1.1
  nameFrom = AXNameFromTitle;
  if (nameSources) {
    nameSources->append(NameSource(foundTextAlternative, titleAttr));
    nameSources->last().type = nameFrom;
  }
  const AtomicString& title = getAttribute(titleAttr);
  if (!title.isEmpty()) {
    textAlternative = title;
    if (nameSources) {
      foundTextAlternative = true;
      nameSources->last().text = textAlternative;
    } else {
      return textAlternative;
    }
  }

  nameFrom = AXNameFromUninitialized;

  if (nameSources && foundTextAlternative) {
    for (size_t i = 0; i < nameSources->size(); ++i) {
      if (!(*nameSources)[i].text.isNull() && !(*nameSources)[i].superseded) {
        NameSource& nameSource = (*nameSources)[i];
        nameFrom = nameSource.type;
        if (!nameSource.relatedObjects.isEmpty())
          *relatedObjects = nameSource.relatedObjects;
        return nameSource.text;
      }
    }
  }

  return String();
}

String AXNodeObject::textFromDescendants(AXObjectSet& visited,
                                         bool recursive) const {
  if (!canHaveChildren() && recursive)
    return String();

  StringBuilder accumulatedText;
  AXObject* previous = nullptr;

  AXObjectVector children;

  HeapVector<Member<AXObject>> ownedChildren;
  computeAriaOwnsChildren(ownedChildren);
  for (AXObject* obj = rawFirstChild(); obj; obj = obj->rawNextSibling()) {
    if (!axObjectCache().isAriaOwned(obj))
      children.append(obj);
  }
  for (const auto& ownedChild : ownedChildren)
    children.append(ownedChild);

  for (AXObject* child : children) {
    // Don't recurse into children that are explicitly marked as aria-hidden.
    // Note that we don't call isInertOrAriaHidden because that would return
    // true if any ancestor is hidden, but we need to be able to compute the
    // accessible name of object inside hidden subtrees (for example, if
    // aria-labelledby points to an object that's hidden).
    if (equalIgnoringCase(child->getAttribute(aria_hiddenAttr), "true"))
      continue;

    // If we're going between two layoutObjects that are in separate
    // LayoutBoxes, add whitespace if it wasn't there already. Intuitively if
    // you have <span>Hello</span><span>World</span>, those are part of the same
    // LayoutBox so we should return "HelloWorld", but given
    // <div>Hello</div><div>World</div> the strings are in separate boxes so we
    // should return "Hello World".
    if (previous && accumulatedText.length() &&
        !isHTMLSpace(accumulatedText[accumulatedText.length() - 1])) {
      if (!isInSameNonInlineBlockFlow(child->getLayoutObject(),
                                      previous->getLayoutObject()))
        accumulatedText.append(' ');
    }

    String result;
    if (child->isPresentational())
      result = child->textFromDescendants(visited, true);
    else
      result = recursiveTextAlternative(*child, false, visited);
    accumulatedText.append(result);
    previous = child;
  }

  return accumulatedText.toString();
}

bool AXNodeObject::nameFromLabelElement() const {
  // This unfortunately duplicates a bit of logic from textAlternative and
  // nativeTextAlternative, but it's necessary because nameFromLabelElement
  // needs to be called from computeAccessibilityIsIgnored, which isn't allowed
  // to call axObjectCache->getOrCreate.

  if (!getNode() && !getLayoutObject())
    return false;

  // Step 2A from: http://www.w3.org/TR/accname-aam-1.1
  if (isHiddenForTextAlternativeCalculation())
    return false;

  // Step 2B from: http://www.w3.org/TR/accname-aam-1.1
  HeapVector<Member<Element>> elements;
  ariaLabelledbyElementVector(elements);
  if (elements.size() > 0)
    return false;

  // Step 2C from: http://www.w3.org/TR/accname-aam-1.1
  const AtomicString& ariaLabel = getAttribute(aria_labelAttr);
  if (!ariaLabel.isEmpty())
    return false;

  // Based on
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
  // 5.1/5.5 Text inputs, Other labelable Elements
  HTMLElement* htmlElement = nullptr;
  if (getNode()->isHTMLElement())
    htmlElement = toHTMLElement(getNode());
  if (htmlElement && isLabelableElement(htmlElement)) {
    if (toLabelableElement(htmlElement)->labels() &&
        toLabelableElement(htmlElement)->labels()->length() > 0)
      return true;
  }

  return false;
}

void AXNodeObject::getRelativeBounds(AXObject** outContainer,
                                     FloatRect& outBoundsInContainer,
                                     SkMatrix44& outContainerTransform) const {
  if (layoutObjectForRelativeBounds()) {
    AXObject::getRelativeBounds(outContainer, outBoundsInContainer,
                                outContainerTransform);
    return;
  }

  *outContainer = nullptr;
  outBoundsInContainer = FloatRect();
  outContainerTransform.setIdentity();

  // First check if it has explicit bounds, for example if this element is tied
  // to a canvas path. When explicit coordinates are provided, the ID of the
  // explicit container element that the coordinates are relative to must be
  // provided too.
  if (!m_explicitElementRect.isEmpty()) {
    *outContainer = axObjectCache().objectFromAXID(m_explicitContainerID);
    if (*outContainer) {
      outBoundsInContainer = FloatRect(m_explicitElementRect);
      return;
    }
  }

  // If it's in a canvas but doesn't have an explicit rect, get the bounding
  // rect of its children.
  if (getNode()->parentElement()->isInCanvasSubtree()) {
    Vector<FloatRect> rects;
    for (Node& child : NodeTraversal::childrenOf(*getNode())) {
      if (child.isHTMLElement()) {
        if (AXObject* obj = axObjectCache().get(&child)) {
          AXObject* container;
          FloatRect bounds;
          obj->getRelativeBounds(&container, bounds, outContainerTransform);
          if (container) {
            *outContainer = container;
            rects.append(bounds);
          }
        }
      }
    }

    if (*outContainer) {
      outBoundsInContainer = unionRect(rects);
      return;
    }
  }

  // If this object doesn't have an explicit element rect or computable from its
  // children, for now, let's return the position of the ancestor that does have
  // a position, and make it the width of that parent, and about the height of a
  // line of text, so that it's clear the object is a child of the parent.
  for (AXObject* positionProvider = parentObject(); positionProvider;
       positionProvider = positionProvider->parentObject()) {
    if (positionProvider->isAXLayoutObject()) {
      positionProvider->getRelativeBounds(outContainer, outBoundsInContainer,
                                          outContainerTransform);
      if (*outContainer)
        outBoundsInContainer.setSize(
            FloatSize(outBoundsInContainer.width(),
                      std::min(10.0f, outBoundsInContainer.height())));
      break;
    }
  }
}

static Node* getParentNodeForComputeParent(Node* node) {
  if (!node)
    return nullptr;

  Node* parentNode = nullptr;

  // Skip over <optgroup> and consider the <select> the immediate parent of an
  // <option>.
  if (isHTMLOptionElement(node))
    parentNode = toHTMLOptionElement(node)->ownerSelectElement();

  if (!parentNode)
    parentNode = node->parentNode();

  return parentNode;
}

AXObject* AXNodeObject::computeParent() const {
  ASSERT(!isDetached());
  if (Node* parentNode = getParentNodeForComputeParent(getNode()))
    return axObjectCache().getOrCreate(parentNode);

  return nullptr;
}

AXObject* AXNodeObject::computeParentIfExists() const {
  if (Node* parentNode = getParentNodeForComputeParent(getNode()))
    return axObjectCache().get(parentNode);

  return nullptr;
}

AXObject* AXNodeObject::rawFirstChild() const {
  if (!getNode())
    return 0;

  Node* firstChild = getNode()->firstChild();

  if (!firstChild)
    return 0;

  return axObjectCache().getOrCreate(firstChild);
}

AXObject* AXNodeObject::rawNextSibling() const {
  if (!getNode())
    return 0;

  Node* nextSibling = getNode()->nextSibling();
  if (!nextSibling)
    return 0;

  return axObjectCache().getOrCreate(nextSibling);
}

void AXNodeObject::addChildren() {
  ASSERT(!isDetached());
  // If the need to add more children in addition to existing children arises,
  // childrenChanged should have been called, leaving the object with no
  // children.
  ASSERT(!m_haveChildren);

  if (!m_node)
    return;

  m_haveChildren = true;

  // The only time we add children from the DOM tree to a node with a
  // layoutObject is when it's a canvas.
  if (getLayoutObject() && !isHTMLCanvasElement(*m_node))
    return;

  HeapVector<Member<AXObject>> ownedChildren;
  computeAriaOwnsChildren(ownedChildren);

  for (Node& child : NodeTraversal::childrenOf(*m_node)) {
    AXObject* childObj = axObjectCache().getOrCreate(&child);
    if (childObj && !axObjectCache().isAriaOwned(childObj))
      addChild(childObj);
  }

  for (const auto& ownedChild : ownedChildren)
    addChild(ownedChild);

  for (const auto& child : m_children)
    child->setParent(this);
}

void AXNodeObject::addChild(AXObject* child) {
  insertChild(child, m_children.size());
}

void AXNodeObject::insertChild(AXObject* child, unsigned index) {
  if (!child)
    return;

  // If the parent is asking for this child's children, then either it's the
  // first time (and clearing is a no-op), or its visibility has changed. In the
  // latter case, this child may have a stale child cached.  This can prevent
  // aria-hidden changes from working correctly. Hence, whenever a parent is
  // getting children, ensure data is not stale.
  child->clearChildren();

  if (child->accessibilityIsIgnored()) {
    const auto& children = child->children();
    size_t length = children.size();
    for (size_t i = 0; i < length; ++i)
      m_children.insert(index + i, children[i]);
  } else {
    ASSERT(child->parentObject() == this);
    m_children.insert(index, child);
  }
}

bool AXNodeObject::canHaveChildren() const {
  // If this is an AXLayoutObject, then it's okay if this object
  // doesn't have a node - there are some layoutObjects that don't have
  // associated nodes, like scroll areas and css-generated text.
  if (!getNode() && !isAXLayoutObject())
    return false;

  if (getNode() && isHTMLMapElement(getNode()))
    return false;

  AccessibilityRole role = roleValue();

  // If an element has an ARIA role of presentation, we need to consider the
  // native role when deciding whether it can have children or not - otherwise
  // giving something a role of presentation could expose inner implementation
  // details.
  if (isPresentational())
    role = nativeAccessibilityRoleIgnoringAria();

  switch (role) {
    case ImageRole:
    case ButtonRole:
    case PopUpButtonRole:
    case CheckBoxRole:
    case RadioButtonRole:
    case SwitchRole:
    case TabRole:
    case ToggleButtonRole:
    case ListBoxOptionRole:
    case ScrollBarRole:
      return false;
    case StaticTextRole:
      if (!axObjectCache().inlineTextBoxAccessibilityEnabled())
        return false;
    default:
      return true;
  }
}

Element* AXNodeObject::actionElement() const {
  Node* node = this->getNode();
  if (!node)
    return 0;

  if (isHTMLInputElement(*node)) {
    HTMLInputElement& input = toHTMLInputElement(*node);
    if (!input.isDisabledFormControl() &&
        (isCheckboxOrRadio() || input.isTextButton() ||
         input.type() == InputTypeNames::file))
      return &input;
  } else if (isHTMLButtonElement(*node)) {
    return toElement(node);
  }

  if (AXObject::isARIAInput(ariaRoleAttribute()))
    return toElement(node);

  if (isImageButton())
    return toElement(node);

  if (isHTMLSelectElement(*node))
    return toElement(node);

  switch (roleValue()) {
    case ButtonRole:
    case PopUpButtonRole:
    case ToggleButtonRole:
    case TabRole:
    case MenuItemRole:
    case MenuItemCheckBoxRole:
    case MenuItemRadioRole:
      return toElement(node);
    default:
      break;
  }

  Element* anchor = anchorElement();
  Element* clickElement = mouseButtonListener();
  if (!anchor || (clickElement && clickElement->isDescendantOf(anchor)))
    return clickElement;
  return anchor;
}

Element* AXNodeObject::anchorElement() const {
  Node* node = this->getNode();
  if (!node)
    return 0;

  AXObjectCacheImpl& cache = axObjectCache();

  // search up the DOM tree for an anchor element
  // NOTE: this assumes that any non-image with an anchor is an
  // HTMLAnchorElement
  for (; node; node = node->parentNode()) {
    if (isHTMLAnchorElement(*node) ||
        (node->layoutObject() &&
         cache.getOrCreate(node->layoutObject())->isAnchor()))
      return toElement(node);
  }

  return 0;
}

Document* AXNodeObject::getDocument() const {
  if (!getNode())
    return 0;
  return &getNode()->document();
}

void AXNodeObject::setNode(Node* node) {
  m_node = node;
}

AXObject* AXNodeObject::correspondingControlForLabelElement() const {
  HTMLLabelElement* labelElement = labelElementContainer();
  if (!labelElement)
    return 0;

  HTMLElement* correspondingControl = labelElement->control();
  if (!correspondingControl)
    return 0;

  // Make sure the corresponding control isn't a descendant of this label
  // that's in the middle of being destroyed.
  if (correspondingControl->layoutObject() &&
      !correspondingControl->layoutObject()->parent())
    return 0;

  return axObjectCache().getOrCreate(correspondingControl);
}

HTMLLabelElement* AXNodeObject::labelElementContainer() const {
  if (!getNode())
    return 0;

  // the control element should not be considered part of the label
  if (isControl())
    return 0;

  // the link element should not be considered part of the label
  if (isLink())
    return 0;

  // find if this has a ancestor that is a label
  return Traversal<HTMLLabelElement>::firstAncestorOrSelf(*getNode());
}

void AXNodeObject::setFocused(bool on) {
  if (!canSetFocusAttribute())
    return;

  Document* document = this->getDocument();
  if (!on) {
    document->clearFocusedElement();
  } else {
    Node* node = this->getNode();
    if (node && node->isElementNode()) {
      // If this node is already the currently focused node, then calling
      // focus() won't do anything.  That is a problem when focus is removed
      // from the webpage to chrome, and then returns.  In these cases, we need
      // to do what keyboard and mouse focus do, which is reset focus first.
      if (document->focusedElement() == node)
        document->clearFocusedElement();

      toElement(node)->focus();
    } else {
      document->clearFocusedElement();
    }
  }
}

void AXNodeObject::increment() {
  UserGestureIndicator gestureIndicator(DocumentUserGestureToken::create(
      getDocument(), UserGestureToken::NewGesture));
  alterSliderValue(true);
}

void AXNodeObject::decrement() {
  UserGestureIndicator gestureIndicator(DocumentUserGestureToken::create(
      getDocument(), UserGestureToken::NewGesture));
  alterSliderValue(false);
}

void AXNodeObject::setSequentialFocusNavigationStartingPoint() {
  if (!getNode())
    return;

  getNode()->document().clearFocusedElement();
  getNode()->document().setSequentialFocusNavigationStartingPoint(getNode());
}

void AXNodeObject::childrenChanged() {
  // This method is meant as a quick way of marking a portion of the
  // accessibility tree dirty.
  if (!getNode() && !getLayoutObject())
    return;

  // If this is not part of the accessibility tree because an ancestor
  // has only presentational children, invalidate this object's children but
  // skip sending a notification and skip walking up the ancestors.
  if (ancestorForWhichThisIsAPresentationalChild()) {
    setNeedsToUpdateChildren();
    return;
  }

  axObjectCache().postNotification(this, AXObjectCacheImpl::AXChildrenChanged);

  // Go up the accessibility parent chain, but only if the element already
  // exists. This method is called during layout, minimal work should be done.
  // If AX elements are created now, they could interrogate the layout tree
  // while it's in a funky state.  At the same time, process ARIA live region
  // changes.
  for (AXObject* parent = this; parent;
       parent = parent->parentObjectIfExists()) {
    parent->setNeedsToUpdateChildren();

    // These notifications always need to be sent because screenreaders are
    // reliant on them to perform.  In other words, they need to be sent even
    // when the screen reader has not accessed this live region since the last
    // update.

    // If this element supports ARIA live regions, then notify the AT of
    // changes.
    if (parent->isLiveRegion())
      axObjectCache().postNotification(parent,
                                       AXObjectCacheImpl::AXLiveRegionChanged);

    // If this element is an ARIA text box or content editable, post a "value
    // changed" notification on it so that it behaves just like a native input
    // element or textarea.
    if (isNonNativeTextControl())
      axObjectCache().postNotification(parent,
                                       AXObjectCacheImpl::AXValueChanged);
  }
}

void AXNodeObject::selectionChanged() {
  // Post the selected text changed event on the first ancestor that's
  // focused (to handle form controls, ARIA text boxes and contentEditable),
  // or the web area if the selection is just in the document somewhere.
  if (isFocused() || isWebArea()) {
    axObjectCache().postNotification(this,
                                     AXObjectCacheImpl::AXSelectedTextChanged);
    if (getDocument()) {
      AXObject* documentObject = axObjectCache().getOrCreate(getDocument());
      axObjectCache().postNotification(
          documentObject, AXObjectCacheImpl::AXDocumentSelectionChanged);
    }
  } else {
    AXObject::selectionChanged();  // Calls selectionChanged on parent.
  }
}

void AXNodeObject::textChanged() {
  // If this element supports ARIA live regions, or is part of a region with an
  // ARIA editable role, then notify the AT of changes.
  AXObjectCacheImpl& cache = axObjectCache();
  for (Node* parentNode = getNode(); parentNode;
       parentNode = parentNode->parentNode()) {
    AXObject* parent = cache.get(parentNode);
    if (!parent)
      continue;

    if (parent->isLiveRegion())
      cache.postNotification(parentNode,
                             AXObjectCacheImpl::AXLiveRegionChanged);

    // If this element is an ARIA text box or content editable, post a "value
    // changed" notification on it so that it behaves just like a native input
    // element or textarea.
    if (parent->isNonNativeTextControl())
      cache.postNotification(parentNode, AXObjectCacheImpl::AXValueChanged);
  }
}

void AXNodeObject::updateAccessibilityRole() {
  bool ignoredStatus = accessibilityIsIgnored();
  m_role = determineAccessibilityRole();

  // The AX hierarchy only needs to be updated if the ignored status of an
  // element has changed.
  if (ignoredStatus != accessibilityIsIgnored())
    childrenChanged();
}

void AXNodeObject::computeAriaOwnsChildren(
    HeapVector<Member<AXObject>>& ownedChildren) const {
  if (!hasAttribute(aria_ownsAttr))
    return;

  Vector<String> idVector;
  if (canHaveChildren() && !isNativeTextControl() &&
      !hasContentEditableAttributeSet())
    tokenVectorFromAttribute(idVector, aria_ownsAttr);

  axObjectCache().updateAriaOwns(this, idVector, ownedChildren);
}

// Based on
// http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
String AXNodeObject::nativeTextAlternative(
    AXObjectSet& visited,
    AXNameFrom& nameFrom,
    AXRelatedObjectVector* relatedObjects,
    NameSources* nameSources,
    bool* foundTextAlternative) const {
  if (!getNode())
    return String();

  // If nameSources is non-null, relatedObjects is used in filling it in, so it
  // must be non-null as well.
  if (nameSources)
    ASSERT(relatedObjects);

  String textAlternative;
  AXRelatedObjectVector localRelatedObjects;

  const HTMLInputElement* inputElement = nullptr;
  if (isHTMLInputElement(getNode()))
    inputElement = toHTMLInputElement(getNode());

  // 5.1/5.5 Text inputs, Other labelable Elements
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  HTMLElement* htmlElement = nullptr;
  if (getNode()->isHTMLElement())
    htmlElement = toHTMLElement(getNode());

  if (htmlElement && htmlElement->isLabelable()) {
    nameFrom = AXNameFromRelatedElement;
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative));
      nameSources->last().type = nameFrom;
      nameSources->last().nativeSource = AXTextFromNativeHTMLLabel;
    }

    LabelsNodeList* labels = toLabelableElement(htmlElement)->labels();
    if (labels && labels->length() > 0) {
      HeapVector<Member<Element>> labelElements;
      for (unsigned labelIndex = 0; labelIndex < labels->length();
           ++labelIndex) {
        Element* label = labels->item(labelIndex);
        if (nameSources) {
          if (label->getAttribute(forAttr) == htmlElement->getIdAttribute())
            nameSources->last().nativeSource = AXTextFromNativeHTMLLabelFor;
          else
            nameSources->last().nativeSource = AXTextFromNativeHTMLLabelWrapped;
        }
        labelElements.append(label);
      }

      textAlternative =
          textFromElements(false, visited, labelElements, relatedObjects);
      if (!textAlternative.isNull()) {
        *foundTextAlternative = true;
        if (nameSources) {
          NameSource& source = nameSources->last();
          source.relatedObjects = *relatedObjects;
          source.text = textAlternative;
        } else {
          return textAlternative;
        }
      } else if (nameSources) {
        nameSources->last().invalid = true;
      }
    }
  }

  // 5.2 input type="button", input type="submit" and input type="reset"
  if (inputElement && inputElement->isTextButton()) {
    // value attribue
    nameFrom = AXNameFromValue;
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative, valueAttr));
      nameSources->last().type = nameFrom;
    }
    String value = inputElement->value();
    if (!value.isNull()) {
      textAlternative = value;
      if (nameSources) {
        NameSource& source = nameSources->last();
        source.text = textAlternative;
        *foundTextAlternative = true;
      } else {
        return textAlternative;
      }
    }
    return textAlternative;
  }

  // 5.3 input type="image"
  if (inputElement &&
      inputElement->getAttribute(typeAttr) == InputTypeNames::image) {
    // alt attr
    nameFrom = AXNameFromAttribute;
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative, altAttr));
      nameSources->last().type = nameFrom;
    }
    const AtomicString& alt = inputElement->getAttribute(altAttr);
    if (!alt.isNull()) {
      textAlternative = alt;
      if (nameSources) {
        NameSource& source = nameSources->last();
        source.attributeValue = alt;
        source.text = textAlternative;
        *foundTextAlternative = true;
      } else {
        return textAlternative;
      }
    }

    // value attr
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative, valueAttr));
      nameSources->last().type = nameFrom;
    }
    nameFrom = AXNameFromAttribute;
    String value = inputElement->value();
    if (!value.isNull()) {
      textAlternative = value;
      if (nameSources) {
        NameSource& source = nameSources->last();
        source.text = textAlternative;
        *foundTextAlternative = true;
      } else {
        return textAlternative;
      }
    }

    // localised default value ("Submit")
    nameFrom = AXNameFromValue;
    textAlternative = inputElement->locale().queryString(
        WebLocalizedString::SubmitButtonDefaultLabel);
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative, typeAttr));
      NameSource& source = nameSources->last();
      source.attributeValue = inputElement->getAttribute(typeAttr);
      source.type = nameFrom;
      source.text = textAlternative;
      *foundTextAlternative = true;
    } else {
      return textAlternative;
    }
    return textAlternative;
  }

  // 5.1 Text inputs - step 3 (placeholder attribute)
  if (htmlElement && htmlElement->isTextControl()) {
    nameFrom = AXNameFromPlaceholder;
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative, placeholderAttr));
      NameSource& source = nameSources->last();
      source.type = nameFrom;
    }
    HTMLElement* element = toHTMLElement(getNode());
    const AtomicString& placeholder =
        element->fastGetAttribute(placeholderAttr);
    if (!placeholder.isEmpty()) {
      textAlternative = placeholder;
      if (nameSources) {
        NameSource& source = nameSources->last();
        source.text = textAlternative;
        source.attributeValue = placeholder;
      } else {
        return textAlternative;
      }
    }
    return textAlternative;
  }

  // 5.7 figure and figcaption Elements
  if (getNode()->hasTagName(figureTag)) {
    // figcaption
    nameFrom = AXNameFromRelatedElement;
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative));
      nameSources->last().type = nameFrom;
      nameSources->last().nativeSource = AXTextFromNativeHTMLFigcaption;
    }
    Element* figcaption = nullptr;
    for (Element& element : ElementTraversal::descendantsOf(*(getNode()))) {
      if (element.hasTagName(figcaptionTag)) {
        figcaption = &element;
        break;
      }
    }
    if (figcaption) {
      AXObject* figcaptionAXObject = axObjectCache().getOrCreate(figcaption);
      if (figcaptionAXObject) {
        textAlternative =
            recursiveTextAlternative(*figcaptionAXObject, false, visited);

        if (relatedObjects) {
          localRelatedObjects.append(
              new NameSourceRelatedObject(figcaptionAXObject, textAlternative));
          *relatedObjects = localRelatedObjects;
          localRelatedObjects.clear();
        }

        if (nameSources) {
          NameSource& source = nameSources->last();
          source.relatedObjects = *relatedObjects;
          source.text = textAlternative;
          *foundTextAlternative = true;
        } else {
          return textAlternative;
        }
      }
    }
    return textAlternative;
  }

  // 5.8 img or area Element
  if (isHTMLImageElement(getNode()) || isHTMLAreaElement(getNode()) ||
      (getLayoutObject() && getLayoutObject()->isSVGImage())) {
    // alt
    nameFrom = AXNameFromAttribute;
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative, altAttr));
      nameSources->last().type = nameFrom;
    }
    const AtomicString& alt = getAttribute(altAttr);
    if (!alt.isNull()) {
      textAlternative = alt;
      if (nameSources) {
        NameSource& source = nameSources->last();
        source.attributeValue = alt;
        source.text = textAlternative;
        *foundTextAlternative = true;
      } else {
        return textAlternative;
      }
    }
    return textAlternative;
  }

  // 5.9 table Element
  if (isHTMLTableElement(getNode())) {
    HTMLTableElement* tableElement = toHTMLTableElement(getNode());

    // caption
    nameFrom = AXNameFromCaption;
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative));
      nameSources->last().type = nameFrom;
      nameSources->last().nativeSource = AXTextFromNativeHTMLTableCaption;
    }
    HTMLTableCaptionElement* caption = tableElement->caption();
    if (caption) {
      AXObject* captionAXObject = axObjectCache().getOrCreate(caption);
      if (captionAXObject) {
        textAlternative =
            recursiveTextAlternative(*captionAXObject, false, visited);
        if (relatedObjects) {
          localRelatedObjects.append(
              new NameSourceRelatedObject(captionAXObject, textAlternative));
          *relatedObjects = localRelatedObjects;
          localRelatedObjects.clear();
        }

        if (nameSources) {
          NameSource& source = nameSources->last();
          source.relatedObjects = *relatedObjects;
          source.text = textAlternative;
          *foundTextAlternative = true;
        } else {
          return textAlternative;
        }
      }
    }

    // summary
    nameFrom = AXNameFromAttribute;
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative, summaryAttr));
      nameSources->last().type = nameFrom;
    }
    const AtomicString& summary = getAttribute(summaryAttr);
    if (!summary.isNull()) {
      textAlternative = summary;
      if (nameSources) {
        NameSource& source = nameSources->last();
        source.attributeValue = summary;
        source.text = textAlternative;
        *foundTextAlternative = true;
      } else {
        return textAlternative;
      }
    }

    return textAlternative;
  }

  // Per SVG AAM 1.0's modifications to 2D of this algorithm.
  if (getNode()->isSVGElement()) {
    nameFrom = AXNameFromRelatedElement;
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative));
      nameSources->last().type = nameFrom;
      nameSources->last().nativeSource = AXTextFromNativeHTMLTitleElement;
    }
    ASSERT(getNode()->isContainerNode());
    Element* title = ElementTraversal::firstChild(
        toContainerNode(*(getNode())), HasTagName(SVGNames::titleTag));

    if (title) {
      AXObject* titleAXObject = axObjectCache().getOrCreate(title);
      if (titleAXObject && !visited.contains(titleAXObject)) {
        textAlternative =
            recursiveTextAlternative(*titleAXObject, false, visited);
        if (relatedObjects) {
          localRelatedObjects.append(
              new NameSourceRelatedObject(titleAXObject, textAlternative));
          *relatedObjects = localRelatedObjects;
          localRelatedObjects.clear();
        }
      }
      if (nameSources) {
        NameSource& source = nameSources->last();
        source.text = textAlternative;
        source.relatedObjects = *relatedObjects;
        *foundTextAlternative = true;
      } else {
        return textAlternative;
      }
    }
  }

  // Fieldset / legend.
  if (isHTMLFieldSetElement(getNode())) {
    nameFrom = AXNameFromRelatedElement;
    if (nameSources) {
      nameSources->append(NameSource(*foundTextAlternative));
      nameSources->last().type = nameFrom;
      nameSources->last().nativeSource = AXTextFromNativeHTMLLegend;
    }
    HTMLElement* legend = toHTMLFieldSetElement(getNode())->legend();
    if (legend) {
      AXObject* legendAXObject = axObjectCache().getOrCreate(legend);
      // Avoid an infinite loop
      if (legendAXObject && !visited.contains(legendAXObject)) {
        textAlternative =
            recursiveTextAlternative(*legendAXObject, false, visited);

        if (relatedObjects) {
          localRelatedObjects.append(
              new NameSourceRelatedObject(legendAXObject, textAlternative));
          *relatedObjects = localRelatedObjects;
          localRelatedObjects.clear();
        }

        if (nameSources) {
          NameSource& source = nameSources->last();
          source.relatedObjects = *relatedObjects;
          source.text = textAlternative;
          *foundTextAlternative = true;
        } else {
          return textAlternative;
        }
      }
    }
  }

  // Document.
  if (isWebArea()) {
    Document* document = this->getDocument();
    if (document) {
      nameFrom = AXNameFromAttribute;
      if (nameSources) {
        nameSources->append(NameSource(foundTextAlternative, aria_labelAttr));
        nameSources->last().type = nameFrom;
      }
      if (Element* documentElement = document->documentElement()) {
        const AtomicString& ariaLabel =
            documentElement->getAttribute(aria_labelAttr);
        if (!ariaLabel.isEmpty()) {
          textAlternative = ariaLabel;

          if (nameSources) {
            NameSource& source = nameSources->last();
            source.text = textAlternative;
            source.attributeValue = ariaLabel;
            *foundTextAlternative = true;
          } else {
            return textAlternative;
          }
        }
      }

      nameFrom = AXNameFromRelatedElement;
      if (nameSources) {
        nameSources->append(NameSource(*foundTextAlternative));
        nameSources->last().type = nameFrom;
        nameSources->last().nativeSource = AXTextFromNativeHTMLTitleElement;
      }

      textAlternative = document->title();

      Element* titleElement = document->titleElement();
      AXObject* titleAXObject = axObjectCache().getOrCreate(titleElement);
      if (titleAXObject) {
        if (relatedObjects) {
          localRelatedObjects.append(
              new NameSourceRelatedObject(titleAXObject, textAlternative));
          *relatedObjects = localRelatedObjects;
          localRelatedObjects.clear();
        }

        if (nameSources) {
          NameSource& source = nameSources->last();
          source.relatedObjects = *relatedObjects;
          source.text = textAlternative;
          *foundTextAlternative = true;
        } else {
          return textAlternative;
        }
      }
    }
  }

  return textAlternative;
}

String AXNodeObject::description(AXNameFrom nameFrom,
                                 AXDescriptionFrom& descriptionFrom,
                                 AXObjectVector* descriptionObjects) const {
  AXRelatedObjectVector relatedObjects;
  String result =
      description(nameFrom, descriptionFrom, nullptr, &relatedObjects);
  if (descriptionObjects) {
    descriptionObjects->clear();
    for (size_t i = 0; i < relatedObjects.size(); i++)
      descriptionObjects->append(relatedObjects[i]->object);
  }

  return collapseWhitespace(result);
}

// Based on
// http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
String AXNodeObject::description(AXNameFrom nameFrom,
                                 AXDescriptionFrom& descriptionFrom,
                                 DescriptionSources* descriptionSources,
                                 AXRelatedObjectVector* relatedObjects) const {
  // If descriptionSources is non-null, relatedObjects is used in filling it in,
  // so it must be non-null as well.
  if (descriptionSources)
    ASSERT(relatedObjects);

  if (!getNode())
    return String();

  String description;
  bool foundDescription = false;

  descriptionFrom = AXDescriptionFromRelatedElement;
  if (descriptionSources) {
    descriptionSources->append(
        DescriptionSource(foundDescription, aria_describedbyAttr));
    descriptionSources->last().type = descriptionFrom;
  }

  // aria-describedby overrides any other accessible description, from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  const AtomicString& ariaDescribedby = getAttribute(aria_describedbyAttr);
  if (!ariaDescribedby.isNull()) {
    if (descriptionSources)
      descriptionSources->last().attributeValue = ariaDescribedby;

    description = textFromAriaDescribedby(relatedObjects);

    if (!description.isNull()) {
      if (descriptionSources) {
        DescriptionSource& source = descriptionSources->last();
        source.type = descriptionFrom;
        source.relatedObjects = *relatedObjects;
        source.text = description;
        foundDescription = true;
      } else {
        return description;
      }
    } else if (descriptionSources) {
      descriptionSources->last().invalid = true;
    }
  }

  HTMLElement* htmlElement = nullptr;
  if (getNode()->isHTMLElement())
    htmlElement = toHTMLElement(getNode());

  // placeholder, 5.1.2 from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (nameFrom != AXNameFromPlaceholder && htmlElement &&
      htmlElement->isTextControl()) {
    descriptionFrom = AXDescriptionFromPlaceholder;
    if (descriptionSources) {
      descriptionSources->append(
          DescriptionSource(foundDescription, placeholderAttr));
      DescriptionSource& source = descriptionSources->last();
      source.type = descriptionFrom;
    }
    HTMLElement* element = toHTMLElement(getNode());
    const AtomicString& placeholder =
        element->fastGetAttribute(placeholderAttr);
    if (!placeholder.isEmpty()) {
      description = placeholder;
      if (descriptionSources) {
        DescriptionSource& source = descriptionSources->last();
        source.text = description;
        source.attributeValue = placeholder;
        foundDescription = true;
      } else {
        return description;
      }
    }
  }

  const HTMLInputElement* inputElement = nullptr;
  if (isHTMLInputElement(getNode()))
    inputElement = toHTMLInputElement(getNode());

  // value, 5.2.2 from: http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (nameFrom != AXNameFromValue && inputElement &&
      inputElement->isTextButton()) {
    descriptionFrom = AXDescriptionFromAttribute;
    if (descriptionSources) {
      descriptionSources->append(
          DescriptionSource(foundDescription, valueAttr));
      descriptionSources->last().type = descriptionFrom;
    }
    String value = inputElement->value();
    if (!value.isNull()) {
      description = value;
      if (descriptionSources) {
        DescriptionSource& source = descriptionSources->last();
        source.text = description;
        foundDescription = true;
      } else {
        return description;
      }
    }
  }

  // table caption, 5.9.2 from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (nameFrom != AXNameFromCaption && isHTMLTableElement(getNode())) {
    HTMLTableElement* tableElement = toHTMLTableElement(getNode());

    descriptionFrom = AXDescriptionFromRelatedElement;
    if (descriptionSources) {
      descriptionSources->append(DescriptionSource(foundDescription));
      descriptionSources->last().type = descriptionFrom;
      descriptionSources->last().nativeSource =
          AXTextFromNativeHTMLTableCaption;
    }
    HTMLTableCaptionElement* caption = tableElement->caption();
    if (caption) {
      AXObject* captionAXObject = axObjectCache().getOrCreate(caption);
      if (captionAXObject) {
        AXObjectSet visited;
        description =
            recursiveTextAlternative(*captionAXObject, false, visited);
        if (relatedObjects)
          relatedObjects->append(
              new NameSourceRelatedObject(captionAXObject, description));

        if (descriptionSources) {
          DescriptionSource& source = descriptionSources->last();
          source.relatedObjects = *relatedObjects;
          source.text = description;
          foundDescription = true;
        } else {
          return description;
        }
      }
    }
  }

  // summary, 5.6.2 from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (nameFrom != AXNameFromContents && isHTMLSummaryElement(getNode())) {
    descriptionFrom = AXDescriptionFromContents;
    if (descriptionSources) {
      descriptionSources->append(DescriptionSource(foundDescription));
      descriptionSources->last().type = descriptionFrom;
    }

    AXObjectSet visited;
    description = textFromDescendants(visited, false);

    if (!description.isEmpty()) {
      if (descriptionSources) {
        foundDescription = true;
        descriptionSources->last().text = description;
      } else {
        return description;
      }
    }
  }

  // title attribute, from:
  // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
  if (nameFrom != AXNameFromTitle) {
    descriptionFrom = AXDescriptionFromAttribute;
    if (descriptionSources) {
      descriptionSources->append(
          DescriptionSource(foundDescription, titleAttr));
      descriptionSources->last().type = descriptionFrom;
    }
    const AtomicString& title = getAttribute(titleAttr);
    if (!title.isEmpty()) {
      description = title;
      if (descriptionSources) {
        foundDescription = true;
        descriptionSources->last().text = description;
      } else {
        return description;
      }
    }
  }

  // aria-help.
  // FIXME: this is not part of the official standard, but it's needed because
  // the built-in date/time controls use it.
  descriptionFrom = AXDescriptionFromAttribute;
  if (descriptionSources) {
    descriptionSources->append(
        DescriptionSource(foundDescription, aria_helpAttr));
    descriptionSources->last().type = descriptionFrom;
  }
  const AtomicString& help = getAttribute(aria_helpAttr);
  if (!help.isEmpty()) {
    description = help;
    if (descriptionSources) {
      foundDescription = true;
      descriptionSources->last().text = description;
    } else {
      return description;
    }
  }

  descriptionFrom = AXDescriptionFromUninitialized;

  if (foundDescription) {
    for (size_t i = 0; i < descriptionSources->size(); ++i) {
      if (!(*descriptionSources)[i].text.isNull() &&
          !(*descriptionSources)[i].superseded) {
        DescriptionSource& descriptionSource = (*descriptionSources)[i];
        descriptionFrom = descriptionSource.type;
        if (!descriptionSource.relatedObjects.isEmpty())
          *relatedObjects = descriptionSource.relatedObjects;
        return descriptionSource.text;
      }
    }
  }

  return String();
}

String AXNodeObject::placeholder(AXNameFrom nameFrom,
                                 AXDescriptionFrom descriptionFrom) const {
  if (nameFrom == AXNameFromPlaceholder)
    return String();

  if (descriptionFrom == AXDescriptionFromPlaceholder)
    return String();

  if (!getNode())
    return String();

  String placeholder;
  if (isHTMLInputElement(*getNode())) {
    HTMLInputElement* inputElement = toHTMLInputElement(getNode());
    placeholder = inputElement->strippedPlaceholder();
  } else if (isHTMLTextAreaElement(*getNode())) {
    HTMLTextAreaElement* textAreaElement = toHTMLTextAreaElement(getNode());
    placeholder = textAreaElement->strippedPlaceholder();
  }
  return placeholder;
}

DEFINE_TRACE(AXNodeObject) {
  visitor->trace(m_node);
  AXObject::trace(visitor);
}

}  // namespace blink
