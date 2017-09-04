/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2012 Apple Computer, Inc.
 * Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
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

#include "core/css/CSSMediaRule.h"

#include "core/css/StyleRule.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSMediaRule::CSSMediaRule(StyleRuleMedia* mediaRule, CSSStyleSheet* parent)
    : CSSConditionRule(mediaRule, parent) {}

CSSMediaRule::~CSSMediaRule() {}

MediaQuerySet* CSSMediaRule::mediaQueries() const {
  return toStyleRuleMedia(m_groupRule.get())->mediaQueries();
}

String CSSMediaRule::cssText() const {
  StringBuilder result;
  result.append("@media ");
  if (mediaQueries()) {
    result.append(mediaQueries()->mediaText());
    result.append(' ');
  }
  result.append("{\n");
  appendCSSTextForItems(result);
  result.append('}');
  return result.toString();
}

String CSSMediaRule::conditionText() const {
  if (!mediaQueries())
    return String();
  return mediaQueries()->mediaText();
}

MediaList* CSSMediaRule::media() const {
  if (!mediaQueries())
    return nullptr;
  if (!m_mediaCSSOMWrapper)
    m_mediaCSSOMWrapper =
        MediaList::create(mediaQueries(), const_cast<CSSMediaRule*>(this));
  return m_mediaCSSOMWrapper.get();
}

void CSSMediaRule::reattach(StyleRuleBase* rule) {
  CSSConditionRule::reattach(rule);
  if (m_mediaCSSOMWrapper && mediaQueries())
    m_mediaCSSOMWrapper->reattach(mediaQueries());
}

DEFINE_TRACE(CSSMediaRule) {
  visitor->trace(m_mediaCSSOMWrapper);
  CSSConditionRule::trace(visitor);
}
}  // namespace blink
