// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/SelectionTemplate.h"

#include "wtf/Assertions.h"
#include <ostream>  // NOLINT

namespace blink {

template <typename Strategy>
SelectionTemplate<Strategy>::SelectionTemplate(const SelectionTemplate& other)
    : m_base(other.m_base),
      m_extent(other.m_extent),
      m_affinity(other.m_affinity),
      m_granularity(other.m_granularity),
      m_hasTrailingWhitespace(other.m_hasTrailingWhitespace),
      m_isDirectional(other.m_isDirectional)
#if DCHECK_IS_ON()
      ,
      m_domTreeVersion(other.m_domTreeVersion)
#endif
{
  DCHECK(other.assertValid());
  if (!m_hasTrailingWhitespace)
    return;
  DCHECK_EQ(m_granularity, WordGranularity) << *this;
}

template <typename Strategy>
SelectionTemplate<Strategy>::SelectionTemplate() = default;

template <typename Strategy>
bool SelectionTemplate<Strategy>::operator==(
    const SelectionTemplate& other) const {
  DCHECK(assertValid());
  DCHECK(other.assertValid());
  if (isNone())
    return other.isNone();
  if (other.isNone())
    return false;
  DCHECK_EQ(m_base.document(), other.document()) << *this << ' ' << other;
  return m_base == other.m_base && m_extent == other.m_extent &&
         m_affinity == other.m_affinity &&
         m_granularity == other.m_granularity &&
         m_hasTrailingWhitespace == other.m_hasTrailingWhitespace &&
         m_isDirectional == other.m_isDirectional;
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::operator!=(
    const SelectionTemplate& other) const {
  return !operator==(other);
}

template <typename Strategy>
const PositionTemplate<Strategy>& SelectionTemplate<Strategy>::base() const {
  DCHECK(assertValid());
  DCHECK(!m_base.isOrphan()) << m_base;
  return m_base;
}

template <typename Strategy>
Document* SelectionTemplate<Strategy>::document() const {
  DCHECK(assertValid());
  return m_base.document();
}

template <typename Strategy>
const PositionTemplate<Strategy>& SelectionTemplate<Strategy>::extent() const {
  DCHECK(assertValid());
  DCHECK(!m_extent.isOrphan()) << m_extent;
  return m_extent;
}

template <typename Strategy>
bool SelectionTemplate<Strategy>::assertValidFor(
    const Document& document) const {
  if (!assertValid())
    return false;
  if (m_base.isNull())
    return true;
  DCHECK_EQ(m_base.document(), document) << *this;
  return true;
}

#if DCHECK_IS_ON()
template <typename Strategy>
bool SelectionTemplate<Strategy>::assertValid() const {
  if (m_base.isNull())
    return true;
  DCHECK_EQ(m_base.document()->domTreeVersion(), m_domTreeVersion) << *this;
  DCHECK(!m_base.isOrphan()) << *this;
  DCHECK(!m_extent.isOrphan()) << *this;
  DCHECK_EQ(m_base.document(), m_extent.document());
  return true;
}
#else
template <typename Strategy>
bool SelectionTemplate<Strategy>::assertValid() const {
  return true;
}
#endif

template <typename Strategy>
void SelectionTemplate<Strategy>::printTo(std::ostream* ostream,
                                          const char* type) const {
  if (isNone()) {
    *ostream << "()";
    return;
  }
  *ostream << type << '(';
#if DCHECK_IS_ON()
  if (m_domTreeVersion != m_base.document()->domTreeVersion()) {
    *ostream << "Dirty: " << m_domTreeVersion;
    *ostream << " != " << m_base.document()->domTreeVersion() << ' ';
  }
#endif
  *ostream << "base: " << m_base << ", extent: " << m_extent << ')';
}

std::ostream& operator<<(std::ostream& ostream,
                         const SelectionInDOMTree& selection) {
  selection.printTo(&ostream, "Selection");
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         const SelectionInFlatTree& selection) {
  selection.printTo(&ostream, "SelectionInFlatTree");
  return ostream;
}

// --

template <typename Strategy>
SelectionTemplate<Strategy>::Builder::Builder(
    const SelectionTemplate<Strategy>& selection)
    : m_selection(selection) {}

template <typename Strategy>
SelectionTemplate<Strategy>::Builder::Builder() = default;

template <typename Strategy>
SelectionTemplate<Strategy> SelectionTemplate<Strategy>::Builder::build()
    const {
  DCHECK(m_selection.assertValid());
  return m_selection;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::collapse(
    const PositionTemplate<Strategy>& position) {
  DCHECK(position.isConnected()) << position;
  m_selection.m_base = position;
  m_selection.m_extent = position;
#if DCHECK_IS_ON()
  m_selection.m_domTreeVersion = position.document()->domTreeVersion();
#endif
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::collapse(
    const PositionWithAffinityTemplate<Strategy>& positionWithAffinity) {
  collapse(positionWithAffinity.position());
  setAffinity(positionWithAffinity.affinity());
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::extend(
    const PositionTemplate<Strategy>& position) {
  DCHECK(position.isConnected()) << position;
  DCHECK_EQ(m_selection.document(), position.document());
  DCHECK(m_selection.base().isConnected()) << m_selection.base();
  DCHECK(m_selection.assertValid());
  m_selection.m_extent = position;
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::selectAllChildren(const Node& node) {
  DCHECK(node.canContainRangeEndPoint()) << node;
  return setBaseAndExtent(
      EphemeralRangeTemplate<Strategy>::rangeOfContents(node));
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::setAffinity(TextAffinity affinity) {
  m_selection.m_affinity = affinity;
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::setBaseAndExtent(
    const EphemeralRangeTemplate<Strategy>& range) {
  if (range.isNull()) {
    m_selection.m_base = PositionTemplate<Strategy>();
    m_selection.m_extent = PositionTemplate<Strategy>();
#if DCHECK_IS_ON()
    m_selection.m_domTreeVersion = 0;
#endif
    return *this;
  }
  return collapse(range.startPosition()).extend(range.endPosition());
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::setBaseAndExtent(
    const PositionTemplate<Strategy>& base,
    const PositionTemplate<Strategy>& extent) {
  if (base.isNull()) {
    DCHECK(extent.isNull()) << extent;
    return setBaseAndExtent(EphemeralRangeTemplate<Strategy>());
  }
  DCHECK(extent.isNotNull());
  return collapse(base).extend(extent);
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::setBaseAndExtentDeprecated(
    const PositionTemplate<Strategy>& base,
    const PositionTemplate<Strategy>& extent) {
  if (base.isNotNull() && extent.isNotNull()) {
    return setBaseAndExtent(base, extent);
  }
  if (base.isNotNull())
    return collapse(base);
  if (extent.isNotNull())
    return collapse(extent);
  return setBaseAndExtent(EphemeralRangeTemplate<Strategy>());
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::setGranularity(
    TextGranularity granularity) {
  m_selection.m_granularity = granularity;
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::setHasTrailingWhitespace(
    bool hasTrailingWhitespace) {
  m_selection.m_hasTrailingWhitespace = hasTrailingWhitespace;
  return *this;
}

template <typename Strategy>
typename SelectionTemplate<Strategy>::Builder&
SelectionTemplate<Strategy>::Builder::setIsDirectional(bool isDirectional) {
  m_selection.m_isDirectional = isDirectional;
  return *this;
}

template class CORE_TEMPLATE_EXPORT SelectionTemplate<EditingStrategy>;
template class CORE_TEMPLATE_EXPORT
    SelectionTemplate<EditingInFlatTreeStrategy>;

}  // namespace blink
