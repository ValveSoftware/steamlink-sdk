// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file./*

#ifndef PositionWithAffinity_h
#define PositionWithAffinity_h

#include "core/CoreExport.h"
#include "core/editing/Position.h"
#include "core/editing/TextAffinity.h"
#include <iosfwd>

namespace blink {

template <typename Strategy>
class CORE_TEMPLATE_CLASS_EXPORT PositionWithAffinityTemplate {
    DISALLOW_NEW();
public:
    // TODO(yosin) We should have single parameter constructor not to use
    // default parameter for avoiding include "TextAffinity.h"
    PositionWithAffinityTemplate(const PositionTemplate<Strategy>&, TextAffinity = TextAffinity::Downstream);
    PositionWithAffinityTemplate();
    ~PositionWithAffinityTemplate();

    TextAffinity affinity() const { return m_affinity; }
    const PositionTemplate<Strategy>& position() const { return m_position; }

    // Returns true if both |this| and |other| is null or both |m_position|
    // and |m_affinity| equal.
    bool operator==(const PositionWithAffinityTemplate& other) const;
    bool operator!=(const PositionWithAffinityTemplate& other) const { return !operator==(other); }

    bool isNotNull() const { return m_position.isNotNull(); }
    bool isNull() const { return m_position.isNull(); }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_position);
    }

private:
    PositionTemplate<Strategy> m_position;
    TextAffinity m_affinity;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT PositionWithAffinityTemplate<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT PositionWithAffinityTemplate<EditingInFlatTreeStrategy>;

using PositionWithAffinity = PositionWithAffinityTemplate<EditingStrategy>;
using PositionInFlatTreeWithAffinity = PositionWithAffinityTemplate<EditingInFlatTreeStrategy>;

template <typename Strategy>
PositionWithAffinityTemplate<Strategy> fromPositionInDOMTree(const PositionWithAffinity&);

template <>
inline PositionWithAffinity fromPositionInDOMTree<EditingStrategy>(const PositionWithAffinity& positionWithAffinity)
{
    return positionWithAffinity;
}

template <>
inline PositionInFlatTreeWithAffinity fromPositionInDOMTree<EditingInFlatTreeStrategy>(const PositionWithAffinity& positionWithAffinity)
{
    return PositionInFlatTreeWithAffinity(toPositionInFlatTree(positionWithAffinity.position()), positionWithAffinity.affinity());
}

CORE_EXPORT std::ostream& operator<<(std::ostream&, const PositionWithAffinity&);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const PositionInFlatTreeWithAffinity&);

} // namespace blink

#endif // PositionWithAffinity_h
