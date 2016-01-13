/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef AXObjectCache_h
#define AXObjectCache_h

#include "core/accessibility/AXObject.h"
#include "core/rendering/RenderText.h"
#include "platform/Timer.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class AbstractInlineTextBox;
class Document;
class HTMLAreaElement;
class Node;
class Page;
class RenderObject;
class ScrollView;
class VisiblePosition;
class Widget;

struct TextMarkerData {
    AXID axID;
    Node* node;
    int offset;
    EAffinity affinity;
};

class AXComputedObjectAttributeCache {
public:
    static PassOwnPtr<AXComputedObjectAttributeCache> create() { return adoptPtr(new AXComputedObjectAttributeCache()); }

    AXObjectInclusion getIgnored(AXID) const;
    void setIgnored(AXID, AXObjectInclusion);

    void clear();

private:
    AXComputedObjectAttributeCache() { }

    struct CachedAXObjectAttributes {
        CachedAXObjectAttributes() : ignored(DefaultBehavior) { }

        AXObjectInclusion ignored;
    };

    HashMap<AXID, CachedAXObjectAttributes> m_idMapping;
};

enum PostType { PostSynchronously, PostAsynchronously };

class AXObjectCache {
    WTF_MAKE_NONCOPYABLE(AXObjectCache); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit AXObjectCache(Document&);
    ~AXObjectCache();

    static AXObject* focusedUIElementForPage(const Page*);

    // Returns the root object for the entire document.
    AXObject* rootObject();

    // For AX objects with elements that back them.
    AXObject* getOrCreate(RenderObject*);
    AXObject* getOrCreate(Widget*);
    AXObject* getOrCreate(Node*);
    AXObject* getOrCreate(AbstractInlineTextBox*);

    // used for objects without backing elements
    AXObject* getOrCreate(AccessibilityRole);

    // will only return the AXObject if it already exists
    AXObject* get(RenderObject*);
    AXObject* get(Widget*);
    AXObject* get(Node*);
    AXObject* get(AbstractInlineTextBox*);

    void remove(RenderObject*);
    void remove(Node*);
    void remove(Widget*);
    void remove(AbstractInlineTextBox*);
    void remove(AXID);

    void clearWeakMembers(Visitor*);

    void detachWrapper(AXObject*);
    void attachWrapper(AXObject*);
    void childrenChanged(Node*);
    void childrenChanged(RenderObject*);
    void childrenChanged(AXObject*);
    void checkedStateChanged(Node*);
    void selectedChildrenChanged(Node*);
    void selectedChildrenChanged(RenderObject*);
    void selectionChanged(Node*);
    // Called by a node when text or a text equivalent (e.g. alt) attribute is changed.
    void textChanged(Node*);
    void textChanged(RenderObject*);
    // Called when a node has just been attached, so we can make sure we have the right subclass of AXObject.
    void updateCacheAfterNodeIsAttached(Node*);

    void handleActiveDescendantChanged(Node*);
    void handleAriaRoleChanged(Node*);
    void handleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode);
    void handleScrolledToAnchor(const Node* anchorNode);
    void handleAriaExpandedChange(Node*);

    // Called when scroll bars are added / removed (as the view resizes).
    void handleScrollbarUpdate(ScrollView*);

    void handleLayoutComplete(RenderObject*);

    // Called when the scroll offset changes.
    void handleScrollPositionChanged(ScrollView*);
    void handleScrollPositionChanged(RenderObject*);

    void handleAttributeChanged(const QualifiedName& attrName, Element*);
    void recomputeIsIgnored(RenderObject* renderer);

    void inlineTextBoxesUpdated(RenderObject* renderer);

    static void enableAccessibility() { gAccessibilityEnabled = true; }
    static bool accessibilityEnabled() { return gAccessibilityEnabled; }
    static void setInlineTextBoxAccessibility(bool flag) { gInlineTextBoxAccessibility = flag; }
    static bool inlineTextBoxAccessibility() { return gInlineTextBoxAccessibility; }

    void removeAXID(AXObject*);
    bool isIDinUse(AXID id) const { return m_idsInUse.contains(id); }

    Element* rootAXEditableElement(Node*);
    const Element* rootAXEditableElement(const Node*);
    bool nodeIsTextControl(const Node*);

    AXID platformGenerateAXID() const;
    AXObject* objectFromAXID(AXID id) const { return m_objects.get(id); }

    enum AXNotification {
        AXActiveDescendantChanged,
        AXAlert,
        AXAriaAttributeChanged,
        AXAutocorrectionOccured,
        AXBlur,
        AXCheckedStateChanged,
        AXChildrenChanged,
        AXFocusedUIElementChanged,
        AXHide,
        AXInvalidStatusChanged,
        AXLayoutComplete,
        AXLiveRegionChanged,
        AXLoadComplete,
        AXLocationChanged,
        AXMenuListItemSelected,
        AXMenuListValueChanged,
        AXRowCollapsed,
        AXRowCountChanged,
        AXRowExpanded,
        AXScrollPositionChanged,
        AXScrolledToAnchor,
        AXSelectedChildrenChanged,
        AXSelectedTextChanged,
        AXShow,
        AXTextChanged,
        AXTextInserted,
        AXTextRemoved,
        AXValueChanged
    };

    void postNotification(RenderObject*, AXNotification, bool postToElement, PostType = PostAsynchronously);
    void postNotification(Node*, AXNotification, bool postToElement, PostType = PostAsynchronously);
    void postNotification(AXObject*, Document*, AXNotification, bool postToElement, PostType = PostAsynchronously);

    bool nodeHasRole(Node*, const AtomicString& role);

    AXComputedObjectAttributeCache* computedObjectAttributeCache() { return m_computedObjectAttributeCache.get(); }

protected:
    void postPlatformNotification(AXObject*, AXNotification);
    void textChanged(AXObject*);
    void labelChanged(Element*);

    // This is a weak reference cache for knowing if Nodes used by TextMarkers are valid.
    void setNodeInUse(Node* n) { m_textMarkerNodes.add(n); }
    void removeNodeForUse(Node* n) { m_textMarkerNodes.remove(n); }
    bool isNodeInUse(Node* n) { return m_textMarkerNodes.contains(n); }

private:
    Document& m_document;
    HashMap<AXID, RefPtr<AXObject> > m_objects;
    HashMap<RenderObject*, AXID> m_renderObjectMapping;
    HashMap<Widget*, AXID> m_widgetObjectMapping;
    HashMap<Node*, AXID> m_nodeObjectMapping;
    HashMap<AbstractInlineTextBox*, AXID> m_inlineTextBoxObjectMapping;
    HashSet<Node*> m_textMarkerNodes;
    OwnPtr<AXComputedObjectAttributeCache> m_computedObjectAttributeCache;
    static bool gAccessibilityEnabled;
    static bool gInlineTextBoxAccessibility;

    HashSet<AXID> m_idsInUse;

    Timer<AXObjectCache> m_notificationPostTimer;
    Vector<pair<RefPtr<AXObject>, AXNotification> > m_notificationsToPost;
    void notificationPostTimerFired(Timer<AXObjectCache>*);

    static AXObject* focusedImageMapUIElement(HTMLAreaElement*);

    AXID getAXID(AXObject*);
};

bool nodeHasRole(Node*, const String& role);
// This will let you know if aria-hidden was explicitly set to false.
bool isNodeAriaVisible(Node*);

}

#endif
