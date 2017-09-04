/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef CSSShadowValue_h
#define CSSShadowValue_h

#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class CSSIdentifierValue;
class CSSPrimitiveValue;

// Used for text-shadow and box-shadow
class CORE_EXPORT CSSShadowValue : public CSSValue {
 public:
  static CSSShadowValue* create(CSSPrimitiveValue* x,
                                CSSPrimitiveValue* y,
                                CSSPrimitiveValue* blur,
                                CSSPrimitiveValue* spread,
                                CSSIdentifierValue* style,
                                CSSValue* color) {
    return new CSSShadowValue(x, y, blur, spread, style, color);
  }

  String customCSSText() const;

  bool equals(const CSSShadowValue&) const;

  Member<CSSPrimitiveValue> x;
  Member<CSSPrimitiveValue> y;
  Member<CSSPrimitiveValue> blur;
  Member<CSSPrimitiveValue> spread;
  Member<CSSIdentifierValue> style;
  Member<CSSValue> color;

  DECLARE_TRACE_AFTER_DISPATCH();

 private:
  CSSShadowValue(CSSPrimitiveValue* x,
                 CSSPrimitiveValue* y,
                 CSSPrimitiveValue* blur,
                 CSSPrimitiveValue* spread,
                 CSSIdentifierValue* style,
                 CSSValue* color);
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSShadowValue, isShadowValue());

}  // namespace blink

#endif
