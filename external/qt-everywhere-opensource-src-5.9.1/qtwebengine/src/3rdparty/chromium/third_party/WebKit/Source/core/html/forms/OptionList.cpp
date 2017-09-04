// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/forms/OptionList.h"

#include "core/dom/ElementTraversal.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"

namespace blink {

void OptionListIterator::advance(HTMLOptionElement* previous) {
  // This function returns only
  // - An OPTION child of m_select, or
  // - An OPTION child of an OPTGROUP child of m_select.

  Element* current;
  if (previous) {
    DCHECK_EQ(previous->ownerSelectElement(), m_select);
    current = ElementTraversal::nextSkippingChildren(*previous, m_select);
  } else {
    current = ElementTraversal::firstChild(*m_select);
  }
  while (current) {
    if (isHTMLOptionElement(current)) {
      m_current = toHTMLOptionElement(current);
      return;
    }
    if (isHTMLOptGroupElement(current) &&
        current->parentNode() == m_select.get()) {
      if ((m_current = Traversal<HTMLOptionElement>::firstChild(*current)))
        return;
    }
    current = ElementTraversal::nextSkippingChildren(*current, m_select);
  }
  m_current = nullptr;
}

}  // namespace blink
