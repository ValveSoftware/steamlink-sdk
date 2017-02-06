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

#ifndef WebAXObject_h
#define WebAXObject_h

#include "../platform/WebCommon.h"
#include "../platform/WebPoint.h"
#include "../platform/WebPrivateOwnPtr.h"
#include "../platform/WebPrivatePtr.h"
#include "../platform/WebVector.h"
#include "WebAXEnums.h"

#if BLINK_IMPLEMENTATION
namespace WTF { template <typename T> class PassRefPtr; }
#endif

class SkMatrix44;

namespace blink {

class AXObject;
class ScopedAXObjectCache;
class WebAXObject;
class WebNode;
class WebDocument;
class WebString;
class WebURL;
struct WebFloatRect;
struct WebPoint;
struct WebRect;

// An instance of this class, while kept alive, indicates that accessibility
// should be temporarily enabled. If accessibility was enabled globally
// (WebSettings::setAccessibilityEnabled), this will have no effect.
class WebScopedAXContext {
public:
    BLINK_EXPORT WebScopedAXContext(WebDocument& rootDocument);
    BLINK_EXPORT ~WebScopedAXContext();

    BLINK_EXPORT WebAXObject root() const;

private:
    WebPrivateOwnPtr<ScopedAXObjectCache> m_private;
};

// A container for passing around a reference to AXObject.
class WebAXObject {
public:
    ~WebAXObject() { reset(); }

    WebAXObject() { }
    WebAXObject(const WebAXObject& o) { assign(o); }
    WebAXObject& operator=(const WebAXObject& o)
    {
        assign(o);
        return *this;
    }

    BLINK_EXPORT void reset();
    BLINK_EXPORT void assign(const WebAXObject&);
    BLINK_EXPORT bool equals(const WebAXObject&) const;

    bool isNull() const { return m_private.isNull(); }
    // isDetached also checks for null, so it's safe to just call isDetached.
    BLINK_EXPORT bool isDetached() const;

    BLINK_EXPORT int axID() const;

    // Get a new AXID that's not used by any accessibility node in this process, for when the
    // client needs to insert additional nodes into the accessibility tree.
    BLINK_EXPORT int generateAXID() const;

    // Update layout on the underlying tree, and return true if this object is
    // still valid (not detached). Note that calling this method
    // can cause other WebAXObjects to become invalid, too,
    // so always call isDetached if any other WebCore code has run.
    BLINK_EXPORT bool updateLayoutAndCheckValidity();

    BLINK_EXPORT unsigned childCount() const;

    BLINK_EXPORT WebAXObject childAt(unsigned) const;
    BLINK_EXPORT WebAXObject parentObject() const;

    BLINK_EXPORT bool isAnchor() const;
    BLINK_EXPORT bool isAriaReadOnly() const;
    BLINK_EXPORT bool isButtonStateMixed() const;
    BLINK_EXPORT bool isChecked() const;
    BLINK_EXPORT bool isClickable() const;
    BLINK_EXPORT bool isCollapsed() const;
    BLINK_EXPORT bool isControl() const;
    BLINK_EXPORT bool isEnabled() const;
    BLINK_EXPORT WebAXExpanded isExpanded() const;
    BLINK_EXPORT bool isFocused() const;
    BLINK_EXPORT bool isHovered() const;
    BLINK_EXPORT bool isLinked() const;
    BLINK_EXPORT bool isLoaded() const;
    BLINK_EXPORT bool isMultiSelectable() const;
    BLINK_EXPORT bool isOffScreen() const;
    BLINK_EXPORT bool isPasswordField() const;
    BLINK_EXPORT bool isPressed() const;
    BLINK_EXPORT bool isReadOnly() const;
    BLINK_EXPORT bool isRequired() const;
    BLINK_EXPORT bool isSelected() const;
    BLINK_EXPORT bool isSelectedOptionActive() const;
    BLINK_EXPORT bool isVisible() const;
    BLINK_EXPORT bool isVisited() const;

    BLINK_EXPORT WebString accessKey() const;
    BLINK_EXPORT unsigned backgroundColor() const;
    BLINK_EXPORT unsigned color() const;
    // Deprecated.
    BLINK_EXPORT void colorValue(int& r, int& g, int& b) const;
    BLINK_EXPORT unsigned colorValue() const;
    BLINK_EXPORT WebAXObject ariaActiveDescendant() const;
    BLINK_EXPORT WebString ariaAutoComplete() const;
    BLINK_EXPORT bool ariaControls(WebVector<WebAXObject>& controlsElements) const;
    BLINK_EXPORT WebAXAriaCurrentState ariaCurrentState() const;
    BLINK_EXPORT bool ariaFlowTo(WebVector<WebAXObject>& flowToElements) const;
    BLINK_EXPORT bool ariaHasPopup() const;
    BLINK_EXPORT bool isEditable() const;
    BLINK_EXPORT bool isMultiline() const;
    BLINK_EXPORT bool isRichlyEditable() const;
    BLINK_EXPORT bool ariaOwns(WebVector<WebAXObject>& ownsElements) const;
    BLINK_EXPORT WebString fontFamily() const;
    BLINK_EXPORT float fontSize() const;
    BLINK_EXPORT bool canvasHasFallbackContent() const;
    BLINK_EXPORT WebPoint clickPoint() const;
    BLINK_EXPORT WebAXInvalidState invalidState() const;
    // Only used when invalidState() returns WebAXInvalidStateOther.
    BLINK_EXPORT WebString ariaInvalidValue() const;
    BLINK_EXPORT double estimatedLoadingProgress() const;
    BLINK_EXPORT int headingLevel() const;
    BLINK_EXPORT int hierarchicalLevel() const;
    BLINK_EXPORT WebAXObject hitTest(const WebPoint&) const;
    BLINK_EXPORT WebString keyboardShortcut() const;
    BLINK_EXPORT WebString language() const;
    BLINK_EXPORT WebAXOrientation orientation() const;
    BLINK_EXPORT WebAXRole role() const;
    BLINK_EXPORT WebString stringValue() const;
    BLINK_EXPORT WebAXTextDirection textDirection() const;
    BLINK_EXPORT WebAXTextStyle textStyle() const;
    BLINK_EXPORT WebURL url() const;

    // Retrieves the accessible name of the object, an enum indicating where the name
    // was derived from, and a list of related objects that were used to derive the name, if any.
    BLINK_EXPORT WebString name(WebAXNameFrom&, WebVector<WebAXObject>& nameObjects) const;
    // Simplified version of |name| when nameFrom and nameObjects aren't needed.
    BLINK_EXPORT WebString name() const;
    // Takes the result of nameFrom from calling |name|, above, and retrieves the
    // accessible description of the object, which is secondary to |name|, an enum indicating
    // where the description was derived from, and a list of objects that were used to
    // derive the description, if any.
    BLINK_EXPORT WebString description(WebAXNameFrom, WebAXDescriptionFrom&, WebVector<WebAXObject>& descriptionObjects) const;
    // Takes the result of nameFrom and descriptionFrom from calling |name| and |description|,
    // above, and retrieves the placeholder of the object, if present and if it wasn't already
    // exposed by one of the two functions above.
    BLINK_EXPORT WebString placeholder(WebAXNameFrom, WebAXDescriptionFrom) const;

    // The following selection functions get or set the global document
    // selection and can be called on any object in the tree.
    BLINK_EXPORT void selection(WebAXObject& anchorObject, int& anchorOffset,
        WebAXObject& focusObject, int& focusOffset) const;
    BLINK_EXPORT void setSelection(const WebAXObject& anchorObject, int anchorOffset,
        const WebAXObject& focusObject, int focusOffset) const;

    // The following selection functions return text offsets calculated starting
    // the current object. They only report on a selection that is placed on
    // the current object or on any of its descendants.
    BLINK_EXPORT unsigned selectionEnd() const;
    BLINK_EXPORT unsigned selectionEndLineNumber() const;
    BLINK_EXPORT unsigned selectionStart() const;
    BLINK_EXPORT unsigned selectionStartLineNumber() const;

    // 1-based position in set & Size of set.
    BLINK_EXPORT int posInSet() const;
    BLINK_EXPORT int setSize() const;

    // Live regions.
    BLINK_EXPORT bool isInLiveRegion() const;
    BLINK_EXPORT bool liveRegionAtomic() const;
    BLINK_EXPORT bool liveRegionBusy() const;
    BLINK_EXPORT WebString liveRegionRelevant() const;
    BLINK_EXPORT WebString liveRegionStatus() const;
    BLINK_EXPORT WebAXObject liveRegionRoot() const;
    BLINK_EXPORT bool containerLiveRegionAtomic() const;
    BLINK_EXPORT bool containerLiveRegionBusy() const;
    BLINK_EXPORT WebString containerLiveRegionRelevant() const;
    BLINK_EXPORT WebString containerLiveRegionStatus() const;

    BLINK_EXPORT bool supportsRangeValue() const;
    BLINK_EXPORT WebString valueDescription() const;
    BLINK_EXPORT float valueForRange() const;
    BLINK_EXPORT float maxValueForRange() const;
    BLINK_EXPORT float minValueForRange() const;

    BLINK_EXPORT WebNode node() const;
    BLINK_EXPORT WebDocument document() const;
    BLINK_EXPORT bool hasComputedStyle() const;
    BLINK_EXPORT WebString computedStyleDisplay() const;
    BLINK_EXPORT bool accessibilityIsIgnored() const;
    BLINK_EXPORT bool lineBreaks(WebVector<int>&) const;
    BLINK_EXPORT void markers(
        WebVector<WebAXMarkerType>& types,
        WebVector<int>& starts,
        WebVector<int>& ends) const;

    // Actions
    BLINK_EXPORT WebString actionVerb() const; // The verb corresponding to performDefaultAction.
    BLINK_EXPORT bool canDecrement() const;
    BLINK_EXPORT bool canIncrement() const;
    BLINK_EXPORT bool canPress() const;
    BLINK_EXPORT bool canSetFocusAttribute() const;
    BLINK_EXPORT bool canSetSelectedAttribute() const;
    BLINK_EXPORT bool canSetValueAttribute() const;
    BLINK_EXPORT bool performDefaultAction() const;
    BLINK_EXPORT bool press() const;
    BLINK_EXPORT bool increment() const;
    BLINK_EXPORT bool decrement() const;
    BLINK_EXPORT void setFocused(bool) const;
    BLINK_EXPORT void setSelectedTextRange(int selectionStart, int selectionEnd) const;
    BLINK_EXPORT void setValue(WebString) const;
    BLINK_EXPORT void showContextMenu() const;

    // For a table
    BLINK_EXPORT unsigned columnCount() const;
    BLINK_EXPORT unsigned rowCount() const;
    BLINK_EXPORT WebAXObject cellForColumnAndRow(unsigned column, unsigned row) const;
    BLINK_EXPORT WebAXObject headerContainerObject() const;
    BLINK_EXPORT WebAXObject rowAtIndex(unsigned rowIndex) const;
    BLINK_EXPORT WebAXObject columnAtIndex(unsigned columnIndex) const;
    BLINK_EXPORT void rowHeaders(WebVector<WebAXObject>&) const;
    BLINK_EXPORT void columnHeaders(WebVector<WebAXObject>&) const;

    // For a table row
    BLINK_EXPORT unsigned rowIndex() const;
    BLINK_EXPORT WebAXObject rowHeader() const;

    // For a table column
    BLINK_EXPORT unsigned columnIndex() const;
    BLINK_EXPORT WebAXObject columnHeader() const;

    // For a table cell
    BLINK_EXPORT unsigned cellColumnIndex() const;
    BLINK_EXPORT unsigned cellColumnSpan() const;
    BLINK_EXPORT unsigned cellRowIndex() const;
    BLINK_EXPORT unsigned cellRowSpan() const;
    BLINK_EXPORT WebAXSortDirection sortDirection() const;

    // Load inline text boxes for just this subtree, even if
    // settings->inlineTextBoxAccessibilityEnabled() is false.
    BLINK_EXPORT void loadInlineTextBoxes() const;

    // Walk the WebAXObjects on the same line. This is supported on any
    // object type but primarily intended to be used for inline text boxes.
    BLINK_EXPORT WebAXObject nextOnLine() const;
    BLINK_EXPORT WebAXObject previousOnLine() const;

    // For an inline text box.
    BLINK_EXPORT void characterOffsets(WebVector<int>&) const;
    BLINK_EXPORT void wordBoundaries(WebVector<int>& starts, WebVector<int>& ends) const;

    // Scrollable containers.
    BLINK_EXPORT bool isScrollableContainer() const;
    BLINK_EXPORT WebPoint scrollOffset() const;
    BLINK_EXPORT WebPoint minimumScrollOffset() const;
    BLINK_EXPORT WebPoint maximumScrollOffset() const;
    BLINK_EXPORT void setScrollOffset(const WebPoint&) const;

    // Old bounds calculation interface. DEPRECATED, to be replaced with getRelativeBounds.
    BLINK_EXPORT WebRect boundingBoxRect() const;

    // NEW bounds calculation interface. Every object's bounding box is returned
    // relative to a container object (which is guaranteed to be an ancestor) and
    // optionally a transformation matrix that needs to be applied too.
    // To compute the absolute bounding box of an element, start with its
    // boundsInContainer and apply the transform. Then as long as its container is
    // not null, walk up to its container and offset by the container's offset from
    // origin, the container's scroll position if any, and apply the container's transform.
    // Do this until you reach the root of the tree.
    BLINK_EXPORT void getRelativeBounds(WebAXObject& offsetContainer, WebFloatRect& boundsInContainer, SkMatrix44& containerTransform) const;

    // Transformation relative to the parent frame, if local (otherwise returns identity).
    // DEPRECATED, to be replaced with getRelativeBounds.
    BLINK_EXPORT SkMatrix44 transformFromLocalParentFrame() const;

    // Make this object visible by scrolling as many nested scrollable views as needed.
    BLINK_EXPORT void scrollToMakeVisible() const;
    // Same, but if the whole object can't be made visible, try for this subrect, in local coordinates.
    BLINK_EXPORT void scrollToMakeVisibleWithSubFocus(const WebRect&) const;
    // Scroll this object to a given point in global coordinates of the top-level window.
    BLINK_EXPORT void scrollToGlobalPoint(const WebPoint&) const;

#if BLINK_IMPLEMENTATION
    WebAXObject(AXObject*);
    WebAXObject& operator=(AXObject*);
    operator AXObject*() const;
#endif

private:
    WebPrivatePtr<AXObject> m_private;
};

} // namespace blink

#endif
