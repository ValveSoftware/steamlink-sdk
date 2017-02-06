/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TreeScope_h
#define TreeScope_h

#include "core/CoreExport.h"
#include "core/dom/DocumentOrderedMap.h"
#include "core/html/forms/RadioButtonGroupScope.h"
#include "core/layout/HitTestRequest.h"
#include "platform/heap/Handle.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class ContainerNode;
class DOMSelection;
class Document;
class Element;
class HTMLMapElement;
class HitTestResult;
class IdTargetObserverRegistry;
class ScopedStyleResolver;
class Node;

// A class which inherits both Node and TreeScope must call clearRareData() in its destructor
// so that the Node destructor no longer does problematic NodeList cache manipulation in
// the destructor.
class CORE_EXPORT TreeScope : public GarbageCollectedMixin {
public:
    TreeScope* parentTreeScope() const { return m_parentTreeScope; }

    TreeScope* olderShadowRootOrParentTreeScope() const;
    bool isInclusiveOlderSiblingShadowRootOrAncestorTreeScopeOf(const TreeScope&) const;

    Element* adjustedFocusedElement() const;
    Element* adjustedPointerLockElement(const Element&) const;
    Element* getElementById(const AtomicString&) const;
    const HeapVector<Member<Element>>& getAllElementsById(const AtomicString&) const;
    bool hasElementWithId(const AtomicString& id) const;
    bool containsMultipleElementsWithId(const AtomicString& id) const;
    void addElementById(const AtomicString& elementId, Element*);
    void removeElementById(const AtomicString& elementId, Element*);

    Document& document() const
    {
        DCHECK(m_document);
        return *m_document;
    }

    Node* ancestorInThisScope(Node*) const;

    void addImageMap(HTMLMapElement*);
    void removeImageMap(HTMLMapElement*);
    HTMLMapElement* getImageMap(const String& url) const;

    Element* elementFromPoint(int x, int y) const;
    Element* hitTestPoint(int x, int y, const HitTestRequest&) const;
    HeapVector<Member<Element>> elementsFromPoint(int x, int y) const;
    HeapVector<Member<Element>> elementsFromHitTestResult(HitTestResult&) const;

    DOMSelection* getSelection() const;

    Element* retarget(const Element& target) const;

    // Find first anchor with the given name.
    // First searches for an element with the given ID, but if that fails, then looks
    // for an anchor with the given name. ID matching is always case sensitive, but
    // Anchor name matching is case sensitive in strict mode and not case sensitive in
    // quirks mode for historical compatibility reasons.
    Element* findAnchor(const String& name);

    // Used by the basic DOM mutation methods (e.g., appendChild()).
    void adoptIfNeeded(Node&);

    ContainerNode& rootNode() const { return *m_rootNode; }

    IdTargetObserverRegistry& idTargetObserverRegistry() const { return *m_idTargetObserverRegistry.get(); }

    RadioButtonGroupScope& radioButtonGroupScope() { return m_radioButtonGroupScope; }

    bool isInclusiveAncestorOf(const TreeScope&) const;
    unsigned short comparePosition(const TreeScope&) const;

    const TreeScope* commonAncestorTreeScope(const TreeScope& other) const;
    TreeScope* commonAncestorTreeScope(TreeScope& other);

    Element* getElementByAccessKey(const String& key) const;

    DECLARE_VIRTUAL_TRACE();

    ScopedStyleResolver* scopedStyleResolver() const { return m_scopedStyleResolver.get(); }
    ScopedStyleResolver& ensureScopedStyleResolver();
    void clearScopedStyleResolver();

protected:
    TreeScope(ContainerNode&, Document&);
    TreeScope(Document&);
    virtual ~TreeScope();

    void setDocument(Document& document) { m_document = &document; }
    void setParentTreeScope(TreeScope&);
    void setNeedsStyleRecalcForViewportUnits();

private:
    Member<ContainerNode> m_rootNode;
    Member<Document> m_document;
    Member<TreeScope> m_parentTreeScope;

    Member<DocumentOrderedMap> m_elementsById;
    Member<DocumentOrderedMap> m_imageMapsByName;

    Member<IdTargetObserverRegistry> m_idTargetObserverRegistry;

    Member<ScopedStyleResolver> m_scopedStyleResolver;

    mutable Member<DOMSelection> m_selection;

    RadioButtonGroupScope m_radioButtonGroupScope;
};

inline bool TreeScope::hasElementWithId(const AtomicString& id) const
{
    DCHECK(!id.isNull());
    return m_elementsById && m_elementsById->contains(id);
}

inline bool TreeScope::containsMultipleElementsWithId(const AtomicString& id) const
{
    return m_elementsById && m_elementsById->containsMultiple(id);
}

DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(TreeScope)

HitTestResult hitTestInDocument(const Document*, int x, int y, const HitTestRequest& = HitTestRequest::ReadOnly | HitTestRequest::Active);

} // namespace blink

#endif // TreeScope_h
