/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
 */

#ifndef StyleSheetList_h
#define StyleSheetList_h

#include "bindings/core/v8/TraceWrapperMember.h"
#include "core/css/CSSStyleSheet.h"
#include "core/dom/TreeScope.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"

namespace blink {

class HTMLStyleElement;
class StyleSheet;

class CORE_EXPORT StyleSheetList final
    : public GarbageCollected<StyleSheetList>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static StyleSheetList* create(TreeScope* treeScope) {
    return new StyleSheetList(treeScope);
  }

  unsigned length();
  StyleSheet* item(unsigned index);

  HTMLStyleElement* getNamedItem(const AtomicString&) const;

  Document* document() const {
    return m_treeScope ? &m_treeScope->document() : nullptr;
  }

  CSSStyleSheet* anonymousNamedGetter(const AtomicString&);

  DECLARE_TRACE();

 private:
  explicit StyleSheetList(TreeScope*);
  const HeapVector<TraceWrapperMember<StyleSheet>>& styleSheets() const;

  Member<TreeScope> m_treeScope;
};

}  // namespace blink

#endif  // StyleSheetList_h
