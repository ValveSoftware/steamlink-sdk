// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/AtomicHTMLToken.h"

namespace blink {

QualifiedName AtomicHTMLToken::nameForAttribute(const HTMLToken::Attribute& attribute) const
{
    return QualifiedName(nullAtom, attribute.name(), nullAtom);
}

bool AtomicHTMLToken::usesName() const
{
    return m_type == HTMLToken::StartTag || m_type == HTMLToken::EndTag || m_type == HTMLToken::DOCTYPE;
}

bool AtomicHTMLToken::usesAttributes() const
{
    return m_type == HTMLToken::StartTag || m_type == HTMLToken::EndTag;
}

} // namespace blink
