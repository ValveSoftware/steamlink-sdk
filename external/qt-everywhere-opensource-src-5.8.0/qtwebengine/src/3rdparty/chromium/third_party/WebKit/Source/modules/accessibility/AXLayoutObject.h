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

#ifndef AXLayoutObject_h
#define AXLayoutObject_h

#include "modules/ModulesExport.h"
#include "modules/accessibility/AXNodeObject.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/Forward.h"

namespace blink {

class AXObjectCacheImpl;
class AXSVGRoot;
class Element;
class FrameView;
class HTMLAreaElement;
class IntPoint;
class Node;
class Widget;

class MODULES_EXPORT AXLayoutObject : public AXNodeObject {
    WTF_MAKE_NONCOPYABLE(AXLayoutObject);

protected:
    AXLayoutObject(LayoutObject*, AXObjectCacheImpl&);

public:
    static AXLayoutObject* create(LayoutObject*, AXObjectCacheImpl&);
    ~AXLayoutObject() override;

    // Public, overridden from AXObject.
    LayoutObject* getLayoutObject() const final { return m_layoutObject; }
    LayoutRect elementRect() const override;
    LayoutBoxModelObject* getLayoutBoxModelObject() const;
    SkMatrix44 transformFromLocalParentFrame() const override;
    ScrollableArea* getScrollableAreaIfScrollable() const final;
    void getRelativeBounds(AXObject** container, FloatRect& boundsInContainer, SkMatrix44& containerTransform) const override;
    AccessibilityRole determineAccessibilityRole() override;
    AccessibilityRole nativeAccessibilityRoleIgnoringAria() const override;
    void checkCachedElementRect() const;
    void updateCachedElementRect() const;

protected:
    LayoutObject* m_layoutObject;
    mutable LayoutRect m_cachedElementRect;
    mutable LayoutRect m_cachedFrameRect;
    mutable IntPoint m_cachedScrollPosition;
    mutable bool m_cachedElementRectDirty;

    //
    // Overridden from AXObject.
    //

    void init() override;
    void detach() override;
    bool isDetached() const override { return !m_layoutObject; }
    bool isAXLayoutObject() const override { return true; }

    // Check object role or purpose.
    bool isEditable() const override;
    bool isRichlyEditable() const override;
    bool isLinked() const override;
    bool isLoaded() const override;
    bool isOffScreen() const override;
    bool isReadOnly() const override;
    bool isVisited() const override;

    // Check object state.
    bool isFocused() const override;
    bool isSelected() const override;

    // Whether objects are ignored, i.e. not included in the tree.
    AXObjectInclusion defaultObjectInclusion(IgnoredReasons* = nullptr) const override;
    bool computeAccessibilityIsIgnored(IgnoredReasons* = nullptr) const override;

    // Properties of static elements.
    const AtomicString& accessKey() const override;
    RGBA32 computeBackgroundColor() const final;
    RGBA32 color() const final;
    String fontFamily() const final;
    // Font size is in pixels.
    float fontSize() const final;
    String text() const override;
    AccessibilityTextDirection textDirection() const final;
    int textLength() const override;
    TextStyle getTextStyle() const final;
    KURL url() const override;

    // Inline text boxes.
    void loadInlineTextBoxes() override;
    AXObject* nextOnLine() const override;
    AXObject* previousOnLine() const override;

    // Properties of interactive elements.
    String stringValue() const override;

    // ARIA attributes.
    void ariaFlowToElements(AXObjectVector&) const override;
    void ariaControlsElements(AXObjectVector&) const override;
    void ariaDescribedbyElements(AXObjectVector&) const override;
    void ariaLabelledbyElements(AXObjectVector&) const override;
    void ariaOwnsElements(AXObjectVector&) const override;

    bool ariaHasPopup() const override;
    bool ariaRoleHasPresentationalChildren() const override;
    AXObject* ancestorForWhichThisIsAPresentationalChild() const override;
    bool supportsARIADragging() const override;
    bool supportsARIADropping() const override;
    bool supportsARIAFlowTo() const override;
    bool supportsARIAOwns() const override;

    // ARIA live-region features.
    const AtomicString& liveRegionStatus() const override;
    const AtomicString& liveRegionRelevant() const override;
    bool liveRegionAtomic() const override;
    bool liveRegionBusy() const override;

    // AX name calc.
    String textAlternative(bool recursive, bool inAriaLabelledByTraversal, AXObjectSet& visited, AXNameFrom&, AXRelatedObjectVector*, NameSources*) const override;

    // Methods that retrieve or manipulate the current selection.

    AXRange selection() const override;
    AXRange selectionUnderObject() const override;
    void setSelection(const AXRange&) override;

    // Location and click point in frame-relative coordinates.
    void markCachedElementRectDirty() const override;
    IntPoint clickPoint() override;

    // Hit testing.
    AXObject* accessibilityHitTest(const IntPoint&) const override;
    AXObject* elementAccessibilityHitTest(const IntPoint&) const override;

    // High-level accessibility tree access. Other modules should only use these functions.
    AXObject* computeParent() const override;
    AXObject* computeParentIfExists() const override;

    // Low-level accessibility tree exploration, only for use within the accessibility module.
    AXObject* rawFirstChild() const override;
    AXObject* rawNextSibling() const override;
    void addChildren() override;
    bool canHaveChildren() const override;
    void updateChildrenIfNecessary() override;
    bool needsToUpdateChildren() const override { return m_childrenDirty; }
    void setNeedsToUpdateChildren() override { m_childrenDirty = true; }
    void clearChildren() override;

    // Properties of the object's owning document or page.
    double estimatedLoadingProgress() const override;

    // DOM and layout tree access.
    Node* getNode() const override;
    Document* getDocument() const override;
    FrameView* documentFrameView() const override;
    Element* anchorElement() const override;

    void setValue(const String&) override;

    // Notifications that this object may have changed.
    void handleActiveDescendantChanged() override;
    void handleAriaExpandedChanged() override;
    void textChanged() override;

    // Text metrics. Most of these should be deprecated, needs major cleanup.
    int index(const VisiblePosition&) const override;
    VisiblePosition visiblePositionForIndex(int) const override;
    void lineBreaks(Vector<int>&) const final;

private:
    AXObject* treeAncestorDisallowingChild() const;
    bool isTabItemSelected() const;
    bool isValidSelectionBound(const AXObject*) const;
    AXObject* accessibilityImageMapHitTest(HTMLAreaElement*, const IntPoint&) const;
    LayoutObject* layoutParentObject() const;
    bool isSVGImage() const;
    void detachRemoteSVGRoot();
    AXSVGRoot* remoteSVGRootElement() const;
    AXObject* remoteSVGElementHitTest(const IntPoint&) const;
    void offsetBoundingBoxForRemoteSVGElement(LayoutRect&) const;
    void addHiddenChildren();
    void addTextFieldChildren();
    void addImageMapChildren();
    void addCanvasChildren();
    void addPopupChildren();
    void addRemoteSVGChildren();
    void addInlineTextBoxChildren(bool force);

    bool elementAttributeValue(const QualifiedName&) const;
    LayoutRect computeElementRect() const;
    AXRange textControlSelection() const;
    int indexForVisiblePosition(const VisiblePosition&) const;
    AXLayoutObject* getUnignoredObjectFromNode(Node&) const;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXLayoutObject, isAXLayoutObject());

} // namespace blink

#endif // AXLayoutObject_h
