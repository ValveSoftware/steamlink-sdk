// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElement_h
#define CustomElement_h

#include "core/CoreExport.h"
#include "core/HTMLNames.h"
#include "core/dom/Element.h"
#include "platform/text/Character.h"
#include "wtf/ASCIICType.h"
#include "wtf/Allocator.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Document;
class Element;
class HTMLElement;
class QualifiedName;
class CustomElementDefinition;
class CustomElementReaction;
class CustomElementRegistry;

class CORE_EXPORT CustomElement {
  STATIC_ONLY(CustomElement);

 public:
  // Retrieves the CustomElementRegistry for Element, if any. This
  // may be a different object for a given element over its lifetime
  // as it moves between documents.
  static CustomElementRegistry* registry(const Element&);
  static CustomElementRegistry* registry(const Document&);

  static CustomElementDefinition* definitionForElement(const Element*);

  static bool isValidName(const AtomicString& name) {
    // This quickly rejects all common built-in element names.
    if (name.find('-', 1) == kNotFound)
      return false;

    if (!isASCIILower(name[0]))
      return false;

    if (name.is8Bit()) {
      const LChar* characters = name.characters8();
      for (size_t i = 1; i < name.length(); ++i) {
        if (!Character::isPotentialCustomElementName8BitChar(characters[i]))
          return false;
      }
    } else {
      const UChar* characters = name.characters16();
      for (size_t i = 1; i < name.length();) {
        UChar32 ch;
        U16_NEXT(characters, i, name.length(), ch);
        if (!Character::isPotentialCustomElementNameChar(ch))
          return false;
      }
    }

    return !isHyphenatedSpecElementName(name);
  }

  static bool shouldCreateCustomElement(const AtomicString& localName);
  static bool shouldCreateCustomElement(const QualifiedName&);

  static HTMLElement* createCustomElementSync(Document&,
                                              const AtomicString& localName);
  static HTMLElement* createCustomElementSync(Document&, const QualifiedName&);
  static HTMLElement* createCustomElementAsync(Document&, const QualifiedName&);

  static HTMLElement* createFailedElement(Document&, const QualifiedName&);

  static void enqueue(Element*, CustomElementReaction*);
  static void enqueueConnectedCallback(Element*);
  static void enqueueDisconnectedCallback(Element*);
  static void enqueueAdoptedCallback(Element*,
                                     Document* oldOwner,
                                     Document* newOwner);
  static void enqueueAttributeChangedCallback(Element*,
                                              const QualifiedName&,
                                              const AtomicString& oldValue,
                                              const AtomicString& newValue);

  static void tryToUpgrade(Element*);

 private:
  // Some existing specs have element names with hyphens in them,
  // like font-face in SVG. The custom elements spec explicitly
  // disallows these as custom element names.
  // https://html.spec.whatwg.org/#valid-custom-element-name
  static bool isHyphenatedSpecElementName(const AtomicString&);
  static HTMLElement* createUndefinedElement(Document&, const QualifiedName&);
};

}  // namespace blink

#endif  // CustomElement_h
