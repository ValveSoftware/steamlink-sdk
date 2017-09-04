// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlineStylePropertyMap_h
#define InlineStylePropertyMap_h

#include "core/css/cssom/MutableStylePropertyMap.h"
#include "core/dom/Element.h"

namespace blink {

class CORE_EXPORT InlineStylePropertyMap final
    : public MutableStylePropertyMap {
  WTF_MAKE_NONCOPYABLE(InlineStylePropertyMap);

 public:
  explicit InlineStylePropertyMap(Element* ownerElement)
      : m_ownerElement(ownerElement) {}

  Vector<String> getProperties() override;

  void set(CSSPropertyID,
           CSSStyleValueOrCSSStyleValueSequenceOrString&,
           ExceptionState&) override;
  void append(CSSPropertyID,
              CSSStyleValueOrCSSStyleValueSequenceOrString&,
              ExceptionState&) override;
  void remove(CSSPropertyID, ExceptionState&) override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_ownerElement);
    MutableStylePropertyMap::trace(visitor);
  }

 protected:
  CSSStyleValueVector getAllInternal(CSSPropertyID) override;
  CSSStyleValueVector getAllInternal(AtomicString customPropertyName) override;

  HeapVector<StylePropertyMapEntry> getIterationEntries() override;

 private:
  Member<Element> m_ownerElement;
};

}  // namespace blink

#endif
