// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSGridAutoRepeatValue_h
#define CSSGridAutoRepeatValue_h

#include "core/CSSValueKeywords.h"
#include "core/css/CSSValueList.h"

namespace blink {

// CSSGridAutoRepeatValue stores the track sizes and line numbers when the
// auto-repeat syntax is used
//
// Right now the auto-repeat syntax is as follows:
// <auto-repeat>  = repeat( [ auto-fill | auto-fit ], <line-names>? <fixed-size>
// <line-names>? )
//
// meaning that only one fixed size track is allowed. It could be argued that a
// different class storing two CSSGridLineNamesValue and one CSSValue (for the
// track size) fits better but the CSSWG has left the door open to allow more
// than one track in the future. That's why we're using a list, it's prepared
// for future changes and it also allows us to keep the parsing algorithm almost
// intact.
class CSSGridAutoRepeatValue : public CSSValueList {
 public:
  static CSSGridAutoRepeatValue* create(CSSValueID id) {
    return new CSSGridAutoRepeatValue(id);
  }

  String customCSSText() const;
  CSSValueID autoRepeatID() const { return m_autoRepeatID; }

  DEFINE_INLINE_TRACE_AFTER_DISPATCH() {
    CSSValueList::traceAfterDispatch(visitor);
  }

 private:
  CSSGridAutoRepeatValue(CSSValueID id)
      : CSSValueList(GridAutoRepeatClass, SpaceSeparator), m_autoRepeatID(id) {
    ASSERT(id == CSSValueAutoFill || id == CSSValueAutoFit);
  }

  const CSSValueID m_autoRepeatID;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSGridAutoRepeatValue, isGridAutoRepeatValue());

}  // namespace blink

#endif  // CSSGridAutoRepeatValue_h
