// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OptionList_h
#define OptionList_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLSelectElement;
class HTMLOptionElement;

class CORE_EXPORT OptionListIterator final {
  STACK_ALLOCATED();

 public:
  explicit OptionListIterator(const HTMLSelectElement* select)
      : m_select(select) {
    if (m_select)
      advance(nullptr);
  }
  HTMLOptionElement* operator*() { return m_current; }
  void operator++() {
    if (m_current)
      advance(m_current);
  }
  bool operator==(const OptionListIterator& other) const {
    return m_current == other.m_current;
  }
  bool operator!=(const OptionListIterator& other) const {
    return !(*this == other);
  }

 private:
  void advance(HTMLOptionElement* current);

  Member<const HTMLSelectElement> m_select;
  Member<HTMLOptionElement> m_current;  // nullptr means we reached to the end.
};

// OptionList class is a lightweight version of HTMLOptionsCollection.
class OptionList final {
  STACK_ALLOCATED();

 public:
  explicit OptionList(const HTMLSelectElement& select) : m_select(select) {}
  using Iterator = OptionListIterator;
  Iterator begin() { return Iterator(m_select); }
  Iterator end() { return Iterator(nullptr); }

 private:
  Member<const HTMLSelectElement> m_select;
};

}  // namespace blink

#endif
