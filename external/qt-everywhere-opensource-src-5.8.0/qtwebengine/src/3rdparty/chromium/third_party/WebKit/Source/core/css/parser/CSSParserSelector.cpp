/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "core/css/parser/CSSParserSelector.h"

#include "core/css/CSSSelectorList.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

CSSParserSelector::CSSParserSelector()
    : m_selector(wrapUnique(new CSSSelector()))
{
}

CSSParserSelector::CSSParserSelector(const QualifiedName& tagQName, bool isImplicit)
    : m_selector(wrapUnique(new CSSSelector(tagQName, isImplicit)))
{
}

CSSParserSelector::~CSSParserSelector()
{
    if (!m_tagHistory)
        return;
    Vector<std::unique_ptr<CSSParserSelector>, 16> toDelete;
    std::unique_ptr<CSSParserSelector> selector = std::move(m_tagHistory);
    while (true) {
        std::unique_ptr<CSSParserSelector> next = std::move(selector->m_tagHistory);
        toDelete.append(std::move(selector));
        if (!next)
            break;
        selector = std::move(next);
    }
}

void CSSParserSelector::adoptSelectorVector(Vector<std::unique_ptr<CSSParserSelector>>& selectorVector)
{
    CSSSelectorList* selectorList = new CSSSelectorList(CSSSelectorList::adoptSelectorVector(selectorVector));
    m_selector->setSelectorList(wrapUnique(selectorList));
}

void CSSParserSelector::setSelectorList(std::unique_ptr<CSSSelectorList> selectorList)
{
    m_selector->setSelectorList(std::move(selectorList));
}

bool CSSParserSelector::isSimple() const
{
    if (m_selector->selectorList() || m_selector->match() == CSSSelector::PseudoElement)
        return false;

    if (!m_tagHistory)
        return true;

    if (m_selector->match() == CSSSelector::Tag) {
        // We can't check against anyQName() here because namespace may not be nullAtom.
        // Example:
        //     @namespace "http://www.w3.org/2000/svg";
        //     svg:not(:root) { ...
        if (m_selector->tagQName().localName() == starAtom)
            return m_tagHistory->isSimple();
    }

    return false;
}

void CSSParserSelector::appendTagHistory(CSSSelector::RelationType relation, std::unique_ptr<CSSParserSelector> selector)
{
    CSSParserSelector* end = this;
    while (end->tagHistory())
        end = end->tagHistory();
    end->setRelation(relation);
    end->setTagHistory(std::move(selector));
}

std::unique_ptr<CSSParserSelector> CSSParserSelector::releaseTagHistory()
{
    setRelation(CSSSelector::SubSelector);
    return std::move(m_tagHistory);
}

void CSSParserSelector::prependTagSelector(const QualifiedName& tagQName, bool isImplicit)
{
    std::unique_ptr<CSSParserSelector> second = CSSParserSelector::create();
    second->m_selector = std::move(m_selector);
    second->m_tagHistory = std::move(m_tagHistory);
    m_tagHistory = std::move(second);
    m_selector = wrapUnique(new CSSSelector(tagQName, isImplicit));
}

bool CSSParserSelector::isHostPseudoSelector() const
{
    return pseudoType() == CSSSelector::PseudoHost || pseudoType() == CSSSelector::PseudoHostContext;
}

} // namespace blink
