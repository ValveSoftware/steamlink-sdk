/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef PseudoStyleRequest_h
#define PseudoStyleRequest_h

#include "core/layout/LayoutScrollbar.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class ComputedStyle;

class PseudoStyleRequest {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  enum RequestType { ForRenderer, ForComputedStyle };

  PseudoStyleRequest(PseudoId pseudoId,
                     LayoutScrollbar* scrollbar = 0,
                     ScrollbarPart scrollbarPart = NoPart)
      : pseudoId(pseudoId),
        type(ForRenderer),
        scrollbarPart(scrollbarPart),
        scrollbar(scrollbar) {}

  PseudoStyleRequest(PseudoId pseudoId, RequestType requestType)
      : pseudoId(pseudoId),
        type(requestType),
        scrollbarPart(NoPart),
        scrollbar(nullptr) {}

  DEFINE_INLINE_TRACE() { visitor->trace(scrollbar); }

  // The spec disallows inheritance for ::backdrop.
  bool allowsInheritance(const ComputedStyle* parentStyle) const {
    return parentStyle && pseudoId != PseudoIdBackdrop;
  }

  PseudoId pseudoId;
  RequestType type;
  ScrollbarPart scrollbarPart;
  Member<LayoutScrollbar> scrollbar;
};

}  // namespace blink

#endif  // PseudoStyleRequest_h
