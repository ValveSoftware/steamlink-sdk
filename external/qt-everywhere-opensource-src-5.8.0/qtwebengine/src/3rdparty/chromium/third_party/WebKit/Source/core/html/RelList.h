// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RelList_h
#define RelList_h

#include "core/HTMLNames.h"
#include "core/dom/DOMTokenList.h"
#include "core/dom/Element.h"
#include "core/dom/SpaceSplitString.h"

namespace blink {

class RelList final : public DOMTokenList {
public:
    static RelList* create(Element* element)
    {
        return new RelList(element);
    }

    unsigned length() const override;
    const AtomicString item(unsigned index) const override;

    Element* element() override { return m_element; }
    void setRelValues(const AtomicString&);

    DECLARE_VIRTUAL_TRACE();

    using SupportedTokens = HashSet<AtomicString>;

private:
    explicit RelList(Element*);

    bool containsInternal(const AtomicString&) const override;

    const AtomicString& value() const override { return m_element->getAttribute(HTMLNames::relAttr); }
    void setValue(const AtomicString& value) override { m_element->setAttribute(HTMLNames::relAttr, value); }

    bool validateTokenValue(const AtomicString&, ExceptionState&) const override;

    Member<Element> m_element;
    SpaceSplitString m_relValues;
};

} // namespace blink

#endif // RelList_h
