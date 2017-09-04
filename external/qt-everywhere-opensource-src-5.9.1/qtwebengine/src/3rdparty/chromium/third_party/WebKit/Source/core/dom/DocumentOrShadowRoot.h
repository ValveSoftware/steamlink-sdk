// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentOrShadowRoot_h
#define DocumentOrShadowRoot_h

#include "core/dom/Document.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/UseCounter.h"

namespace blink {

class DocumentOrShadowRoot {
 public:
  static Element* activeElement(Document& document) {
    return document.activeElement();
  }

  static Element* activeElement(ShadowRoot& shadowRoot) {
    return shadowRoot.activeElement();
  }

  static StyleSheetList* styleSheets(Document& document) {
    return &document.styleSheets();
  }

  static StyleSheetList* styleSheets(ShadowRoot& shadowRoot) {
    return &shadowRoot.styleSheets();
  }

  static DOMSelection* getSelection(TreeScope& treeScope) {
    return treeScope.getSelection();
  }

  static Element* elementFromPoint(TreeScope& treeScope, int x, int y) {
    return treeScope.elementFromPoint(x, y);
  }

  static HeapVector<Member<Element>> elementsFromPoint(TreeScope& treeScope,
                                                       int x,
                                                       int y) {
    return treeScope.elementsFromPoint(x, y);
  }

  static Element* pointerLockElement(Document& document) {
    UseCounter::count(document, UseCounter::DocumentPointerLockElement);
    const Element* target = document.pointerLockElement();
    if (!target)
      return nullptr;
    // For Shadow DOM V0 compatibility: We allow returning an element in V0
    // shadow tree, even though it leaks the Shadow DOM.
    // TODO(kochi): Once V0 code is removed, the following V0 check is
    // unnecessary.
    if (target && target->isInV0ShadowTree()) {
      UseCounter::count(document,
                        UseCounter::DocumentPointerLockElementInV0Shadow);
      return const_cast<Element*>(target);
    }
    return document.adjustedElement(*target);
  }

  static Element* pointerLockElement(ShadowRoot& shadowRoot) {
    // TODO(kochi): Once V0 code is removed, the following non-V1 check is
    // unnecessary.  After V0 code is removed, we can use the same logic for
    // Document and ShadowRoot.
    if (!shadowRoot.isV1())
      return nullptr;
    UseCounter::count(shadowRoot.document(),
                      UseCounter::ShadowRootPointerLockElement);
    const Element* target = shadowRoot.document().pointerLockElement();
    if (!target)
      return nullptr;
    return shadowRoot.adjustedElement(*target);
  }

  static Element* fullscreenElement(TreeScope& scope) {
    return Fullscreen::fullscreenElementForBindingFrom(scope);
  }
};

}  // namespace blink

#endif  // DocumentOrShadowRoot_h
