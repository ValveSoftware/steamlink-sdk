/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VisibleSelection_h
#define VisibleSelection_h

#include "core/CoreExport.h"
#include "core/editing/EditingStrategy.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/SelectionType.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/TextGranularity.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/VisibleUnits.h"
#include "wtf/Allocator.h"

namespace blink {

class LayoutPoint;
class SelectionAdjuster;

const TextAffinity SelDefaultAffinity = TextAffinity::Downstream;
enum SelectionDirection { DirectionForward, DirectionBackward, DirectionRight, DirectionLeft };

template <typename Strategy>
class CORE_TEMPLATE_CLASS_EXPORT VisibleSelectionTemplate {
    DISALLOW_NEW();
public:
    VisibleSelectionTemplate();
    VisibleSelectionTemplate(const PositionTemplate<Strategy>&, TextAffinity, bool isDirectional = false);
    VisibleSelectionTemplate(const PositionTemplate<Strategy>& base, const PositionTemplate<Strategy>& extent, TextAffinity = SelDefaultAffinity, bool isDirectional = false);
    explicit VisibleSelectionTemplate(const EphemeralRangeTemplate<Strategy>&, TextAffinity = SelDefaultAffinity, bool isDirectional = false);

    explicit VisibleSelectionTemplate(const VisiblePositionTemplate<Strategy>&, bool isDirectional = false);
    VisibleSelectionTemplate(const VisiblePositionTemplate<Strategy>&, const VisiblePositionTemplate<Strategy>&, bool isDirectional = false);

    explicit VisibleSelectionTemplate(const PositionWithAffinityTemplate<Strategy>&, bool isDirectional = false);

    VisibleSelectionTemplate(const VisibleSelectionTemplate&);
    VisibleSelectionTemplate& operator=(const VisibleSelectionTemplate&);

    static VisibleSelectionTemplate selectionFromContentsOfNode(Node*);

    SelectionType getSelectionType() const { return m_selectionType; }

    void setAffinity(TextAffinity affinity) { m_affinity = affinity; }
    TextAffinity affinity() const { return m_affinity; }

    void setBase(const PositionTemplate<Strategy>&);
    void setBase(const VisiblePositionTemplate<Strategy>&);
    void setExtent(const PositionTemplate<Strategy>&);
    void setExtent(const VisiblePositionTemplate<Strategy>&);

    PositionTemplate<Strategy> base() const { return m_base; }
    PositionTemplate<Strategy> extent() const { return m_extent; }
    PositionTemplate<Strategy> start() const { return m_start; }
    PositionTemplate<Strategy> end() const { return m_end; }

    VisiblePositionTemplate<Strategy> visibleStart() const { return createVisiblePosition(m_start, isRange() ? TextAffinity::Downstream : affinity()); }
    VisiblePositionTemplate<Strategy> visibleEnd() const { return createVisiblePosition(m_end, isRange() ? TextAffinity::Upstream : affinity()); }
    VisiblePositionTemplate<Strategy> visibleBase() const { return createVisiblePosition(m_base, isRange() ? (isBaseFirst() ? TextAffinity::Upstream : TextAffinity::Downstream) : affinity()); }
    VisiblePositionTemplate<Strategy> visibleExtent() const { return createVisiblePosition(m_extent, isRange() ? (isBaseFirst() ? TextAffinity::Downstream : TextAffinity::Upstream) : affinity()); }

    bool operator==(const VisibleSelectionTemplate&) const;
    bool operator!=(const VisibleSelectionTemplate& other) const { return !operator==(other); }

    bool isNone() const { return getSelectionType() == NoSelection; }
    bool isCaret() const { return getSelectionType() == CaretSelection; }
    bool isRange() const { return getSelectionType() == RangeSelection; }
    bool isNonOrphanedRange() const { return isRange() && !start().isOrphan() && !end().isOrphan(); }
    bool isNonOrphanedCaretOrRange() const { return !isNone() && !start().isOrphan() && !end().isOrphan(); }

    bool isBaseFirst() const { return m_baseIsFirst; }
    bool isDirectional() const { return m_isDirectional; }
    void setIsDirectional(bool isDirectional) { m_isDirectional = isDirectional; }

    void appendTrailingWhitespace();

    void expandUsingGranularity(TextGranularity);

    // TODO(yosin) Most callers probably don't want these functions, but
    // are using them for historical reasons. |toNormalizedEphemeralRange()|
    // contracts the range around text, and moves the caret most backward
    // visually equivalent position before returning the range/positions.
    EphemeralRangeTemplate<Strategy> toNormalizedEphemeralRange() const;

    Element* rootEditableElement() const;
    bool isContentEditable() const;
    bool hasEditableStyle() const;
    bool isContentRichlyEditable() const;

    bool isValidFor(const Document&) const;
    void setWithoutValidation(const PositionTemplate<Strategy>&, const PositionTemplate<Strategy>&);

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_base);
        visitor->trace(m_extent);
        visitor->trace(m_start);
        visitor->trace(m_end);
    }

    void updateIfNeeded();
    void validatePositionsIfNeeded();

#ifndef NDEBUG
    void debugPosition(const char* message) const;
    void formatForDebugger(char* buffer, unsigned length) const;
    void showTreeForThis() const;
#endif
    static void PrintTo(const VisibleSelectionTemplate&, std::ostream*);

private:
    friend class SelectionAdjuster;

    void validate(TextGranularity = CharacterGranularity);

    // Support methods for validate()
    void setBaseAndExtentToDeepEquivalents();
    void adjustSelectionToAvoidCrossingShadowBoundaries();
    void adjustSelectionToAvoidCrossingEditingBoundaries();
    void setEndRespectingGranularity(TextGranularity);
    void setStartRespectingGranularity(TextGranularity);
    void updateSelectionType();

    // We need to store these as Positions because VisibleSelection is
    // used to store values in editing commands for use when
    // undoing the command. We need to be able to create a selection that, while
    // currently invalid, will be valid once the changes are undone.

    // Where the first click happened
    PositionTemplate<Strategy> m_base;
    // Where the end click happened
    PositionTemplate<Strategy> m_extent;
    // Leftmost position when expanded to respect granularity
    PositionTemplate<Strategy> m_start;
    // Rightmost position when expanded to respect granularity
    PositionTemplate<Strategy> m_end;

    TextAffinity m_affinity; // the upstream/downstream affinity of the caret

    // these are cached, can be recalculated by validate()
    SelectionType m_selectionType; // None, Caret, Range
    bool m_baseIsFirst : 1; // True if base is before the extent
    bool m_isDirectional : 1; // Non-directional ignores m_baseIsFirst and selection always extends on shift + arrow key.

    TextGranularity m_granularity;
    // |updateIfNeeded()| uses |m_hasTrailingWhitespace| for word granularity.
    // |m_hasTrailingWhitespace| is set by |appendTrailingWhitespace()|.
    // TODO(yosin): Once we unify start/end and base/extent, we should get rid
    // of |m_hasTrailingWhitespace|.
    bool m_hasTrailingWhitespace : 1;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT VisibleSelectionTemplate<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT VisibleSelectionTemplate<EditingInFlatTreeStrategy>;

using VisibleSelection = VisibleSelectionTemplate<EditingStrategy>;
using VisibleSelectionInFlatTree = VisibleSelectionTemplate<EditingInFlatTreeStrategy>;

// We don't yet support multi-range selections, so we only ever have one range
// to return.
CORE_EXPORT EphemeralRange firstEphemeralRangeOf(const VisibleSelection&);

// TODO(sof): move more firstRangeOf() uses to be over EphemeralRange instead.
CORE_EXPORT Range* firstRangeOf(const VisibleSelection&);

CORE_EXPORT std::ostream& operator<<(std::ostream&, const VisibleSelection&);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const VisibleSelectionInFlatTree&);

} // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const blink::VisibleSelection&);
void showTree(const blink::VisibleSelection*);
void showTree(const blink::VisibleSelectionInFlatTree&);
void showTree(const blink::VisibleSelectionInFlatTree*);
#endif

#endif // VisibleSelection_h
