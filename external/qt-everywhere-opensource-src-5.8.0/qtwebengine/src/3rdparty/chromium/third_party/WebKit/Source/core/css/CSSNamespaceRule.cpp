// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSNamespaceRule.h"

#include "core/css/CSSMarkup.h"
#include "core/css/StyleRuleNamespace.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

CSSNamespaceRule::CSSNamespaceRule(StyleRuleNamespace* namespaceRule, CSSStyleSheet* parent)
    : CSSRule(parent)
    , m_namespaceRule(namespaceRule)
{
}

CSSNamespaceRule::~CSSNamespaceRule()
{
}

String CSSNamespaceRule::cssText() const
{
    StringBuilder result;
    result.append("@namespace ");
    serializeIdentifier(prefix(), result);
    if (!prefix().isEmpty())
        result.append(" ");
    result.append("url(");
    result.append(serializeString(namespaceURI()));
    result.append(");");
    return result.toString();
}

AtomicString CSSNamespaceRule::namespaceURI() const
{
    return m_namespaceRule->uri();
}

AtomicString CSSNamespaceRule::prefix() const
{
    return m_namespaceRule->prefix();
}

DEFINE_TRACE(CSSNamespaceRule)
{
    visitor->trace(m_namespaceRule);
    CSSRule::trace(visitor);
}

} // namespace blink
