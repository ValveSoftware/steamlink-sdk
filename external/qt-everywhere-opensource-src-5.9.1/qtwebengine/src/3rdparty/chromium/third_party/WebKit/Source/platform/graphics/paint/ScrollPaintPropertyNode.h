// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollPaintPropertyNode_h
#define ScrollPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/scroll/MainThreadScrollingReason.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

#include <iosfwd>

namespace blink {

using MainThreadScrollingReasons = uint32_t;

// A scroll node contains auxiliary scrolling information for threaded scrolling
// which includes how far an area can be scrolled, which transform node contains
// the scroll offset, etc.
//
// Main thread scrolling reasons force scroll updates to go to the main thread
// and can have dependencies on other nodes. For example, all parents of a
// scroll node with background attachment fixed set should also have it set.
class PLATFORM_EXPORT ScrollPaintPropertyNode
    : public RefCounted<ScrollPaintPropertyNode> {
 public:
  static ScrollPaintPropertyNode* root();

  static PassRefPtr<ScrollPaintPropertyNode> create(
      PassRefPtr<ScrollPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> scrollOffsetTranslation,
      const IntSize& clip,
      const IntSize& bounds,
      bool userScrollableHorizontal,
      bool userScrollableVertical) {
    return adoptRef(new ScrollPaintPropertyNode(
        std::move(parent), std::move(scrollOffsetTranslation), clip, bounds,
        userScrollableHorizontal, userScrollableVertical));
  }

  void update(
      PassRefPtr<ScrollPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> scrollOffsetTranslation,
      const IntSize& clip,
      const IntSize& bounds,
      bool userScrollableHorizontal,
      bool userScrollableVertical) {
    DCHECK(!isRoot());
    DCHECK(parent != this);
    m_parent = parent;
    DCHECK(scrollOffsetTranslation->matrix().isIdentityOr2DTranslation());
    m_scrollOffsetTranslation = scrollOffsetTranslation;
    m_clip = clip;
    m_bounds = bounds;
    m_userScrollableHorizontal = userScrollableHorizontal;
    m_userScrollableVertical = userScrollableVertical;
    m_mainThreadScrollingReasons = 0;
  }

  ScrollPaintPropertyNode* parent() const { return m_parent.get(); }
  bool isRoot() const { return !m_parent; }

  // Transform that the scroll is relative to.
  const TransformPaintPropertyNode* scrollOffsetTranslation() const {
    return m_scrollOffsetTranslation.get();
  }

  // The clipped area that contains the scrolled content.
  const IntSize& clip() const { return m_clip; }

  // The bounds of the content that is scrolled within |clip|.
  const IntSize& bounds() const { return m_bounds; }

  bool userScrollableHorizontal() const { return m_userScrollableHorizontal; }
  bool userScrollableVertical() const { return m_userScrollableVertical; }

  // Return reason bitfield with values from cc::MainThreadScrollingReason.
  MainThreadScrollingReasons mainThreadScrollingReasons() const {
    return m_mainThreadScrollingReasons;
  }
  bool hasMainThreadScrollingReasons(MainThreadScrollingReasons reasons) const {
    return m_mainThreadScrollingReasons & reasons;
  }
  void addMainThreadScrollingReasons(MainThreadScrollingReasons reasons) {
    m_mainThreadScrollingReasons |= reasons;
  }
  void clearMainThreadScrollingReasons() { m_mainThreadScrollingReasons = 0; }

#if DCHECK_IS_ON()
  // The clone function is used by FindPropertiesNeedingUpdate.h for recording
  // a scroll node before it has been updated, to later detect changes.
  PassRefPtr<ScrollPaintPropertyNode> clone() const {
    RefPtr<ScrollPaintPropertyNode> cloned =
        adoptRef(new ScrollPaintPropertyNode(
            m_parent, m_scrollOffsetTranslation, m_clip, m_bounds,
            m_userScrollableHorizontal, m_userScrollableVertical));
    cloned->addMainThreadScrollingReasons(m_mainThreadScrollingReasons);
    return cloned;
  }

  // The equality operator is used by FindPropertiesNeedingUpdate.h for checking
  // if a scroll node has changed.
  bool operator==(const ScrollPaintPropertyNode& o) const {
    // TODO(pdr): Check main thread scrolling reason equality as well. We do
    // not yet mark nodes as needing a paint property update on main thread
    // scrolling reason changes. See: See: https://crbug.com/664672.
    return m_parent == o.m_parent &&
           m_scrollOffsetTranslation == o.m_scrollOffsetTranslation &&
           m_clip == o.m_clip && m_bounds == o.m_bounds &&
           m_userScrollableHorizontal == o.m_userScrollableHorizontal &&
           m_userScrollableVertical == o.m_userScrollableVertical;
  }
#endif

 private:
  ScrollPaintPropertyNode(
      PassRefPtr<ScrollPaintPropertyNode> parent,
      PassRefPtr<const TransformPaintPropertyNode> scrollOffsetTranslation,
      IntSize clip,
      IntSize bounds,
      bool userScrollableHorizontal,
      bool userScrollableVertical)
      : m_parent(parent),
        m_scrollOffsetTranslation(scrollOffsetTranslation),
        m_clip(clip),
        m_bounds(bounds),
        m_userScrollableHorizontal(userScrollableHorizontal),
        m_userScrollableVertical(userScrollableVertical),
        m_mainThreadScrollingReasons(0) {
    DCHECK(m_scrollOffsetTranslation->matrix().isIdentityOr2DTranslation());
  }

  RefPtr<ScrollPaintPropertyNode> m_parent;
  RefPtr<const TransformPaintPropertyNode> m_scrollOffsetTranslation;
  IntSize m_clip;
  IntSize m_bounds;
  bool m_userScrollableHorizontal;
  bool m_userScrollableVertical;
  MainThreadScrollingReasons m_mainThreadScrollingReasons;
  // TODO(pdr): Add an offset for the clip and bounds to the transform.
  // TODO(pdr): Add 2 bits for whether this is a viewport scroll node.
  // TODO(pdr): Add a bit for whether this is affected by page scale.
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const ScrollPaintPropertyNode&, std::ostream*);

}  // namespace blink

#endif  // ScrollPaintPropertyNode_h
