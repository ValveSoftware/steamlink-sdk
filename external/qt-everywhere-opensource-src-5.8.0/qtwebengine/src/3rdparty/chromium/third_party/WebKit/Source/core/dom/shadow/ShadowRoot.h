/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef ShadowRoot_h
#define ShadowRoot_h

#include "core/CoreExport.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/Element.h"
#include "core/dom/TreeScope.h"

namespace blink {

class Document;
class ElementShadow;
class ExceptionState;
class HTMLShadowElement;
class HTMLSlotElement;
class InsertionPoint;
class ShadowRootRareDataV0;
class SlotAssignment;
class StyleSheetList;

enum class ShadowRootType {
    UserAgent,
    V0,
    Open,
    Closed
};

class CORE_EXPORT ShadowRoot final : public DocumentFragment, public TreeScope {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(ShadowRoot);
public:
    // FIXME: Current implementation does not work well if a shadow root is dynamically created.
    // So multiple shadow subtrees in several elements are prohibited.
    // See https://github.com/w3c/webcomponents/issues/102 and http://crbug.com/234020
    static ShadowRoot* create(Document& document, ShadowRootType type)
    {
        return new ShadowRoot(document, type);
    }

    // Disambiguate between Node and TreeScope hierarchies; TreeScope's implementation is simpler.
    using TreeScope::document;
    using TreeScope::getElementById;

    // Make protected methods from base class public here.
    using TreeScope::setDocument;
    using TreeScope::setParentTreeScope;

    Element& host() const { DCHECK(parentOrShadowHostNode()); return *toElement(parentOrShadowHostNode()); }
    ElementShadow* owner() const { return host().shadow(); }
    ShadowRootType type() const { return static_cast<ShadowRootType>(m_type); }
    String mode() const { return (type() == ShadowRootType::V0 || type() == ShadowRootType::Open) ? "open" : "closed"; };

    bool isOpenOrV0() const { return type() == ShadowRootType::V0 || type() == ShadowRootType::Open; }
    bool isV1() const { return type() == ShadowRootType::Open || type() == ShadowRootType::Closed; }

    void attach(const AttachContext& = AttachContext()) override;
    void detach(const AttachContext& = AttachContext()) override;

    InsertionNotificationRequest insertedInto(ContainerNode*) override;
    void removedFrom(ContainerNode*) override;

    // For V0
    ShadowRoot* youngerShadowRoot() const;
    ShadowRoot* olderShadowRoot() const;
    ShadowRoot* olderShadowRootForBindings() const;
    void setYoungerShadowRoot(ShadowRoot&);
    void setOlderShadowRoot(ShadowRoot&);
    bool isYoungest() const { return !youngerShadowRoot(); }
    bool isOldest() const { return !olderShadowRoot(); }
    bool containsShadowElements() const;
    bool containsContentElements() const;
    bool containsInsertionPoints() const { return containsShadowElements() || containsContentElements(); }
    unsigned descendantShadowElementCount() const;
    HTMLShadowElement* shadowInsertionPointOfYoungerShadowRoot() const;
    void setShadowInsertionPointOfYoungerShadowRoot(HTMLShadowElement*);
    void didAddInsertionPoint(InsertionPoint*);
    void didRemoveInsertionPoint(InsertionPoint*);
    const HeapVector<Member<InsertionPoint>>& descendantInsertionPoints();

    // For Internals, don't use this.
    unsigned childShadowRootCount() const { return m_childShadowRootCount; }
    unsigned numberOfStyles() const { return m_numberOfStyles; }

    void recalcStyle(StyleRecalcChange);

    void registerScopedHTMLStyleChild();
    void unregisterScopedHTMLStyleChild();

    SlotAssignment& ensureSlotAssignment();

    void distributeV1();

    Element* activeElement() const;

    String innerHTML() const;
    void setInnerHTML(const String&, ExceptionState&);

    Node* cloneNode(bool, ExceptionState&);

    void setDelegatesFocus(bool flag) { m_delegatesFocus = flag; }
    bool delegatesFocus() const { return m_delegatesFocus; }

    bool containsShadowRoots() const { return m_childShadowRootCount; }

    StyleSheetList& styleSheets();
    void setStyleSheets(StyleSheetList* styleSheetList) { m_styleSheetList = styleSheetList; }

    DECLARE_VIRTUAL_TRACE();

    DECLARE_VIRTUAL_TRACE_WRAPPERS();

private:
    ShadowRoot(Document&, ShadowRootType);
    ~ShadowRoot() override;

    void childrenChanged(const ChildrenChange&) override;

    ShadowRootRareDataV0& ensureShadowRootRareDataV0();

    void addChildShadowRoot() { ++m_childShadowRootCount; }
    void removeChildShadowRoot() { DCHECK_GT(m_childShadowRootCount, 0u); --m_childShadowRootCount; }
    void invalidateDescendantInsertionPoints();

    // ShadowRoots should never be cloned.
    Node* cloneNode(bool) override { return nullptr; }

    Member<ShadowRootRareDataV0> m_shadowRootRareDataV0;
    Member<StyleSheetList> m_styleSheetList;
    Member<SlotAssignment> m_slotAssignment;
    unsigned m_numberOfStyles : 14;
    unsigned m_childShadowRootCount : 13;
    unsigned m_type : 2;
    unsigned m_registeredWithParentShadowRoot : 1;
    unsigned m_descendantInsertionPointsIsValid : 1;
    unsigned m_delegatesFocus : 1;
};

inline Element* ShadowRoot::activeElement() const
{
    return adjustedFocusedElement();
}

inline ShadowRoot* Element::shadowRootIfV1() const
{
    ShadowRoot* root = this->shadowRoot();
    if (root && root->isV1())
        return root;
    return nullptr;
}

DEFINE_NODE_TYPE_CASTS(ShadowRoot, isShadowRoot());
DEFINE_TYPE_CASTS(ShadowRoot, TreeScope, treeScope, treeScope->rootNode().isShadowRoot(), treeScope.rootNode().isShadowRoot());
DEFINE_TYPE_CASTS(TreeScope, ShadowRoot, shadowRoot, true, true);

CORE_EXPORT std::ostream& operator<<(std::ostream&, const ShadowRootType&);

} // namespace blink

#endif // ShadowRoot_h
