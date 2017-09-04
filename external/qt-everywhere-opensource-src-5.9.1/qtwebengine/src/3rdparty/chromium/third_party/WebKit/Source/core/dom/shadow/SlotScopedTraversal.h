// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SlotScopedTraversal_h
#define SlotScopedTraversal_h

#include "core/CoreExport.h"

namespace blink {

class Element;
class HTMLSlotElement;

class CORE_EXPORT SlotScopedTraversal {
 public:
  static HTMLSlotElement* findScopeOwnerSlot(const Element&);
  static Element* nearestInclusiveAncestorAssignedToSlot(const Element&);
  static Element* next(const Element&);
  static Element* previous(const Element&);
  static Element* firstAssignedToSlot(HTMLSlotElement&);
  static Element* lastAssignedToSlot(HTMLSlotElement&);

  static bool isSlotScoped(const Element&);
};

}  // namespace blink

#endif
