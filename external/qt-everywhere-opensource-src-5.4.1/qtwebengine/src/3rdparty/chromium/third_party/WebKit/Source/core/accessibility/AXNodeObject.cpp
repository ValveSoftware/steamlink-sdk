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

#include "config.h"
#include "core/accessibility/AXNodeObject.h"

#include "core/accessibility/AXObjectCache.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/Text.h"
#include "core/html/HTMLFieldSetElement.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLLegendElement.h"
#include "core/html/HTMLSelectElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/rendering/RenderObject.h"
#include "platform/UserGestureIndicator.h"
#include "wtf/text/StringBuilder.h"


namespace WebCore {

using namespace HTMLNames;

AXNodeObject::AXNodeObject(Node* node)
    : AXObject()
    , m_ariaRole(UnknownRole)
    , m_childrenDirty(false)
#ifndef NDEBUG
    , m_initialized(false)
#endif
    , m_node(node)
{
}

PassRefPtr<AXNodeObject> AXNodeObject::create(Node* node)
{
    return adoptRef(new AXNodeObject(node));
}

AXNodeObject::~AXNodeObject()
{
    ASSERT(isDetached());
}

// This function implements the ARIA accessible name as described by the Mozilla
// ARIA Implementer's Guide.
static String accessibleNameForNode(Node* node)
{
    if (!node)
        return String();

    if (node->isTextNode())
        return toText(node)->data();

    if (isHTMLInputElement(*node))
        return toHTMLInputElement(*node).value();

    if (node->isHTMLElement()) {
        const AtomicString& alt = toHTMLElement(node)->getAttribute(altAttr);
        if (!alt.isEmpty())
            return alt;
    }

    return String();
}

String AXNodeObject::accessibilityDescriptionForElements(Vector<Element*> &elements) const
{
    StringBuilder builder;
    unsigned size = elements.size();
    for (unsigned i = 0; i < size; ++i) {
        Element* idElement = elements[i];

        builder.append(accessibleNameForNode(idElement));
        for (Node* n = idElement->firstChild(); n; n = NodeTraversal::next(*n, idElement))
            builder.append(accessibleNameForNode(n));

        if (i != size - 1)
            builder.append(' ');
    }
    return builder.toString();
}

void AXNodeObject::alterSliderValue(bool increase)
{
    if (roleValue() != SliderRole)
        return;

    if (!getAttribute(stepAttr).isEmpty())
        changeValueByStep(increase);
    else
        changeValueByPercent(increase ? 5 : -5);
}

String AXNodeObject::ariaAccessibilityDescription() const
{
    String ariaLabeledBy = ariaLabeledByAttribute();
    if (!ariaLabeledBy.isEmpty())
        return ariaLabeledBy;

    const AtomicString& ariaLabel = getAttribute(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        return ariaLabel;

    return String();
}


void AXNodeObject::ariaLabeledByElements(Vector<Element*>& elements) const
{
    elementsFromAttribute(elements, aria_labeledbyAttr);
    if (!elements.size())
        elementsFromAttribute(elements, aria_labelledbyAttr);
}

void AXNodeObject::changeValueByStep(bool increase)
{
    float step = stepValueForRange();
    float value = valueForRange();

    value += increase ? step : -step;

    setValue(String::number(value));

    axObjectCache()->postNotification(node(), AXObjectCache::AXValueChanged, true);
}

bool AXNodeObject::computeAccessibilityIsIgnored() const
{
#ifndef NDEBUG
    // Double-check that an AXObject is never accessed before
    // it's been initialized.
    ASSERT(m_initialized);
#endif

    // If this element is within a parent that cannot have children, it should not be exposed.
    if (isDescendantOfBarrenParent())
        return true;

    // Ignore labels that are already referenced by a control's title UI element.
    AXObject* controlObject = correspondingControlForLabelElement();
    if (controlObject && !controlObject->exposesTitleUIElement() && controlObject->isCheckboxOrRadio())
        return true;

    return m_role == UnknownRole;
}

AccessibilityRole AXNodeObject::determineAccessibilityRole()
{
    if (!node())
        return UnknownRole;

    m_ariaRole = determineAriaRoleAttribute();

    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (ariaRole != UnknownRole)
        return ariaRole;

    if (node()->isLink())
        return LinkRole;
    if (node()->isTextNode())
        return StaticTextRole;
    if (isHTMLButtonElement(*node()))
        return buttonRoleType();
    if (isHTMLInputElement(*node())) {
        HTMLInputElement& input = toHTMLInputElement(*node());
        if (input.isCheckbox())
            return CheckBoxRole;
        if (input.isRadioButton())
            return RadioButtonRole;
        if (input.isTextButton())
            return buttonRoleType();
        if (input.isRangeControl())
            return SliderRole;

        const AtomicString& type = input.getAttribute(typeAttr);
        if (equalIgnoringCase(type, "color"))
            return ColorWellRole;

        return TextFieldRole;
    }
    if (isHTMLSelectElement(*node())) {
        HTMLSelectElement& selectElement = toHTMLSelectElement(*node());
        return selectElement.multiple() ? ListBoxRole : PopUpButtonRole;
    }
    if (isHTMLTextAreaElement(*node()))
        return TextAreaRole;
    if (headingLevel())
        return HeadingRole;
    if (isHTMLDivElement(*node()))
        return DivRole;
    if (isHTMLParagraphElement(*node()))
        return ParagraphRole;
    if (isHTMLLabelElement(*node()))
        return LabelRole;
    if (node()->isElementNode() && toElement(node())->isFocusable())
        return GroupRole;
    if (isHTMLAnchorElement(*node()) && isClickable())
        return LinkRole;
    if (node()->hasTagName(iframeTag))
        return IframeRole;
    if (isEmbeddedObject())
        return EmbeddedObjectRole;

    return UnknownRole;
}

AccessibilityRole AXNodeObject::determineAriaRoleAttribute() const
{
    const AtomicString& ariaRole = getAttribute(roleAttr);
    if (ariaRole.isNull() || ariaRole.isEmpty())
        return UnknownRole;

    AccessibilityRole role = ariaRoleToWebCoreRole(ariaRole);

    // ARIA states if an item can get focus, it should not be presentational.
    if (role == PresentationalRole && canSetFocusAttribute())
        return UnknownRole;

    if (role == ButtonRole)
        role = buttonRoleType();

    if (role == TextAreaRole && !ariaIsMultiline())
        role = TextFieldRole;

    role = remapAriaRoleDueToParent(role);

    if (role)
        return role;

    return UnknownRole;
}

void AXNodeObject::elementsFromAttribute(Vector<Element*>& elements, const QualifiedName& attribute) const
{
    Node* node = this->node();
    if (!node || !node->isElementNode())
        return;

    TreeScope& scope = node->treeScope();

    String idList = getAttribute(attribute).string();
    if (idList.isEmpty())
        return;

    idList.replace('\n', ' ');
    Vector<String> idVector;
    idList.split(' ', idVector);

    unsigned size = idVector.size();
    for (unsigned i = 0; i < size; ++i) {
        AtomicString idName(idVector[i]);
        Element* idElement = scope.getElementById(idName);
        if (idElement)
            elements.append(idElement);
    }
}

// If you call node->rendererIsEditable() since that will return true if an ancestor is editable.
// This only returns true if this is the element that actually has the contentEditable attribute set.
bool AXNodeObject::hasContentEditableAttributeSet() const
{
    if (!hasAttribute(contenteditableAttr))
        return false;
    const AtomicString& contentEditableValue = getAttribute(contenteditableAttr);
    // Both "true" (case-insensitive) and the empty string count as true.
    return contentEditableValue.isEmpty() || equalIgnoringCase(contentEditableValue, "true");
}

bool AXNodeObject::isDescendantOfBarrenParent() const
{
    for (AXObject* object = parentObject(); object; object = object->parentObject()) {
        if (!object->canHaveChildren())
            return true;
    }

    return false;
}

bool AXNodeObject::isGenericFocusableElement() const
{
    if (!canSetFocusAttribute())
        return false;

    // If it's a control, it's not generic.
    if (isControl())
        return false;

    // If it has an aria role, it's not generic.
    if (m_ariaRole != UnknownRole)
        return false;

    // If the content editable attribute is set on this element, that's the reason
    // it's focusable, and existing logic should handle this case already - so it's not a
    // generic focusable element.

    if (hasContentEditableAttributeSet())
        return false;

    // The web area and body element are both focusable, but existing logic handles these
    // cases already, so we don't need to include them here.
    if (roleValue() == WebAreaRole)
        return false;
    if (isHTMLBodyElement(node()))
        return false;

    // An SVG root is focusable by default, but it's probably not interactive, so don't
    // include it. It can still be made accessible by giving it an ARIA role.
    if (roleValue() == SVGRootRole)
        return false;

    return true;
}

HTMLLabelElement* AXNodeObject::labelForElement(Element* element) const
{
    if (!element->isHTMLElement() || !toHTMLElement(element)->isLabelable())
        return 0;

    const AtomicString& id = element->getIdAttribute();
    if (!id.isEmpty()) {
        if (HTMLLabelElement* label = element->treeScope().labelElementForId(id))
            return label;
    }

    for (Element* parent = element->parentElement(); parent; parent = parent->parentElement()) {
        if (isHTMLLabelElement(*parent))
            return toHTMLLabelElement(parent);
    }

    return 0;
}

AXObject* AXNodeObject::menuButtonForMenu() const
{
    Element* menuItem = menuItemElementForMenu();

    if (menuItem) {
        // ARIA just has generic menu items. AppKit needs to know if this is a top level items like MenuBarButton or MenuBarItem
        AXObject* menuItemAX = axObjectCache()->getOrCreate(menuItem);
        if (menuItemAX && menuItemAX->isMenuButton())
            return menuItemAX;
    }
    return 0;
}

static Element* siblingWithAriaRole(String role, Node* node)
{
    Node* parent = node->parentNode();
    if (!parent)
        return 0;

    for (Element* sibling = ElementTraversal::firstChild(*parent); sibling; sibling = ElementTraversal::nextSibling(*sibling)) {
        const AtomicString& siblingAriaRole = sibling->getAttribute(roleAttr);
        if (equalIgnoringCase(siblingAriaRole, role))
            return sibling;
    }

    return 0;
}

Element* AXNodeObject::menuItemElementForMenu() const
{
    if (ariaRoleAttribute() != MenuRole)
        return 0;

    return siblingWithAriaRole("menuitem", node());
}

Element* AXNodeObject::mouseButtonListener() const
{
    Node* node = this->node();
    if (!node)
        return 0;

    // check if our parent is a mouse button listener
    while (node && !node->isElementNode())
        node = node->parentNode();

    if (!node)
        return 0;

    // FIXME: Do the continuation search like anchorElement does
    for (Element* element = toElement(node); element; element = element->parentElement()) {
        if (element->getAttributeEventListener(EventTypeNames::click) || element->getAttributeEventListener(EventTypeNames::mousedown) || element->getAttributeEventListener(EventTypeNames::mouseup))
            return element;
    }

    return 0;
}

AccessibilityRole AXNodeObject::remapAriaRoleDueToParent(AccessibilityRole role) const
{
    // Some objects change their role based on their parent.
    // However, asking for the unignoredParent calls accessibilityIsIgnored(), which can trigger a loop.
    // While inside the call stack of creating an element, we need to avoid accessibilityIsIgnored().
    // https://bugs.webkit.org/show_bug.cgi?id=65174

    if (role != ListBoxOptionRole && role != MenuItemRole)
        return role;

    for (AXObject* parent = parentObject(); parent && !parent->accessibilityIsIgnored(); parent = parent->parentObject()) {
        AccessibilityRole parentAriaRole = parent->ariaRoleAttribute();

        // Selects and listboxes both have options as child roles, but they map to different roles within WebCore.
        if (role == ListBoxOptionRole && parentAriaRole == MenuRole)
            return MenuItemRole;
        // An aria "menuitem" may map to MenuButton or MenuItem depending on its parent.
        if (role == MenuItemRole && parentAriaRole == GroupRole)
            return MenuButtonRole;

        // If the parent had a different role, then we don't need to continue searching up the chain.
        if (parentAriaRole)
            break;
    }

    return role;
}

void AXNodeObject::init()
{
#ifndef NDEBUG
    ASSERT(!m_initialized);
    m_initialized = true;
#endif
    m_role = determineAccessibilityRole();
}

void AXNodeObject::detach()
{
    clearChildren();
    AXObject::detach();
    m_node = 0;
}

bool AXNodeObject::isAnchor() const
{
    return !isNativeImage() && isLink();
}

bool AXNodeObject::isControl() const
{
    Node* node = this->node();
    if (!node)
        return false;

    return ((node->isElementNode() && toElement(node)->isFormControlElement())
        || AXObject::isARIAControl(ariaRoleAttribute()));
}

bool AXNodeObject::isEmbeddedObject() const
{
    return node()
        && (node()->hasTagName(objectTag) || node()->hasTagName(embedTag)
        || node()->hasTagName(appletTag));
}

bool AXNodeObject::isFieldset() const
{
    return isHTMLFieldSetElement(node());
}

bool AXNodeObject::isHeading() const
{
    return roleValue() == HeadingRole;
}

bool AXNodeObject::isHovered() const
{
    Node* node = this->node();
    if (!node)
        return false;

    return node->hovered();
}

bool AXNodeObject::isImage() const
{
    return roleValue() == ImageRole;
}

bool AXNodeObject::isImageButton() const
{
    return isNativeImage() && isButton();
}

bool AXNodeObject::isInputImage() const
{
    Node* node = this->node();
    if (roleValue() == ButtonRole && isHTMLInputElement(node))
        return toHTMLInputElement(*node).isImageButton();

    return false;
}

bool AXNodeObject::isLink() const
{
    return roleValue() == LinkRole;
}

bool AXNodeObject::isMenu() const
{
    return roleValue() == MenuRole;
}

bool AXNodeObject::isMenuButton() const
{
    return roleValue() == MenuButtonRole;
}

bool AXNodeObject::isMultiSelectable() const
{
    const AtomicString& ariaMultiSelectable = getAttribute(aria_multiselectableAttr);
    if (equalIgnoringCase(ariaMultiSelectable, "true"))
        return true;
    if (equalIgnoringCase(ariaMultiSelectable, "false"))
        return false;

    return isHTMLSelectElement(node()) && toHTMLSelectElement(*node()).multiple();
}

bool AXNodeObject::isNativeCheckboxOrRadio() const
{
    Node* node = this->node();
    if (!isHTMLInputElement(node))
        return false;

    HTMLInputElement* input = toHTMLInputElement(node);
    return input->isCheckbox() || input->isRadioButton();
}

bool AXNodeObject::isNativeImage() const
{
    Node* node = this->node();
    if (!node)
        return false;

    if (isHTMLImageElement(*node))
        return true;

    if (isHTMLAppletElement(*node) || isHTMLEmbedElement(*node) || isHTMLObjectElement(*node))
        return true;

    if (isHTMLInputElement(*node))
        return toHTMLInputElement(*node).isImageButton();

    return false;
}

bool AXNodeObject::isNativeTextControl() const
{
    Node* node = this->node();
    if (!node)
        return false;

    if (isHTMLTextAreaElement(*node))
        return true;

    if (isHTMLInputElement(*node)) {
        HTMLInputElement* input = toHTMLInputElement(node);
        return input->isText() || input->isNumberField();
    }

    return false;
}

bool AXNodeObject::isNonNativeTextControl() const
{
    if (isNativeTextControl())
        return false;

    if (hasContentEditableAttributeSet())
        return true;

    if (isARIATextControl())
        return true;

    return false;
}

bool AXNodeObject::isPasswordField() const
{
    Node* node = this->node();
    if (!isHTMLInputElement(node))
        return false;

    if (ariaRoleAttribute() != UnknownRole)
        return false;

    return toHTMLInputElement(node)->isPasswordField();
}

bool AXNodeObject::isProgressIndicator() const
{
    return roleValue() == ProgressIndicatorRole;
}

bool AXNodeObject::isSlider() const
{
    return roleValue() == SliderRole;
}

bool AXNodeObject::isChecked() const
{
    Node* node = this->node();
    if (!node)
        return false;

    // First test for native checkedness semantics
    if (isHTMLInputElement(*node))
        return toHTMLInputElement(*node).shouldAppearChecked();

    // Else, if this is an ARIA checkbox or radio, respect the aria-checked attribute
    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (ariaRole == RadioButtonRole || ariaRole == CheckBoxRole) {
        if (equalIgnoringCase(getAttribute(aria_checkedAttr), "true"))
            return true;
        return false;
    }

    // Otherwise it's not checked
    return false;
}

bool AXNodeObject::isClickable() const
{
    if (node()) {
        if (node()->isElementNode() && toElement(node())->isDisabledFormControl())
            return false;

        // Note: we can't call node()->willRespondToMouseClickEvents() because that triggers a style recalc and can delete this.
        if (node()->hasEventListeners(EventTypeNames::mouseup) || node()->hasEventListeners(EventTypeNames::mousedown) || node()->hasEventListeners(EventTypeNames::click) || node()->hasEventListeners(EventTypeNames::DOMActivate))
            return true;
    }

    return AXObject::isClickable();
}

bool AXNodeObject::isEnabled() const
{
    if (equalIgnoringCase(getAttribute(aria_disabledAttr), "true"))
        return false;

    Node* node = this->node();
    if (!node || !node->isElementNode())
        return true;

    return !toElement(node)->isDisabledFormControl();
}

bool AXNodeObject::isIndeterminate() const
{
    Node* node = this->node();
    if (!isHTMLInputElement(node))
        return false;

    return toHTMLInputElement(node)->shouldAppearIndeterminate();
}

bool AXNodeObject::isPressed() const
{
    if (!isButton())
        return false;

    Node* node = this->node();
    if (!node)
        return false;

    // If this is an ARIA button, check the aria-pressed attribute rather than node()->active()
    if (ariaRoleAttribute() == ButtonRole) {
        if (equalIgnoringCase(getAttribute(aria_pressedAttr), "true"))
            return true;
        return false;
    }

    return node->active();
}

bool AXNodeObject::isReadOnly() const
{
    Node* node = this->node();
    if (!node)
        return true;

    if (isHTMLTextAreaElement(*node))
        return toHTMLTextAreaElement(*node).isReadOnly();

    if (isHTMLInputElement(*node)) {
        HTMLInputElement& input = toHTMLInputElement(*node);
        if (input.isTextField())
            return input.isReadOnly();
    }

    return !node->rendererIsEditable();
}

bool AXNodeObject::isRequired() const
{
    if (equalIgnoringCase(getAttribute(aria_requiredAttr), "true"))
        return true;

    Node* n = this->node();
    if (n && (n->isElementNode() && toElement(n)->isFormControlElement()))
        return toHTMLFormControlElement(n)->isRequired();

    return false;
}

bool AXNodeObject::canSetFocusAttribute() const
{
    Node* node = this->node();
    if (!node)
        return false;

    if (isWebArea())
        return true;

    // NOTE: It would be more accurate to ask the document whether setFocusedNode() would
    // do anything. For example, setFocusedNode() will do nothing if the current focused
    // node will not relinquish the focus.
    if (!node)
        return false;

    if (isDisabledFormControl(node))
        return false;

    return node->isElementNode() && toElement(node)->supportsFocus();
}

bool AXNodeObject::canSetValueAttribute() const
{
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

bool AXNodeObject::canvasHasFallbackContent() const
{
    Node* node = this->node();
    if (!isHTMLCanvasElement(node))
        return false;

    // If it has any children that are elements, we'll assume it might be fallback
    // content. If it has no children or its only children are not elements
    // (e.g. just text nodes), it doesn't have fallback content.
    return ElementTraversal::firstChild(*node);
}

bool AXNodeObject::exposesTitleUIElement() const
{
    if (!isControl())
        return false;

    // If this control is ignored (because it's invisible),
    // then the label needs to be exposed so it can be visible to accessibility.
    if (accessibilityIsIgnored())
        return true;

    // ARIA: section 2A, bullet #3 says if aria-labeledby or aria-label appears, it should
    // override the "label" element association.
    bool hasTextAlternative = (!ariaLabeledByAttribute().isEmpty() || !getAttribute(aria_labelAttr).isEmpty());

    // Checkboxes and radio buttons use the text of their title ui element as their own AXTitle.
    // This code controls whether the title ui element should appear in the AX tree (usually, no).
    // It should appear if the control already has a label (which will be used as the AXTitle instead).
    if (isCheckboxOrRadio())
        return hasTextAlternative;

    // When controls have their own descriptions, the title element should be ignored.
    if (hasTextAlternative)
        return false;

    return true;
}

int AXNodeObject::headingLevel() const
{
    // headings can be in block flow and non-block flow
    Node* node = this->node();
    if (!node)
        return false;

    if (ariaRoleAttribute() == HeadingRole)
        return getAttribute(aria_levelAttr).toInt();

    if (node->hasTagName(h1Tag))
        return 1;

    if (node->hasTagName(h2Tag))
        return 2;

    if (node->hasTagName(h3Tag))
        return 3;

    if (node->hasTagName(h4Tag))
        return 4;

    if (node->hasTagName(h5Tag))
        return 5;

    if (node->hasTagName(h6Tag))
        return 6;

    return 0;
}

unsigned AXNodeObject::hierarchicalLevel() const
{
    Node* node = this->node();
    if (!node || !node->isElementNode())
        return 0;
    Element* element = toElement(node);
    String ariaLevel = element->getAttribute(aria_levelAttr);
    if (!ariaLevel.isEmpty())
        return ariaLevel.toInt();

    // Only tree item will calculate its level through the DOM currently.
    if (roleValue() != TreeItemRole)
        return 0;

    // Hierarchy leveling starts at 1, to match the aria-level spec.
    // We measure tree hierarchy by the number of groups that the item is within.
    unsigned level = 1;
    for (AXObject* parent = parentObject(); parent; parent = parent->parentObject()) {
        AccessibilityRole parentRole = parent->roleValue();
        if (parentRole == GroupRole)
            level++;
        else if (parentRole == TreeRole)
            break;
    }

    return level;
}

String AXNodeObject::text() const
{
    // If this is a user defined static text, use the accessible name computation.
    if (ariaRoleAttribute() == StaticTextRole)
        return ariaAccessibilityDescription();

    if (!isTextControl())
        return String();

    Node* node = this->node();
    if (!node)
        return String();

    if (isNativeTextControl() && (isHTMLTextAreaElement(*node) || isHTMLInputElement(*node)))
        return toHTMLTextFormControlElement(*node).value();

    if (!node->isElementNode())
        return String();

    return toElement(node)->innerText();
}

AXObject* AXNodeObject::titleUIElement() const
{
    if (!node() || !node()->isElementNode())
        return 0;

    if (isFieldset())
        return axObjectCache()->getOrCreate(toHTMLFieldSetElement(node())->legend());

    HTMLLabelElement* label = labelForElement(toElement(node()));
    if (label)
        return axObjectCache()->getOrCreate(label);

    return 0;
}

AccessibilityButtonState AXNodeObject::checkboxOrRadioValue() const
{
    if (isNativeCheckboxOrRadio())
        return isChecked() ? ButtonStateOn : ButtonStateOff;

    return AXObject::checkboxOrRadioValue();
}

void AXNodeObject::colorValue(int& r, int& g, int& b) const
{
    r = 0;
    g = 0;
    b = 0;

    if (!isColorWell())
        return;

    if (!isHTMLInputElement(node()))
        return;

    HTMLInputElement* input = toHTMLInputElement(node());
    const AtomicString& type = input->getAttribute(typeAttr);
    if (!equalIgnoringCase(type, "color"))
        return;

    // HTMLInputElement::value always returns a string parseable by Color.
    Color color;
    bool success = color.setFromString(input->value());
    ASSERT_UNUSED(success, success);
    r = color.red();
    g = color.green();
    b = color.blue();
}

String AXNodeObject::valueDescription() const
{
    if (!supportsRangeValue())
        return String();

    return getAttribute(aria_valuetextAttr).string();
}

float AXNodeObject::valueForRange() const
{
    if (hasAttribute(aria_valuenowAttr))
        return getAttribute(aria_valuenowAttr).toFloat();

    if (isHTMLInputElement(node())) {
        HTMLInputElement& input = toHTMLInputElement(*node());
        if (input.isRangeControl())
            return input.valueAsNumber();
    }

    return 0.0;
}

float AXNodeObject::maxValueForRange() const
{
    if (hasAttribute(aria_valuemaxAttr))
        return getAttribute(aria_valuemaxAttr).toFloat();

    if (isHTMLInputElement(node())) {
        HTMLInputElement& input = toHTMLInputElement(*node());
        if (input.isRangeControl())
            return input.maximum();
    }

    return 0.0;
}

float AXNodeObject::minValueForRange() const
{
    if (hasAttribute(aria_valueminAttr))
        return getAttribute(aria_valueminAttr).toFloat();

    if (isHTMLInputElement(node())) {
        HTMLInputElement& input = toHTMLInputElement(*node());
        if (input.isRangeControl())
            return input.minimum();
    }

    return 0.0;
}

float AXNodeObject::stepValueForRange() const
{
    return getAttribute(stepAttr).toFloat();
}

String AXNodeObject::stringValue() const
{
    Node* node = this->node();
    if (!node)
        return String();

    if (ariaRoleAttribute() == StaticTextRole) {
        String staticText = text();
        if (!staticText.length())
            staticText = textUnderElement();
        return staticText;
    }

    if (node->isTextNode())
        return textUnderElement();

    if (isHTMLSelectElement(*node)) {
        HTMLSelectElement& selectElement = toHTMLSelectElement(*node);
        int selectedIndex = selectElement.selectedIndex();
        const WillBeHeapVector<RawPtrWillBeMember<HTMLElement> >& listItems = selectElement.listItems();
        if (selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < listItems.size()) {
            const AtomicString& overriddenDescription = listItems[selectedIndex]->fastGetAttribute(aria_labelAttr);
            if (!overriddenDescription.isNull())
                return overriddenDescription;
        }
        if (!selectElement.multiple())
            return selectElement.value();
        return String();
    }

    if (isTextControl())
        return text();

    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return String();
}

String AXNodeObject::ariaDescribedByAttribute() const
{
    Vector<Element*> elements;
    elementsFromAttribute(elements, aria_describedbyAttr);

    return accessibilityDescriptionForElements(elements);
}


String AXNodeObject::ariaLabeledByAttribute() const
{
    Vector<Element*> elements;
    ariaLabeledByElements(elements);

    return accessibilityDescriptionForElements(elements);
}

AccessibilityRole AXNodeObject::ariaRoleAttribute() const
{
    return m_ariaRole;
}

// When building the textUnderElement for an object, determine whether or not
// we should include the inner text of this given descendant object or skip it.
static bool shouldUseAccessiblityObjectInnerText(AXObject* obj)
{
    // Consider this hypothetical example:
    // <div tabindex=0>
    //   <h2>
    //     Table of contents
    //   </h2>
    //   <a href="#start">Jump to start of book</a>
    //   <ul>
    //     <li><a href="#1">Chapter 1</a></li>
    //     <li><a href="#1">Chapter 2</a></li>
    //   </ul>
    // </div>
    //
    // The goal is to return a reasonable title for the outer container div, because
    // it's focusable - but without making its title be the full inner text, which is
    // quite long. As a heuristic, skip links, controls, and elements that are usually
    // containers with lots of children.

    // Skip hidden children
    if (obj->isInertOrAriaHidden())
        return false;

    // Skip focusable children, so we don't include the text of links and controls.
    if (obj->canSetFocusAttribute())
        return false;

    // Skip big container elements like lists, tables, etc.
    if (obj->isList() || obj->isAXTable() || obj->isTree() || obj->isCanvas())
        return false;

    return true;
}

String AXNodeObject::textUnderElement() const
{
    Node* node = this->node();
    if (node && node->isTextNode())
        return toText(node)->wholeText();

    StringBuilder builder;
    for (AXObject* child = firstChild(); child; child = child->nextSibling()) {
        if (!shouldUseAccessiblityObjectInnerText(child))
            continue;

        if (child->isAXNodeObject()) {
            Vector<AccessibilityText> textOrder;
            toAXNodeObject(child)->alternativeText(textOrder);
            if (textOrder.size() > 0) {
                builder.append(textOrder[0].text);
                continue;
            }
        }

        builder.append(child->textUnderElement());
    }

    return builder.toString();
}

String AXNodeObject::accessibilityDescription() const
{
    // Static text should not have a description, it should only have a stringValue.
    if (roleValue() == StaticTextRole)
        return String();

    String ariaDescription = ariaAccessibilityDescription();
    if (!ariaDescription.isEmpty())
        return ariaDescription;

    if (isImage() || isInputImage() || isNativeImage() || isCanvas()) {
        // Images should use alt as long as the attribute is present, even if empty.
        // Otherwise, it should fallback to other methods, like the title attribute.
        const AtomicString& alt = getAttribute(altAttr);
        if (!alt.isNull())
            return alt;
    }

    // An element's descriptive text is comprised of title() (what's visible on the screen) and accessibilityDescription() (other descriptive text).
    // Both are used to generate what a screen reader speaks.
    // If this point is reached (i.e. there's no accessibilityDescription) and there's no title(), we should fallback to using the title attribute.
    // The title attribute is normally used as help text (because it is a tooltip), but if there is nothing else available, this should be used (according to ARIA).
    if (title().isEmpty())
        return getAttribute(titleAttr);

    return String();
}

String AXNodeObject::title() const
{
    Node* node = this->node();
    if (!node)
        return String();

    bool isInputElement = isHTMLInputElement(*node);
    if (isInputElement) {
        HTMLInputElement& input = toHTMLInputElement(*node);
        if (input.isTextButton())
            return input.valueWithDefault();
    }

    if (isInputElement || AXObject::isARIAInput(ariaRoleAttribute()) || isControl()) {
        HTMLLabelElement* label = labelForElement(toElement(node));
        if (label && !exposesTitleUIElement())
            return label->innerText();
    }

    // If this node isn't rendered, there's no inner text we can extract from a select element.
    if (!isAXRenderObject() && isHTMLSelectElement(*node))
        return String();

    switch (roleValue()) {
    case PopUpButtonRole:
        // Native popup buttons should not use their button children's text as a title. That value is retrieved through stringValue().
        if (isHTMLSelectElement(*node))
            return String();
    case ButtonRole:
    case ToggleButtonRole:
    case CheckBoxRole:
    case ListBoxOptionRole:
    case MenuButtonRole:
    case MenuItemRole:
    case RadioButtonRole:
    case TabRole:
        return textUnderElement();
    // SVGRoots should not use the text under itself as a title. That could include the text of objects like <text>.
    case SVGRootRole:
        return String();
    default:
        break;
    }

    if (isHeading() || isLink())
        return textUnderElement();

    // If it's focusable but it's not content editable or a known control type, then it will appear to
    // the user as a single atomic object, so we should use its text as the default title.
    if (isGenericFocusableElement())
        return textUnderElement();

    return String();
}

String AXNodeObject::helpText() const
{
    Node* node = this->node();
    if (!node)
        return String();

    const AtomicString& ariaHelp = getAttribute(aria_helpAttr);
    if (!ariaHelp.isEmpty())
        return ariaHelp;

    String describedBy = ariaDescribedByAttribute();
    if (!describedBy.isEmpty())
        return describedBy;

    String description = accessibilityDescription();
    for (Node* curr = node; curr; curr = curr->parentNode()) {
        if (curr->isHTMLElement()) {
            const AtomicString& summary = toElement(curr)->getAttribute(summaryAttr);
            if (!summary.isEmpty())
                return summary;

            // The title attribute should be used as help text unless it is already being used as descriptive text.
            const AtomicString& title = toElement(curr)->getAttribute(titleAttr);
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

LayoutRect AXNodeObject::elementRect() const
{
    // First check if it has a custom rect, for example if this element is tied to a canvas path.
    if (!m_explicitElementRect.isEmpty())
        return m_explicitElementRect;

    // AXNodeObjects have no mechanism yet to return a size or position.
    // For now, let's return the position of the ancestor that does have a position,
    // and make it the width of that parent, and about the height of a line of text, so that it's clear the object is a child of the parent.

    LayoutRect boundingBox;

    for (AXObject* positionProvider = parentObject(); positionProvider; positionProvider = positionProvider->parentObject()) {
        if (positionProvider->isAXRenderObject()) {
            LayoutRect parentRect = positionProvider->elementRect();
            boundingBox.setSize(LayoutSize(parentRect.width(), LayoutUnit(std::min(10.0f, parentRect.height().toFloat()))));
            boundingBox.setLocation(parentRect.location());
            break;
        }
    }

    return boundingBox;
}

AXObject* AXNodeObject::parentObject() const
{
    if (!node())
        return 0;

    Node* parentObj = node()->parentNode();
    if (parentObj)
        return axObjectCache()->getOrCreate(parentObj);

    return 0;
}

AXObject* AXNodeObject::parentObjectIfExists() const
{
    return parentObject();
}

AXObject* AXNodeObject::firstChild() const
{
    if (!node())
        return 0;

    Node* firstChild = node()->firstChild();

    if (!firstChild)
        return 0;

    return axObjectCache()->getOrCreate(firstChild);
}

AXObject* AXNodeObject::nextSibling() const
{
    if (!node())
        return 0;

    Node* nextSibling = node()->nextSibling();
    if (!nextSibling)
        return 0;

    return axObjectCache()->getOrCreate(nextSibling);
}

void AXNodeObject::addChildren()
{
    // If the need to add more children in addition to existing children arises,
    // childrenChanged should have been called, leaving the object with no children.
    ASSERT(!m_haveChildren);

    if (!m_node)
        return;

    m_haveChildren = true;

    // The only time we add children from the DOM tree to a node with a renderer is when it's a canvas.
    if (renderer() && !isHTMLCanvasElement(*m_node))
        return;

    for (Node* child = m_node->firstChild(); child; child = child->nextSibling())
        addChild(axObjectCache()->getOrCreate(child));
}

void AXNodeObject::addChild(AXObject* child)
{
    insertChild(child, m_children.size());
}

void AXNodeObject::insertChild(AXObject* child, unsigned index)
{
    if (!child)
        return;

    // If the parent is asking for this child's children, then either it's the first time (and clearing is a no-op),
    // or its visibility has changed. In the latter case, this child may have a stale child cached.
    // This can prevent aria-hidden changes from working correctly. Hence, whenever a parent is getting children, ensure data is not stale.
    child->clearChildren();

    if (child->accessibilityIsIgnored()) {
        AccessibilityChildrenVector children = child->children();
        size_t length = children.size();
        for (size_t i = 0; i < length; ++i)
            m_children.insert(index + i, children[i]);
    } else {
        ASSERT(child->parentObject() == this);
        m_children.insert(index, child);
    }
}

bool AXNodeObject::canHaveChildren() const
{
    // If this is an AXRenderObject, then it's okay if this object
    // doesn't have a node - there are some renderers that don't have associated
    // nodes, like scroll areas and css-generated text.
    if (!node() && !isAXRenderObject())
        return false;

    // Elements that should not have children
    switch (roleValue()) {
    case ImageRole:
    case ButtonRole:
    case PopUpButtonRole:
    case CheckBoxRole:
    case RadioButtonRole:
    case TabRole:
    case ToggleButtonRole:
    case ListBoxOptionRole:
    case ScrollBarRole:
        return false;
    case StaticTextRole:
        if (!axObjectCache()->inlineTextBoxAccessibility())
            return false;
    default:
        return true;
    }
}

Element* AXNodeObject::actionElement() const
{
    Node* node = this->node();
    if (!node)
        return 0;

    if (isHTMLInputElement(*node)) {
        HTMLInputElement& input = toHTMLInputElement(*node);
        if (!input.isDisabledFormControl() && (isCheckboxOrRadio() || input.isTextButton()))
            return &input;
    } else if (isHTMLButtonElement(*node)) {
        return toElement(node);
    }

    if (isFileUploadButton())
        return toElement(node);

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
    case ListItemRole:
        return toElement(node);
    default:
        break;
    }

    Element* elt = anchorElement();
    if (!elt)
        elt = mouseButtonListener();
    return elt;
}

Element* AXNodeObject::anchorElement() const
{
    Node* node = this->node();
    if (!node)
        return 0;

    AXObjectCache* cache = axObjectCache();

    // search up the DOM tree for an anchor element
    // NOTE: this assumes that any non-image with an anchor is an HTMLAnchorElement
    for ( ; node; node = node->parentNode()) {
        if (isHTMLAnchorElement(*node) || (node->renderer() && cache->getOrCreate(node->renderer())->isAnchor()))
            return toElement(node);
    }

    return 0;
}

Document* AXNodeObject::document() const
{
    if (!node())
        return 0;
    return &node()->document();
}

void AXNodeObject::setNode(Node* node)
{
    m_node = node;
}

AXObject* AXNodeObject::correspondingControlForLabelElement() const
{
    HTMLLabelElement* labelElement = labelElementContainer();
    if (!labelElement)
        return 0;

    HTMLElement* correspondingControl = labelElement->control();
    if (!correspondingControl)
        return 0;

    // Make sure the corresponding control isn't a descendant of this label
    // that's in the middle of being destroyed.
    if (correspondingControl->renderer() && !correspondingControl->renderer()->parent())
        return 0;

    return axObjectCache()->getOrCreate(correspondingControl);
}

HTMLLabelElement* AXNodeObject::labelElementContainer() const
{
    if (!node())
        return 0;

    // the control element should not be considered part of the label
    if (isControl())
        return 0;

    // find if this has a ancestor that is a label
    return Traversal<HTMLLabelElement>::firstAncestorOrSelf(*node());
}

void AXNodeObject::setFocused(bool on)
{
    if (!canSetFocusAttribute())
        return;

    Document* document = this->document();
    if (!on) {
        document->setFocusedElement(nullptr);
    } else {
        Node* node = this->node();
        if (node && node->isElementNode()) {
            // If this node is already the currently focused node, then calling focus() won't do anything.
            // That is a problem when focus is removed from the webpage to chrome, and then returns.
            // In these cases, we need to do what keyboard and mouse focus do, which is reset focus first.
            if (document->focusedElement() == node)
                document->setFocusedElement(nullptr);

            toElement(node)->focus();
        } else {
            document->setFocusedElement(nullptr);
        }
    }
}

void AXNodeObject::increment()
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    alterSliderValue(true);
}

void AXNodeObject::decrement()
{
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);
    alterSliderValue(false);
}

void AXNodeObject::childrenChanged()
{
    // This method is meant as a quick way of marking a portion of the accessibility tree dirty.
    if (!node() && !renderer())
        return;

    axObjectCache()->postNotification(this, document(), AXObjectCache::AXChildrenChanged, true);

    // Go up the accessibility parent chain, but only if the element already exists. This method is
    // called during render layouts, minimal work should be done.
    // If AX elements are created now, they could interrogate the render tree while it's in a funky state.
    // At the same time, process ARIA live region changes.
    for (AXObject* parent = this; parent; parent = parent->parentObjectIfExists()) {
        parent->setNeedsToUpdateChildren();

        // These notifications always need to be sent because screenreaders are reliant on them to perform.
        // In other words, they need to be sent even when the screen reader has not accessed this live region since the last update.

        // If this element supports ARIA live regions, then notify the AT of changes.
        if (parent->supportsARIALiveRegion())
            axObjectCache()->postNotification(parent, parent->document(), AXObjectCache::AXLiveRegionChanged, true);

        // If this element is an ARIA text box or content editable, post a "value changed" notification on it
        // so that it behaves just like a native input element or textarea.
        if (isNonNativeTextControl())
            axObjectCache()->postNotification(parent, parent->document(), AXObjectCache::AXValueChanged, true);
    }
}

void AXNodeObject::selectionChanged()
{
    // Post the selected text changed event on the first ancestor that's
    // focused (to handle form controls, ARIA text boxes and contentEditable),
    // or the web area if the selection is just in the document somewhere.
    if (isFocused() || isWebArea())
        axObjectCache()->postNotification(this, document(), AXObjectCache::AXSelectedTextChanged, true);
    else
        AXObject::selectionChanged(); // Calls selectionChanged on parent.
}

void AXNodeObject::textChanged()
{
    // If this element supports ARIA live regions, or is part of a region with an ARIA editable role,
    // then notify the AT of changes.
    AXObjectCache* cache = axObjectCache();
    for (Node* parentNode = node(); parentNode; parentNode = parentNode->parentNode()) {
        AXObject* parent = cache->get(parentNode);
        if (!parent)
            continue;

        if (parent->supportsARIALiveRegion())
            cache->postNotification(parentNode, AXObjectCache::AXLiveRegionChanged, true);

        // If this element is an ARIA text box or content editable, post a "value changed" notification on it
        // so that it behaves just like a native input element or textarea.
        if (parent->isNonNativeTextControl())
            cache->postNotification(parentNode, AXObjectCache::AXValueChanged, true);
    }
}

void AXNodeObject::updateAccessibilityRole()
{
    bool ignoredStatus = accessibilityIsIgnored();
    m_role = determineAccessibilityRole();

    // The AX hierarchy only needs to be updated if the ignored status of an element has changed.
    if (ignoredStatus != accessibilityIsIgnored())
        childrenChanged();
}

String AXNodeObject::alternativeTextForWebArea() const
{
    // The WebArea description should follow this order:
    //     aria-label on the <html>
    //     title on the <html>
    //     <title> inside the <head> (of it was set through JS)
    //     name on the <html>
    // For iframes:
    //     aria-label on the <iframe>
    //     title on the <iframe>
    //     name on the <iframe>

    Document* document = this->document();
    if (!document)
        return String();

    // Check if the HTML element has an aria-label for the webpage.
    if (Element* documentElement = document->documentElement()) {
        const AtomicString& ariaLabel = documentElement->getAttribute(aria_labelAttr);
        if (!ariaLabel.isEmpty())
            return ariaLabel;
    }

    Node* owner = document->ownerElement();
    if (owner) {
        if (isHTMLFrameElementBase(*owner)) {
            const AtomicString& title = toElement(owner)->getAttribute(titleAttr);
            if (!title.isEmpty())
                return title;
            return toElement(owner)->getNameAttribute();
        }
        if (owner->isHTMLElement())
            return toHTMLElement(owner)->getNameAttribute();
    }

    String documentTitle = document->title();
    if (!documentTitle.isEmpty())
        return documentTitle;

    owner = document->body();
    if (owner && owner->isHTMLElement())
        return toHTMLElement(owner)->getNameAttribute();

    return String();
}

void AXNodeObject::alternativeText(Vector<AccessibilityText>& textOrder) const
{
    if (isWebArea()) {
        String webAreaText = alternativeTextForWebArea();
        if (!webAreaText.isEmpty())
            textOrder.append(AccessibilityText(webAreaText, AlternativeText));
        return;
    }

    ariaLabeledByText(textOrder);

    const AtomicString& ariaLabel = getAttribute(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        textOrder.append(AccessibilityText(ariaLabel, AlternativeText));

    if (isImage() || isInputImage() || isNativeImage() || isCanvas()) {
        // Images should use alt as long as the attribute is present, even if empty.
        // Otherwise, it should fallback to other methods, like the title attribute.
        const AtomicString& alt = getAttribute(altAttr);
        if (!alt.isNull())
            textOrder.append(AccessibilityText(alt, AlternativeText));
    }
}

void AXNodeObject::ariaLabeledByText(Vector<AccessibilityText>& textOrder) const
{
    String ariaLabeledBy = ariaLabeledByAttribute();
    if (!ariaLabeledBy.isEmpty()) {
        Vector<Element*> elements;
        ariaLabeledByElements(elements);

        unsigned length = elements.size();
        for (unsigned k = 0; k < length; k++) {
            RefPtr<AXObject> axElement = axObjectCache()->getOrCreate(elements[k]);
            textOrder.append(AccessibilityText(ariaLabeledBy, AlternativeText, axElement));
        }
    }
}

void AXNodeObject::changeValueByPercent(float percentChange)
{
    float range = maxValueForRange() - minValueForRange();
    float value = valueForRange();

    value += range * (percentChange / 100);
    setValue(String::number(value));

    axObjectCache()->postNotification(node(), AXObjectCache::AXValueChanged, true);
}

} // namespace WebCore
