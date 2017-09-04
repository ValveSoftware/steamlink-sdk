/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StyleGridData_h
#define StyleGridData_h

#include "core/style/GridArea.h"
#include "core/style/GridTrackSize.h"
#include "core/style/ComputedStyleConstants.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

typedef HashMap<String, Vector<size_t>> NamedGridLinesMap;
typedef HashMap<size_t,
                Vector<String>,
                WTF::IntHash<size_t>,
                WTF::UnsignedWithZeroKeyHashTraits<size_t>>
    OrderedNamedGridLines;

class StyleGridData : public RefCounted<StyleGridData> {
 public:
  static PassRefPtr<StyleGridData> create() {
    return adoptRef(new StyleGridData);
  }
  PassRefPtr<StyleGridData> copy() const {
    return adoptRef(new StyleGridData(*this));
  }

  bool operator==(const StyleGridData& o) const {
    return m_gridTemplateColumns == o.m_gridTemplateColumns &&
           m_gridTemplateRows == o.m_gridTemplateRows &&
           m_gridAutoFlow == o.m_gridAutoFlow &&
           m_gridAutoRows == o.m_gridAutoRows &&
           m_gridAutoColumns == o.m_gridAutoColumns &&
           m_namedGridColumnLines == o.m_namedGridColumnLines &&
           m_namedGridRowLines == o.m_namedGridRowLines &&
           m_orderedNamedGridColumnLines == o.m_orderedNamedGridColumnLines &&
           m_orderedNamedGridRowLines == o.m_orderedNamedGridRowLines &&
           m_autoRepeatNamedGridColumnLines ==
               o.m_autoRepeatNamedGridColumnLines &&
           m_autoRepeatNamedGridRowLines == o.m_autoRepeatNamedGridRowLines &&
           m_autoRepeatOrderedNamedGridColumnLines ==
               o.m_autoRepeatOrderedNamedGridColumnLines &&
           m_autoRepeatOrderedNamedGridRowLines ==
               o.m_autoRepeatOrderedNamedGridRowLines &&
           m_namedGridArea == o.m_namedGridArea &&
           m_namedGridArea == o.m_namedGridArea &&
           m_namedGridAreaRowCount == o.m_namedGridAreaRowCount &&
           m_namedGridAreaColumnCount == o.m_namedGridAreaColumnCount &&
           m_gridColumnGap == o.m_gridColumnGap &&
           m_gridRowGap == o.m_gridRowGap &&
           m_gridAutoRepeatColumns == o.m_gridAutoRepeatColumns &&
           m_gridAutoRepeatRows == o.m_gridAutoRepeatRows &&
           m_autoRepeatColumnsInsertionPoint ==
               o.m_autoRepeatColumnsInsertionPoint &&
           m_autoRepeatRowsInsertionPoint == o.m_autoRepeatRowsInsertionPoint &&
           m_autoRepeatColumnsType == o.m_autoRepeatColumnsType &&
           m_autoRepeatRowsType == o.m_autoRepeatRowsType;
  }

  bool operator!=(const StyleGridData& o) const { return !(*this == o); }

  Vector<GridTrackSize> m_gridTemplateColumns;
  Vector<GridTrackSize> m_gridTemplateRows;

  NamedGridLinesMap m_namedGridColumnLines;
  NamedGridLinesMap m_namedGridRowLines;

  // In order to reconstruct the original named grid line order, we can't rely
  // on NamedGridLinesMap as it loses the position if multiple grid lines are
  // set on a single track.
  OrderedNamedGridLines m_orderedNamedGridColumnLines;
  OrderedNamedGridLines m_orderedNamedGridRowLines;

  NamedGridLinesMap m_autoRepeatNamedGridColumnLines;
  NamedGridLinesMap m_autoRepeatNamedGridRowLines;
  OrderedNamedGridLines m_autoRepeatOrderedNamedGridColumnLines;
  OrderedNamedGridLines m_autoRepeatOrderedNamedGridRowLines;

  unsigned m_gridAutoFlow : GridAutoFlowBits;

  Vector<GridTrackSize> m_gridAutoRows;
  Vector<GridTrackSize> m_gridAutoColumns;

  NamedGridAreaMap m_namedGridArea;
  // Because m_namedGridArea doesn't store the unnamed grid areas, we need to
  // keep track of the explicit grid size defined by both named and unnamed grid
  // areas.
  size_t m_namedGridAreaRowCount;
  size_t m_namedGridAreaColumnCount;

  Length m_gridColumnGap;
  Length m_gridRowGap;

  Vector<GridTrackSize> m_gridAutoRepeatColumns;
  Vector<GridTrackSize> m_gridAutoRepeatRows;

  size_t m_autoRepeatColumnsInsertionPoint;
  size_t m_autoRepeatRowsInsertionPoint;

  AutoRepeatType m_autoRepeatColumnsType;
  AutoRepeatType m_autoRepeatRowsType;

 private:
  StyleGridData();
  StyleGridData(const StyleGridData&);
};

}  // namespace blink

#endif  // StyleGridData_h
