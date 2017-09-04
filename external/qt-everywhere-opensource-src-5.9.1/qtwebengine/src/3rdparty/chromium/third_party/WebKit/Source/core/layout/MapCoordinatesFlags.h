// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MapCoordinatesFlags_h
#define MapCoordinatesFlags_h

namespace blink {

enum MapCoordinatesMode {
  IsFixed = 1 << 0,
  UseTransforms = 1 << 1,

  // When walking up the containing block chain, applies a container flip for
  // the first element found, if any, for which isFlippedBlocksWritingMode is
  // true. This option should generally be used when mapping a source rect in
  // the "physical coordinates with flipped block-flow" coordinate space (see
  // LayoutBoxModelObject.h) to one in a physical destination space.
  ApplyContainerFlip = 1 << 2,
  TraverseDocumentBoundaries = 1 << 3,

  // Applies to LayoutView::mapLocalToAncestor() and LayoutView::
  // mapToVisualRectInAncestorSpace() only, to indicate the input point or rect
  // is in frame coordinates instead of frame contents coordinates. This
  // disables view clipping and scroll offset adjustment.
  // TODO(wangxianzhu): Remove this when root-layer-scrolls launches.
  InputIsInFrameCoordinates = 1 << 4,
};
typedef unsigned MapCoordinatesFlags;

}  // namespace blink

#endif  // MapCoordinatesFlags_h
