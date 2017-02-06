/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#ifndef CSSPageRule_h
#define CSSPageRule_h

#include "core/css/CSSRule.h"
#include "platform/heap/Handle.h"

namespace blink {

class CSSStyleDeclaration;
class CSSStyleSheet;
class StyleRulePage;
class StyleRuleCSSStyleDeclaration;

class CORE_EXPORT CSSPageRule final : public CSSRule {
    DEFINE_WRAPPERTYPEINFO();
public:
    static CSSPageRule* create(StyleRulePage* rule, CSSStyleSheet* sheet)
    {
        return new CSSPageRule(rule, sheet);
    }

    ~CSSPageRule() override;

    String cssText() const override;
    void reattach(StyleRuleBase*) override;

    CSSStyleDeclaration* style() const;

    String selectorText() const;
    void setSelectorText(const String&);

    DECLARE_VIRTUAL_TRACE();

private:
    CSSPageRule(StyleRulePage*, CSSStyleSheet*);

    CSSRule::Type type() const override { return PAGE_RULE; }

    Member<StyleRulePage> m_pageRule;
    mutable Member<StyleRuleCSSStyleDeclaration> m_propertiesCSSOMWrapper;
};

DEFINE_CSS_RULE_TYPE_CASTS(CSSPageRule, PAGE_RULE);

} // namespace blink

#endif // CSSPageRule_h
