// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EphemeralRange.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"

namespace blink {

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(const PositionTemplate<Strategy>& start, const PositionTemplate<Strategy>& end)
    : m_startPosition(start)
    , m_endPosition(end)
#if DCHECK_IS_ON()
    , m_domTreeVersion(start.isNull() ? 0 : start.document()->domTreeVersion())
#endif
{
    if (m_startPosition.isNull()) {
        DCHECK(m_endPosition.isNull());
        return;
    }
    DCHECK(m_endPosition.isNotNull());
    DCHECK_EQ(m_startPosition.document(), m_endPosition.document());
    DCHECK(m_startPosition.inShadowIncludingDocument());
    DCHECK(m_endPosition.inShadowIncludingDocument());
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(const EphemeralRangeTemplate<Strategy>& other)
    : EphemeralRangeTemplate(other.m_startPosition, other.m_endPosition)
{
    DCHECK(other.isValid());
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(const PositionTemplate<Strategy>& position)
    : EphemeralRangeTemplate(position, position)
{
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate(const Range* range)
{
    if (!range)
        return;
    DCHECK(range->inShadowIncludingDocument());
    m_startPosition = fromPositionInDOMTree<Strategy>(range->startPosition());
    m_endPosition = fromPositionInDOMTree<Strategy>(range->endPosition());
#if DCHECK_IS_ON()
    m_domTreeVersion = range->ownerDocument().domTreeVersion();
#endif
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::EphemeralRangeTemplate()
{
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>::~EphemeralRangeTemplate()
{
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy>& EphemeralRangeTemplate<Strategy>::operator=(const EphemeralRangeTemplate<Strategy>& other)
{
    DCHECK(other.isValid());
    m_startPosition = other.m_startPosition;
    m_endPosition = other.m_endPosition;
#if DCHECK_IS_ON()
    m_domTreeVersion = other.m_domTreeVersion;
#endif
    return *this;
}

template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::operator==(const EphemeralRangeTemplate<Strategy>& other) const
{
    return startPosition() == other.startPosition() && endPosition() == other.endPosition();
}

template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::operator!=(const EphemeralRangeTemplate<Strategy>& other) const
{
    return !operator==(other);
}

template <typename Strategy>
Document& EphemeralRangeTemplate<Strategy>::document() const
{
    DCHECK(isNotNull());
    return *m_startPosition.document();
}

template <typename Strategy>
PositionTemplate<Strategy> EphemeralRangeTemplate<Strategy>::startPosition() const
{
    DCHECK(isValid());
    return m_startPosition;
}

template <typename Strategy>
PositionTemplate<Strategy> EphemeralRangeTemplate<Strategy>::endPosition() const
{
    DCHECK(isValid());
    return m_endPosition;
}

template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::isCollapsed() const
{
    DCHECK(isValid());
    return m_startPosition == m_endPosition;
}

template <typename Strategy>
EphemeralRangeTemplate<Strategy> EphemeralRangeTemplate<Strategy>::rangeOfContents(const Node& node)
{
    return EphemeralRangeTemplate<Strategy>(PositionTemplate<Strategy>::firstPositionInNode(&const_cast<Node&>(node)), PositionTemplate<Strategy>::lastPositionInNode(&const_cast<Node&>(node)));
}

#if DCHECK_IS_ON()
template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::isValid() const
{
    return m_startPosition.isNull() || m_domTreeVersion == m_startPosition.document()->domTreeVersion();
}
#else
template <typename Strategy>
bool EphemeralRangeTemplate<Strategy>::isValid() const
{
    return true;
}
#endif

Range* createRange(const EphemeralRange& range)
{
    if (range.isNull())
        return nullptr;
    return Range::create(range.document(), range.startPosition(), range.endPosition());
}

template class CORE_TEMPLATE_EXPORT EphemeralRangeTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT EphemeralRangeTemplate<EditingInFlatTreeStrategy>;

} // namespace blink
