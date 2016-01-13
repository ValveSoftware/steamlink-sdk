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

#ifndef AXRenderObject_h
#define AXRenderObject_h

#include "core/accessibility/AXNodeObject.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/Forward.h"

namespace WebCore {

class AXSVGRoot;
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

class AXRenderObject : public AXNodeObject {
protected:
    explicit AXRenderObject(RenderObject*);
public:
    static PassRefPtr<AXRenderObject> create(RenderObject*);
    virtual ~AXRenderObject();

    // Public, overridden from AXObject.
    virtual RenderObject* renderer() const OVERRIDE FINAL { return m_renderer; }
    virtual LayoutRect elementRect() const OVERRIDE;

    void setRenderer(RenderObject*);
    RenderBoxModelObject* renderBoxModelObject() const;
    Document* topDocument() const;
    bool shouldNotifyActiveDescendant() const;
    virtual ScrollableArea* getScrollableAreaIfScrollable() const OVERRIDE FINAL;
    virtual AccessibilityRole determineAccessibilityRole() OVERRIDE;
    void checkCachedElementRect() const;
    void updateCachedElementRect() const;

protected:
    RenderObject* m_renderer;
    mutable LayoutRect m_cachedElementRect;
    mutable LayoutRect m_cachedFrameRect;
    mutable IntPoint m_cachedScrollPosition;
    mutable bool m_cachedElementRectDirty;

    //
    // Overridden from AXObject.
    //

    virtual void init() OVERRIDE;
    virtual void detach() OVERRIDE;
    virtual bool isDetached() const OVERRIDE { return !m_renderer; }
    virtual bool isAXRenderObject() const OVERRIDE { return true; }

    // Check object role or purpose.
    virtual bool isAttachment() const OVERRIDE;
    virtual bool isFileUploadButton() const OVERRIDE;
    virtual bool isLinked() const OVERRIDE;
    virtual bool isLoaded() const OVERRIDE;
    virtual bool isOffScreen() const OVERRIDE;
    virtual bool isReadOnly() const OVERRIDE;
    virtual bool isVisited() const OVERRIDE;

    // Check object state.
    virtual bool isFocused() const OVERRIDE;
    virtual bool isSelected() const OVERRIDE;

    // Whether objects are ignored, i.e. not included in the tree.
    virtual AXObjectInclusion defaultObjectInclusion() const OVERRIDE;
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;

    // Properties of static elements.
    virtual const AtomicString& accessKey() const OVERRIDE;
    virtual AccessibilityOrientation orientation() const OVERRIDE;
    virtual String text() const OVERRIDE;
    virtual int textLength() const OVERRIDE;
    virtual KURL url() const OVERRIDE;

    // Properties of interactive elements.
    virtual String actionVerb() const OVERRIDE;
    virtual String stringValue() const OVERRIDE;

    // ARIA attributes.
    virtual AXObject* activeDescendant() const OVERRIDE;
    virtual void ariaFlowToElements(AccessibilityChildrenVector&) const OVERRIDE;
    virtual void ariaControlsElements(AccessibilityChildrenVector&) const OVERRIDE;
    virtual void ariaDescribedbyElements(AccessibilityChildrenVector&) const OVERRIDE;
    virtual void ariaLabelledbyElements(AccessibilityChildrenVector&) const OVERRIDE;
    virtual void ariaOwnsElements(AccessibilityChildrenVector&) const OVERRIDE;

    virtual bool ariaHasPopup() const OVERRIDE;
    virtual bool ariaRoleHasPresentationalChildren() const OVERRIDE;
    virtual bool isPresentationalChildOfAriaRole() const OVERRIDE;
    virtual bool shouldFocusActiveDescendant() const OVERRIDE;
    virtual bool supportsARIADragging() const OVERRIDE;
    virtual bool supportsARIADropping() const OVERRIDE;
    virtual bool supportsARIAFlowTo() const OVERRIDE;
    virtual bool supportsARIAOwns() const OVERRIDE;

    // ARIA live-region features.
    virtual const AtomicString& ariaLiveRegionStatus() const OVERRIDE;
    virtual const AtomicString& ariaLiveRegionRelevant() const OVERRIDE;
    virtual bool ariaLiveRegionAtomic() const OVERRIDE;
    virtual bool ariaLiveRegionBusy() const OVERRIDE;

    // Accessibility Text.
    virtual String textUnderElement() const OVERRIDE;

    // Accessibility Text - (To be deprecated).
    virtual String helpText() const OVERRIDE;

    // Location and click point in frame-relative coordinates.
    virtual void markCachedElementRectDirty() const OVERRIDE;
    virtual IntPoint clickPoint() OVERRIDE;

    // Hit testing.
    virtual AXObject* accessibilityHitTest(const IntPoint&) const OVERRIDE;
    virtual AXObject* elementAccessibilityHitTest(const IntPoint&) const OVERRIDE;

    // High-level accessibility tree access. Other modules should only use these functions.
    virtual AXObject* parentObject() const OVERRIDE;
    virtual AXObject* parentObjectIfExists() const OVERRIDE;

    // Low-level accessibility tree exploration, only for use within the accessibility module.
    virtual AXObject* firstChild() const OVERRIDE;
    virtual AXObject* nextSibling() const OVERRIDE;
    virtual void addChildren() OVERRIDE;
    virtual bool canHaveChildren() const OVERRIDE;
    virtual void updateChildrenIfNecessary() OVERRIDE;
    virtual bool needsToUpdateChildren() const { return m_childrenDirty; }
    virtual void setNeedsToUpdateChildren() OVERRIDE { m_childrenDirty = true; }
    virtual void clearChildren() OVERRIDE;
    virtual AXObject* observableObject() const OVERRIDE;

    // Properties of the object's owning document or page.
    virtual double estimatedLoadingProgress() const OVERRIDE;

    // DOM and Render tree access.
    virtual Node* node() const OVERRIDE;
    virtual Document* document() const OVERRIDE;
    virtual FrameView* documentFrameView() const OVERRIDE;
    virtual Element* anchorElement() const OVERRIDE;
    virtual Widget* widgetForAttachmentView() const OVERRIDE;

    // Selected text.
    virtual PlainTextRange selectedTextRange() const OVERRIDE;

    // Modify or take an action on an object.
    virtual void setSelectedTextRange(const PlainTextRange&) OVERRIDE;
    virtual void setValue(const String&) OVERRIDE;
    virtual void scrollTo(const IntPoint&) const OVERRIDE;

    // Notifications that this object may have changed.
    virtual void handleActiveDescendantChanged() OVERRIDE;
    virtual void handleAriaExpandedChanged() OVERRIDE;
    virtual void textChanged() OVERRIDE;

    // Text metrics. Most of these should be deprecated, needs major cleanup.
    virtual int index(const VisiblePosition&) const OVERRIDE;
    virtual VisiblePosition visiblePositionForIndex(int) const OVERRIDE;
    virtual void lineBreaks(Vector<int>&) const OVERRIDE;

private:
    bool isAllowedChildOfTree() const;
    void ariaListboxSelectedChildren(AccessibilityChildrenVector&);
    PlainTextRange ariaSelectedTextRange() const;
    bool nodeIsTextControl(const Node*) const;
    bool isTabItemSelected() const;
    AXObject* accessibilityImageMapHitTest(HTMLAreaElement*, const IntPoint&) const;
    bool renderObjectIsObservable(RenderObject*) const;
    RenderObject* renderParentObject() const;
    bool isDescendantOfElementType(const QualifiedName& tagName) const;
    bool isSVGImage() const;
    void detachRemoteSVGRoot();
    AXSVGRoot* remoteSVGRootElement() const;
    AXObject* remoteSVGElementHitTest(const IntPoint&) const;
    void offsetBoundingBoxForRemoteSVGElement(LayoutRect&) const;
    void addHiddenChildren();
    void addTextFieldChildren();
    void addImageMapChildren();
    void addCanvasChildren();
    void addAttachmentChildren();
    void addRemoteSVGChildren();
    void addInlineTextBoxChildren();

    void ariaSelectedRows(AccessibilityChildrenVector&);
    bool elementAttributeValue(const QualifiedName&) const;
    bool inheritsPresentationalRole() const;
    LayoutRect computeElementRect() const;
    VisibleSelection selection() const;
    int indexForVisiblePosition(const VisiblePosition&) const;
    void accessibilityChildrenFromAttribute(QualifiedName attr, AccessibilityChildrenVector&) const;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXRenderObject, isAXRenderObject());

} // namespace WebCore

#endif // AXRenderObject_h
