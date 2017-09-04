// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FragmentainerIterator_h
#define FragmentainerIterator_h

#include "platform/geometry/LayoutRect.h"

namespace blink {

class LayoutFlowThread;
class LayoutMultiColumnSet;
class MultiColumnFragmentainerGroup;

// Used to find the fragmentainers that intersect with a given portion of the
// flow thread. The portion typically corresponds to the bounds of some
// descendant layout object. The iterator walks in block direction order.
class FragmentainerIterator {
 public:
  // Initialize the iterator, and move to the first fragmentainer of interest.
  // The clip rectangle is optional. If it's empty, it means that no clipping
  // will be performed, and that the only thing that can limit the set of
  // fragmentainers to visit is |physicalBoundingBox|.
  FragmentainerIterator(
      const LayoutFlowThread&,
      const LayoutRect& physicalBoundingBoxInFlowThread,
      const LayoutRect& clipRectInMulticolContainer = LayoutRect());

  // Advance to the next fragmentainer. Not allowed to call this if atEnd() is
  // true.
  void advance();

  // Return true if we have walked through all relevant fragmentainers.
  bool atEnd() const { return !m_currentColumnSet; }

  // The physical translation to apply to shift the box when converting from
  // flowthread to visual coordinates.
  LayoutSize paginationOffset() const;

  // Return the physical content box of the current fragmentainer, relative to
  // the flow thread.
  LayoutRect fragmentainerInFlowThread() const;

  // Return the physical clip rectangle of the current fragmentainer, relative
  // to the flow thread.
  LayoutRect clipRectInFlowThread() const;

 private:
  const LayoutFlowThread& m_flowThread;
  const LayoutRect m_clipRectInMulticolContainer;

  const LayoutMultiColumnSet* m_currentColumnSet;
  unsigned m_currentFragmentainerGroupIndex;
  unsigned m_currentFragmentainerIndex;
  unsigned m_endFragmentainerIndex;

  LayoutUnit m_logicalTopInFlowThread;
  LayoutUnit m_logicalBottomInFlowThread;

  const MultiColumnFragmentainerGroup& currentGroup() const;
  void moveToNextFragmentainerGroup();
  bool setFragmentainersOfInterest();
  void setAtEnd() { m_currentColumnSet = nullptr; }
  bool hasClipRect() const { return !m_clipRectInMulticolContainer.isEmpty(); }
};

}  // namespace blink

#endif  // FragmentainerIterator_h
