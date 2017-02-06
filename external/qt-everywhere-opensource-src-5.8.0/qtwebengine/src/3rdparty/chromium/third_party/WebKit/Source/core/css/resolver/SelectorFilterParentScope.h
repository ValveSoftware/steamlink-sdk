// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SelectorFilterParentScope_h
#define SelectorFilterParentScope_h

#include "core/css/SelectorFilter.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"

namespace blink {

// Maintains the parent element stack (and bloom filter) inside recalcStyle.
class SelectorFilterParentScope final {
    STACK_ALLOCATED();
public:
    explicit SelectorFilterParentScope(Element& parent);
    ~SelectorFilterParentScope();

    static void ensureParentStackIsPushed();

private:
    void pushParentIfNeeded();

    Member<Element> m_parent;
    bool m_pushed;
    SelectorFilterParentScope* m_previous;
    Member<StyleResolver> m_resolver;

    static SelectorFilterParentScope* s_currentScope;
};

inline SelectorFilterParentScope::SelectorFilterParentScope(Element& parent)
    : m_parent(parent)
    , m_pushed(false)
    , m_previous(s_currentScope)
    , m_resolver(parent.document().styleResolver())
{
    ASSERT(parent.document().inStyleRecalc());
    s_currentScope = this;
}

inline SelectorFilterParentScope::~SelectorFilterParentScope()
{
    s_currentScope = m_previous;
    if (!m_pushed)
        return;
    m_resolver->selectorFilter().popParent(*m_parent);
}

inline void SelectorFilterParentScope::ensureParentStackIsPushed()
{
    if (s_currentScope)
        s_currentScope->pushParentIfNeeded();
}

inline void SelectorFilterParentScope::pushParentIfNeeded()
{
    if (m_pushed)
        return;
    if (m_previous)
        m_previous->pushParentIfNeeded();
    m_resolver->selectorFilter().pushParent(*m_parent);
    m_pushed = true;
}

} // namespace blink

#endif // SelectorFilterParentScope_h
