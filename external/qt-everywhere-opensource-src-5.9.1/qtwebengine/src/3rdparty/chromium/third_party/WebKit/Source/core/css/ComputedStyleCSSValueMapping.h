// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ComputedStyleCSSValueMapping_h
#define ComputedStyleCSSValueMapping_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSValue.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class CSSVariableData;
class ComputedStyle;
class FilterOperations;
class LayoutObject;
class Node;
class PropertyRegistry;
class ShadowData;
class ShadowList;
class StyleColor;

class ComputedStyleCSSValueMapping {
  STATIC_ONLY(ComputedStyleCSSValueMapping);

 public:
  // FIXME: Resolve computed auto alignment in applyProperty/ComputedStyle and
  // remove this non-const styledNode parameter.
  static const CSSValue* get(CSSPropertyID,
                             const ComputedStyle&,
                             const LayoutObject* = nullptr,
                             Node* styledNode = nullptr,
                             bool allowVisitedStyle = false);
  static const CSSValue* get(const AtomicString customPropertyName,
                             const ComputedStyle&,
                             const PropertyRegistry*);
  static std::unique_ptr<HashMap<AtomicString, RefPtr<CSSVariableData>>>
  getVariables(const ComputedStyle&);

 private:
  static CSSValue* currentColorOrValidColor(const ComputedStyle&,
                                            const StyleColor&);
  static CSSValue* valueForShadowData(const ShadowData&,
                                      const ComputedStyle&,
                                      bool useSpread);
  static CSSValue* valueForShadowList(const ShadowList*,
                                      const ComputedStyle&,
                                      bool useSpread);
  static CSSValue* valueForFilter(const ComputedStyle&,
                                  const FilterOperations&);
  static CSSValue* valueForFont(const ComputedStyle&);
};

}  // namespace blink

#endif  // ComputedStyleCSSValueMapping_h
