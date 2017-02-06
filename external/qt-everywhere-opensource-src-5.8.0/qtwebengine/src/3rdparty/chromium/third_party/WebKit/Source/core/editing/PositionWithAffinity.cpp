// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file./*

#include "core/editing/PositionWithAffinity.h"

namespace blink {

template <typename Strategy>
PositionWithAffinityTemplate<Strategy>::PositionWithAffinityTemplate(const PositionTemplate<Strategy>& position, TextAffinity affinity)
    : m_position(position)
    , m_affinity(affinity)
{
}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy>::PositionWithAffinityTemplate()
    : m_affinity(TextAffinity::Downstream)
{
}

template <typename Strategy>
PositionWithAffinityTemplate<Strategy>::~PositionWithAffinityTemplate()
{
}

template <typename Strategy>
bool PositionWithAffinityTemplate<Strategy>::operator==(const PositionWithAffinityTemplate& other) const
{
    if (isNull())
        return other.isNull();
    return m_affinity == other.m_affinity && m_position == other.m_position;
}

template class CORE_TEMPLATE_EXPORT PositionWithAffinityTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT PositionWithAffinityTemplate<EditingInFlatTreeStrategy>;

std::ostream& operator<<(std::ostream& ostream, const PositionWithAffinity& positionWithAffinity)
{
    return ostream << positionWithAffinity.position() << '/' << positionWithAffinity.affinity();
}

std::ostream& operator<<(std::ostream& ostream, const PositionInFlatTreeWithAffinity& positionWithAffinity)
{
    return ostream << positionWithAffinity.position() << '/' << positionWithAffinity.affinity();
}

} // namespace blink
