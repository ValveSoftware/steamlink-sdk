// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EphemeralRange_h
#define EphemeralRange_h

#include "core/editing/Position.h"

namespace blink {

class Document;
class Range;

// We should restrict access to the unwanted version of |TraversalRange::end()|
// function.
template <class Iterator>
class TraversalRangeNodes : private TraversalRange<Iterator> {
  STACK_ALLOCATED();

 public:
  using StartNodeType = typename TraversalRange<Iterator>::StartNodeType;
  TraversalRangeNodes(const StartNodeType* start,
                      const StartNodeType* pastEndNode)
      : TraversalRange<Iterator>(start), m_pastEndNode(pastEndNode) {}

  using TraversalRange<Iterator>::begin;

  Iterator end() { return Iterator(m_pastEndNode); }

 private:
  const Member<const StartNodeType> m_pastEndNode;
};

// This class acts like |TraversalNextIterator| but in addition
// it allows to set current position and checks |m_current| pointer before
// dereferencing.
template <class TraversalNext>
class CheckedTraversalNextIterator
    : public TraversalIteratorBase<TraversalNext> {
  STACK_ALLOCATED();

  using TraversalIteratorBase<TraversalNext>::m_current;

 public:
  using StartNodeType = typename TraversalNext::TraversalNodeType;
  explicit CheckedTraversalNextIterator(const StartNodeType* start)
      : TraversalIteratorBase<TraversalNext>(
            const_cast<StartNodeType*>(start)) {}

  void operator++() {
    DCHECK(m_current);
    m_current = TraversalNext::next(*m_current);
  }
};

// Unlike |Range| objects, |EphemeralRangeTemplate| objects aren't relocated.
// You should not use |EphemeralRangeTemplate| objects after DOM modification.
//
// EphemeralRangeTemplate is supposed to use returning or passing start and end
// position.
//
//  Example usage:
//    Range* range = produceRange();
//    consumeRange(range);
//    ... no DOM modification ...
//    consumeRange2(range);
//
//  Above code should be:
//    EphemeralRangeTemplate range = produceRange();
//    consumeRange(range);
//    ... no DOM modification ...
//    consumeRange2(range);
//
//  Because of |Range| objects consume heap memory and inserted into |Range|
//  object list in |Document| for relocation. These operations are redundant
//  if |Range| objects doesn't live after DOM mutation.
//
template <typename Strategy>
class CORE_TEMPLATE_CLASS_EXPORT EphemeralRangeTemplate final {
  STACK_ALLOCATED();

 public:
  using RangeTraversal =
      TraversalRangeNodes<CheckedTraversalNextIterator<Strategy>>;

  EphemeralRangeTemplate(const PositionTemplate<Strategy>& start,
                         const PositionTemplate<Strategy>& end);
  EphemeralRangeTemplate(const EphemeralRangeTemplate& other);
  // |position| should be |Position::isNull()| or in-document.
  explicit EphemeralRangeTemplate(
      const PositionTemplate<Strategy>& /* position */);
  // When |range| is nullptr, |EphemeralRangeTemplate| is |isNull()|.
  explicit EphemeralRangeTemplate(const Range* /* range */);
  EphemeralRangeTemplate();
  ~EphemeralRangeTemplate();

  EphemeralRangeTemplate<Strategy>& operator=(
      const EphemeralRangeTemplate<Strategy>& other);

  bool operator==(const EphemeralRangeTemplate<Strategy>& other) const;
  bool operator!=(const EphemeralRangeTemplate<Strategy>& other) const;

  Document& document() const;
  PositionTemplate<Strategy> startPosition() const;
  PositionTemplate<Strategy> endPosition() const;

  // Returns true if |m_startPositoin| == |m_endPosition| or |isNull()|.
  bool isCollapsed() const;
  bool isNull() const {
    DCHECK(isValid());
    return m_startPosition.isNull();
  }
  bool isNotNull() const { return !isNull(); }

  RangeTraversal nodes() const;

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_startPosition);
    visitor->trace(m_endPosition);
  }

  // |node| should be in-document and valid for anchor node of
  // |PositionTemplate<Strategy>|.
  static EphemeralRangeTemplate<Strategy> rangeOfContents(
      const Node& /* node */);

 private:
  bool isValid() const;

  PositionTemplate<Strategy> m_startPosition;
  PositionTemplate<Strategy> m_endPosition;
#if DCHECK_IS_ON()
  uint64_t m_domTreeVersion;
#endif
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    EphemeralRangeTemplate<EditingStrategy>;
using EphemeralRange = EphemeralRangeTemplate<EditingStrategy>;

extern template class CORE_EXTERN_TEMPLATE_EXPORT
    EphemeralRangeTemplate<EditingInFlatTreeStrategy>;
using EphemeralRangeInFlatTree =
    EphemeralRangeTemplate<EditingInFlatTreeStrategy>;

// Returns a newly created |Range| object from |range| or |nullptr| if
// |range.isNull()| returns true.
CORE_EXPORT Range* createRange(const EphemeralRange& /* range */);

}  // namespace blink

#endif
