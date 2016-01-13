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

#ifndef AXNodeObject_h
#define AXNodeObject_h

#include "core/accessibility/AXObject.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/Forward.h"

namespace WebCore {

class AXObjectCache;
class Element;
class LocalFrame;
class FrameView;
class HitTestResult;
class HTMLAnchorElement;
class HTMLAreaElement;
class HTMLElement;
class HTMLLabelElement;
class HTMLMapElement;
class HTMLSelectElement;
class IntPoint;
class IntSize;
class Node;
class RenderListBox;
class RenderTextControl;
class RenderView;
class VisibleSelection;
class Widget;

class AXNodeObject : public AXObject {
protected:
    explicit AXNodeObject(Node*);

public:
    static PassRefPtr<AXNodeObject> create(Node*);
    virtual ~AXNodeObject();

protected:
    // Protected data.
    AccessibilityRole m_ariaRole;
    bool m_childrenDirty;
#ifndef NDEBUG
    bool m_initialized;
#endif

    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;
    virtual AccessibilityRole determineAccessibilityRole();

    String accessibilityDescriptionForElements(Vector<Element*> &elements) const;
    void alterSliderValue(bool increase);
    String ariaAccessibilityDescription() const;
    void ariaLabeledByElements(Vector<Element*>& elements) const;
    void changeValueByStep(bool increase);
    AccessibilityRole determineAriaRoleAttribute() const;
    void elementsFromAttribute(Vector<Element*>& elements, const QualifiedName&) const;
    bool hasContentEditableAttributeSet() const;
    bool isDescendantOfBarrenParent() const;
    // This returns true if it's focusable but it's not content editable and it's not a control or ARIA control.
    bool isGenericFocusableElement() const;
    HTMLLabelElement* labelForElement(Element*) const;
    AXObject* menuButtonForMenu() const;
    Element* menuItemElementForMenu() const;
    Element* mouseButtonListener() const;
    AccessibilityRole remapAriaRoleDueToParent(AccessibilityRole) const;
    bool isNativeCheckboxOrRadio() const;
    void setNode(Node*);
    AXObject* correspondingControlForLabelElement() const;
    HTMLLabelElement* labelElementContainer() const;

    //
    // Overridden from AXObject.
    //

    virtual void init() OVERRIDE;
    virtual void detach() OVERRIDE;
    virtual bool isDetached() const OVERRIDE { return !m_node; }
    virtual bool isAXNodeObject() const OVERRIDE FINAL { return true; }

    // Check object role or purpose.
    virtual bool isAnchor() const OVERRIDE FINAL;
    virtual bool isControl() const OVERRIDE;
    virtual bool isEmbeddedObject() const OVERRIDE FINAL;
    virtual bool isFieldset() const OVERRIDE FINAL;
    virtual bool isHeading() const OVERRIDE FINAL;
    virtual bool isHovered() const OVERRIDE FINAL;
    virtual bool isImage() const OVERRIDE FINAL;
    bool isImageButton() const;
    virtual bool isInputImage() const OVERRIDE FINAL;
    virtual bool isLink() const OVERRIDE FINAL;
    virtual bool isMenu() const OVERRIDE FINAL;
    virtual bool isMenuButton() const OVERRIDE FINAL;
    virtual bool isMultiSelectable() const OVERRIDE;
    bool isNativeImage() const;
    virtual bool isNativeTextControl() const OVERRIDE FINAL;
    virtual bool isNonNativeTextControl() const OVERRIDE FINAL;
    virtual bool isPasswordField() const OVERRIDE FINAL;
    virtual bool isProgressIndicator() const OVERRIDE;
    virtual bool isSlider() const OVERRIDE;

    // Check object state.
    virtual bool isChecked() const OVERRIDE FINAL;
    virtual bool isClickable() const OVERRIDE FINAL;
    virtual bool isEnabled() const OVERRIDE;
    virtual bool isIndeterminate() const OVERRIDE FINAL;
    virtual bool isPressed() const OVERRIDE FINAL;
    virtual bool isReadOnly() const OVERRIDE;
    virtual bool isRequired() const OVERRIDE FINAL;

    // Check whether certain properties can be modified.
    virtual bool canSetFocusAttribute() const OVERRIDE;
    virtual bool canSetValueAttribute() const OVERRIDE;

    // Properties of static elements.
    virtual bool canvasHasFallbackContent() const OVERRIDE FINAL;
    virtual bool exposesTitleUIElement() const OVERRIDE;
    virtual int headingLevel() const OVERRIDE FINAL;
    virtual unsigned hierarchicalLevel() const OVERRIDE FINAL;
    virtual String text() const OVERRIDE;
    virtual AXObject* titleUIElement() const OVERRIDE;

    // Properties of interactive elements.
    virtual AccessibilityButtonState checkboxOrRadioValue() const OVERRIDE FINAL;
    virtual void colorValue(int& r, int& g, int& b) const OVERRIDE FINAL;
    virtual String valueDescription() const OVERRIDE;
    virtual float valueForRange() const OVERRIDE;
    virtual float maxValueForRange() const OVERRIDE;
    virtual float minValueForRange() const OVERRIDE;
    virtual String stringValue() const OVERRIDE;

    // ARIA attributes.
    virtual String ariaDescribedByAttribute() const OVERRIDE FINAL;
    virtual String ariaLabeledByAttribute() const OVERRIDE FINAL;
    virtual AccessibilityRole ariaRoleAttribute() const OVERRIDE FINAL;

    // Accessibility Text.
    virtual String textUnderElement() const OVERRIDE;

    // Accessibility Text - (To be deprecated).
    virtual String accessibilityDescription() const OVERRIDE;
    virtual String title() const OVERRIDE;
    virtual String helpText() const OVERRIDE;

    // Location and click point in frame-relative coordinates.
    virtual LayoutRect elementRect() const OVERRIDE;

    // High-level accessibility tree access.
    virtual AXObject* parentObject() const OVERRIDE;
    virtual AXObject* parentObjectIfExists() const OVERRIDE;

    // Low-level accessibility tree exploration.
    virtual AXObject* firstChild() const OVERRIDE;
    virtual AXObject* nextSibling() const OVERRIDE;
    virtual void addChildren() OVERRIDE;
    virtual bool canHaveChildren() const OVERRIDE;
    void addChild(AXObject*);
    void insertChild(AXObject*, unsigned index);

    // DOM and Render tree access.
    virtual Element* actionElement() const OVERRIDE FINAL;
    virtual Element* anchorElement() const OVERRIDE;
    virtual Document* document() const OVERRIDE;
    virtual Node* node() const OVERRIDE { return m_node; }

    // Modify or take an action on an object.
    virtual void setFocused(bool) OVERRIDE FINAL;
    virtual void increment() OVERRIDE FINAL;
    virtual void decrement() OVERRIDE FINAL;

    // Notifications that this object may have changed.
    virtual void childrenChanged() OVERRIDE;
    virtual void selectionChanged() OVERRIDE FINAL;
    virtual void textChanged() OVERRIDE;
    virtual void updateAccessibilityRole() OVERRIDE FINAL;

private:
    Node* m_node;

    String alternativeTextForWebArea() const;
    void alternativeText(Vector<AccessibilityText>&) const;
    void ariaLabeledByText(Vector<AccessibilityText>&) const;
    void changeValueByPercent(float percentChange);
    float stepValueForRange() const;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXNodeObject, isAXNodeObject());

} // namespace WebCore

#endif // AXNodeObject_h
