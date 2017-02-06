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

#include "core/dom/shadow/ShadowRoot.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/StyleSheetList.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleSharingDepthScope.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRootRareDataV0.h"
#include "core/dom/shadow/SlotAssignment.h"
#include "core/editing/serializers/Serialization.h"
#include "core/html/HTMLShadowElement.h"
#include "core/html/HTMLSlotElement.h"
#include "public/platform/Platform.h"

namespace blink {

struct SameSizeAsShadowRoot : public DocumentFragment, public TreeScope {
    char emptyClassFieldsDueToGCMixinMarker[1];
    Member<void*> willbeMember[3];
    unsigned countersAndFlags[1];
};

static_assert(sizeof(ShadowRoot) == sizeof(SameSizeAsShadowRoot), "ShadowRoot should stay small");

ShadowRoot::ShadowRoot(Document& document, ShadowRootType type)
    : DocumentFragment(0, CreateShadowRoot)
    , TreeScope(*this, document)
    , m_numberOfStyles(0)
    , m_childShadowRootCount(0)
    , m_type(static_cast<unsigned>(type))
    , m_registeredWithParentShadowRoot(false)
    , m_descendantInsertionPointsIsValid(false)
    , m_delegatesFocus(false)
{
}

ShadowRoot::~ShadowRoot()
{
}

ShadowRoot* ShadowRoot::youngerShadowRoot() const
{
    if (type() == ShadowRootType::V0 && m_shadowRootRareDataV0)
        return m_shadowRootRareDataV0->youngerShadowRoot();
    return nullptr;
}

ShadowRoot* ShadowRoot::olderShadowRoot() const
{
    if (type() == ShadowRootType::V0 && m_shadowRootRareDataV0)
        return m_shadowRootRareDataV0->olderShadowRoot();
    return nullptr;
}

ShadowRoot* ShadowRoot::olderShadowRootForBindings() const
{
    ShadowRoot* older = olderShadowRoot();
    while (older && !older->isOpenOrV0())
        older = older->olderShadowRoot();
    DCHECK(!older || older->isOpenOrV0());
    return older;
}

void ShadowRoot::setYoungerShadowRoot(ShadowRoot& root)
{
    DCHECK_EQ(type(), ShadowRootType::V0);
    ensureShadowRootRareDataV0().setYoungerShadowRoot(root);
}

void ShadowRoot::setOlderShadowRoot(ShadowRoot& root)
{
    DCHECK_EQ(type(), ShadowRootType::V0);
    ensureShadowRootRareDataV0().setOlderShadowRoot(root);
}

SlotAssignment& ShadowRoot::ensureSlotAssignment()
{
    if (!m_slotAssignment)
        m_slotAssignment = SlotAssignment::create(*this);
    return *m_slotAssignment;
}

Node* ShadowRoot::cloneNode(bool, ExceptionState& exceptionState)
{
    exceptionState.throwDOMException(NotSupportedError, "ShadowRoot nodes are not clonable.");
    return nullptr;
}

String ShadowRoot::innerHTML() const
{
    return createMarkup(this, ChildrenOnly);
}

void ShadowRoot::setInnerHTML(const String& markup, ExceptionState& exceptionState)
{
    if (DocumentFragment* fragment = createFragmentForInnerOuterHTML(markup, &host(), AllowScriptingContent, "innerHTML", exceptionState))
        replaceChildrenWithFragment(this, fragment, exceptionState);
}

void ShadowRoot::recalcStyle(StyleRecalcChange change)
{
    // ShadowRoot doesn't support custom callbacks.
    DCHECK(!hasCustomStyleCallbacks());

    StyleSharingDepthScope sharingScope(*this);

    if (getStyleChangeType() >= SubtreeStyleChange)
        change = Force;

    // There's no style to update so just calling recalcStyle means we're updated.
    clearNeedsStyleRecalc();

    recalcChildStyle(change);
    clearChildNeedsStyleRecalc();
}

void ShadowRoot::attach(const AttachContext& context)
{
    StyleSharingDepthScope sharingScope(*this);
    DocumentFragment::attach(context);
}

void ShadowRoot::detach(const AttachContext& context)
{
    if (context.clearInvalidation)
        document().styleEngine().styleInvalidator().clearInvalidation(*this);
    DocumentFragment::detach(context);
}

Node::InsertionNotificationRequest ShadowRoot::insertedInto(ContainerNode* insertionPoint)
{
    DocumentFragment::insertedInto(insertionPoint);

    if (!insertionPoint->inShadowIncludingDocument() || !isOldest())
        return InsertionDone;

    // FIXME: When parsing <video controls>, insertedInto() is called many times without invoking removedFrom.
    // For now, we check m_registeredWithParentShadowroot. We would like to DCHECK(!m_registeredShadowRoot) here.
    // https://bugs.webkit.org/show_bug.cig?id=101316
    if (m_registeredWithParentShadowRoot)
        return InsertionDone;

    if (ShadowRoot* root = host().containingShadowRoot()) {
        root->addChildShadowRoot();
        m_registeredWithParentShadowRoot = true;
    }

    return InsertionDone;
}

void ShadowRoot::removedFrom(ContainerNode* insertionPoint)
{
    if (insertionPoint->inShadowIncludingDocument()) {
        document().styleEngine().shadowRootRemovedFromDocument(this);
        if (m_registeredWithParentShadowRoot) {
            ShadowRoot* root = host().containingShadowRoot();
            if (!root)
                root = insertionPoint->containingShadowRoot();
            if (root)
                root->removeChildShadowRoot();
            m_registeredWithParentShadowRoot = false;
        }
        if (needsStyleInvalidation())
            document().styleEngine().styleInvalidator().clearInvalidation(*this);
    }

    DocumentFragment::removedFrom(insertionPoint);
}

void ShadowRoot::childrenChanged(const ChildrenChange& change)
{
    ContainerNode::childrenChanged(change);

    if (change.isChildElementChange())
        checkForSiblingStyleChanges(change.type == ElementRemoved ? SiblingElementRemoved : SiblingElementInserted, change.siblingChanged, change.siblingBeforeChange, change.siblingAfterChange);

    if (InsertionPoint* point = shadowInsertionPointOfYoungerShadowRoot()) {
        if (ShadowRoot* root = point->containingShadowRoot())
            root->owner()->setNeedsDistributionRecalc();
    }
}

void ShadowRoot::registerScopedHTMLStyleChild()
{
    ++m_numberOfStyles;
}

void ShadowRoot::unregisterScopedHTMLStyleChild()
{
    DCHECK_GT(m_numberOfStyles, 0u);
    --m_numberOfStyles;
}

ShadowRootRareDataV0& ShadowRoot::ensureShadowRootRareDataV0()
{
    if (m_shadowRootRareDataV0)
        return *m_shadowRootRareDataV0;

    m_shadowRootRareDataV0 = new ShadowRootRareDataV0;
    return *m_shadowRootRareDataV0;
}

bool ShadowRoot::containsShadowElements() const
{
    return m_shadowRootRareDataV0 ? m_shadowRootRareDataV0->containsShadowElements() : false;
}

bool ShadowRoot::containsContentElements() const
{
    return m_shadowRootRareDataV0 ? m_shadowRootRareDataV0->containsContentElements() : false;
}

unsigned ShadowRoot::descendantShadowElementCount() const
{
    return m_shadowRootRareDataV0 ? m_shadowRootRareDataV0->descendantShadowElementCount() : 0;
}

HTMLShadowElement* ShadowRoot::shadowInsertionPointOfYoungerShadowRoot() const
{
    return m_shadowRootRareDataV0 ? m_shadowRootRareDataV0->shadowInsertionPointOfYoungerShadowRoot() : nullptr;
}

void ShadowRoot::setShadowInsertionPointOfYoungerShadowRoot(HTMLShadowElement* shadowInsertionPoint)
{
    if (!m_shadowRootRareDataV0 && !shadowInsertionPoint)
        return;
    ensureShadowRootRareDataV0().setShadowInsertionPointOfYoungerShadowRoot(shadowInsertionPoint);
}

void ShadowRoot::didAddInsertionPoint(InsertionPoint* insertionPoint)
{
    ensureShadowRootRareDataV0().didAddInsertionPoint(insertionPoint);
    invalidateDescendantInsertionPoints();
}

void ShadowRoot::didRemoveInsertionPoint(InsertionPoint* insertionPoint)
{
    m_shadowRootRareDataV0->didRemoveInsertionPoint(insertionPoint);
    invalidateDescendantInsertionPoints();
}

void ShadowRoot::invalidateDescendantInsertionPoints()
{
    m_descendantInsertionPointsIsValid = false;
    m_shadowRootRareDataV0->clearDescendantInsertionPoints();
}

const HeapVector<Member<InsertionPoint>>& ShadowRoot::descendantInsertionPoints()
{
    DEFINE_STATIC_LOCAL(HeapVector<Member<InsertionPoint>>, emptyList, (new HeapVector<Member<InsertionPoint>>));
    if (m_shadowRootRareDataV0 && m_descendantInsertionPointsIsValid)
        return m_shadowRootRareDataV0->descendantInsertionPoints();

    m_descendantInsertionPointsIsValid = true;

    if (!containsInsertionPoints())
        return emptyList;

    HeapVector<Member<InsertionPoint>> insertionPoints;
    for (InsertionPoint& insertionPoint : Traversal<InsertionPoint>::descendantsOf(*this))
        insertionPoints.append(&insertionPoint);

    ensureShadowRootRareDataV0().setDescendantInsertionPoints(insertionPoints);

    return m_shadowRootRareDataV0->descendantInsertionPoints();
}

StyleSheetList& ShadowRoot::styleSheets()
{
    if (!m_styleSheetList)
        setStyleSheets(StyleSheetList::create(this));
    return *m_styleSheetList;
}

void ShadowRoot::distributeV1()
{
    ensureSlotAssignment().resolveDistribution();
}

DEFINE_TRACE(ShadowRoot)
{
    visitor->trace(m_shadowRootRareDataV0);
    visitor->trace(m_slotAssignment);
    visitor->trace(m_styleSheetList);
    TreeScope::trace(visitor);
    DocumentFragment::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(ShadowRoot)
{
    visitor->traceWrappers(m_styleSheetList);
    DocumentFragment::traceWrappers(visitor);
}

std::ostream& operator<<(std::ostream& ostream, const ShadowRootType& type)
{
    switch (type) {
    case ShadowRootType::UserAgent:
        ostream << "ShadowRootType::UserAgent";
        break;
    case ShadowRootType::V0:
        ostream << "ShadowRootType::V0";
        break;
    case ShadowRootType::Open:
        ostream << "ShadowRootType::Open";
        break;
    case ShadowRootType::Closed:
        ostream << "ShadowRootType::Closed";
        break;
    }
    return ostream;
}

} // namespace blink
