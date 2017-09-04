// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLMenuItemElement.h"

#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/events/Event.h"
#include "core/frame/UseCounter.h"

namespace blink {

using namespace HTMLNames;

inline HTMLMenuItemElement::HTMLMenuItemElement(Document& document)
    : HTMLElement(HTMLNames::menuitemTag, document) {
  UseCounter::count(document, UseCounter::MenuItemElement);
}

bool HTMLMenuItemElement::isURLAttribute(const Attribute& attribute) const {
  return attribute.name() == iconAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLMenuItemElement::defaultEventHandler(Event* event) {
  if (event->type() == EventTypeNames::click) {
    if (equalIgnoringCase(fastGetAttribute(typeAttr), "checkbox")) {
      if (fastHasAttribute(checkedAttr))
        removeAttribute(checkedAttr);
      else
        setAttribute(checkedAttr, "checked");
    } else if (equalIgnoringCase(fastGetAttribute(typeAttr), "radio")) {
      if (Element* parent = parentElement()) {
        AtomicString group = fastGetAttribute(radiogroupAttr);
        for (HTMLMenuItemElement& menuItem :
             Traversal<HTMLMenuItemElement>::childrenOf(*parent)) {
          if (!menuItem.fastHasAttribute(checkedAttr))
            continue;
          const AtomicString& groupAttr =
              menuItem.fastGetAttribute(radiogroupAttr);
          if (equalIgnoringNullity(groupAttr.impl(), group.impl()))
            menuItem.removeAttribute(checkedAttr);
        }
      }
      setAttribute(checkedAttr, "checked");
    }
    event->setDefaultHandled();
  }
}

DEFINE_NODE_FACTORY(HTMLMenuItemElement)

}  // namespace blink
