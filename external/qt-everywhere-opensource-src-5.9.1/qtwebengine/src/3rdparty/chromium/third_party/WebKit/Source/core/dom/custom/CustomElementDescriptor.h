// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDescriptor_h
#define CustomElementDescriptor_h

#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "wtf/Allocator.h"
#include "wtf/HashTableDeletedValueType.h"
#include "wtf/text/AtomicString.h"

namespace blink {

// Describes what elements a custom element definition applies to.
// https://html.spec.whatwg.org/multipage/scripting.html#custom-elements-core-concepts
//
// There are two kinds of definitions:
//
// [Autonomous] These have their own tag name. In that case "name"
// (the definition name) and local name (the tag name) are identical.
//
// [Customized built-in] The name is still a valid custom element
// name; but the local name will be a built-in element name, or an
// unknown element name that is *not* a valid custom element name.
//
// CustomElementDescriptor used when the kind of custom element
// definition is known, and generally the difference is important. For
// example, a definition for "my-element", "my-element" must not be
// applied to an element <button is="my-element">.
class CORE_EXPORT CustomElementDescriptor final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  CustomElementDescriptor() {}

  CustomElementDescriptor(const AtomicString& name,
                          const AtomicString& localName)
      : m_name(name), m_localName(localName) {}

  explicit CustomElementDescriptor(WTF::HashTableDeletedValueType value)
      : m_name(value) {}

  bool isHashTableDeletedValue() const {
    return m_name.isHashTableDeletedValue();
  }

  bool operator==(const CustomElementDescriptor& other) const {
    return m_name == other.m_name && m_localName == other.m_localName;
  }

  const AtomicString& name() const { return m_name; }
  const AtomicString& localName() const { return m_localName; }

  bool matches(const Element& element) const {
    return localName() == element.localName() &&
           (isAutonomous() ||
            name() == element.getAttribute(HTMLNames::isAttr)) &&
           element.namespaceURI() == HTMLNames::xhtmlNamespaceURI;
  }

  bool isAutonomous() const { return m_name == m_localName; }

 private:
  AtomicString m_name;
  AtomicString m_localName;
};

}  // namespace blink

#endif  // CustomElementDescriptor_h
