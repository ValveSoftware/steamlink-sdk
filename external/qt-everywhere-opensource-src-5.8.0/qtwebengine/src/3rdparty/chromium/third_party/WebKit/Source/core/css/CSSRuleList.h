/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2012 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef CSSRuleList_h
#define CSSRuleList_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class CSSRule;
class CSSStyleSheet;

class CSSRuleList : public GarbageCollected<CSSRuleList>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(CSSRuleList);
public:

    virtual unsigned length() const = 0;
    virtual CSSRule* item(unsigned index) const = 0;

    virtual CSSStyleSheet* styleSheet() const = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    CSSRuleList() { }
};

class StaticCSSRuleList final : public CSSRuleList {
public:
    static StaticCSSRuleList* create()
    {
        return new StaticCSSRuleList();
    }

    HeapVector<Member<CSSRule>>& rules() { return m_rules; }

    CSSStyleSheet* styleSheet() const override { return 0; }

    DECLARE_VIRTUAL_TRACE();

private:
    StaticCSSRuleList();

    unsigned length() const override { return m_rules.size(); }
    CSSRule* item(unsigned index) const override { return index < m_rules.size() ? m_rules[index].get() : nullptr; }

    HeapVector<Member<CSSRule>> m_rules;
};

template <class Rule>
class LiveCSSRuleList final : public CSSRuleList {
public:
    static LiveCSSRuleList* create(Rule* rule)
    {
        return new LiveCSSRuleList(rule);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_rule);
        CSSRuleList::trace(visitor);
    }

private:
    LiveCSSRuleList(Rule* rule) : m_rule(rule) { }

    unsigned length() const override { return m_rule->length(); }
    CSSRule* item(unsigned index) const override { return m_rule->item(index); }
    CSSStyleSheet* styleSheet() const override { return m_rule->parentStyleSheet(); }

    Member<Rule> m_rule;
};

} // namespace blink

#endif // CSSRuleList_h
