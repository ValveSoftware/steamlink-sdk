/*
 * Copyright (C) 2012 Victor Carbune (victor@rosedu.org)
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

#include "core/layout/LayoutVTTCue.h"

#include "core/frame/Settings.h"
#include "core/html/shadow/MediaControls.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutState.h"
#include "wtf/MathExtras.h"

namespace blink {

namespace {

class SnapToLinesLayouter {
  STACK_ALLOCATED();

 public:
  SnapToLinesLayouter(LayoutVTTCue& cueBox, const IntRect& controlsRect)
      : m_cueBox(cueBox), m_controlsRect(controlsRect), m_margin(0.0) {
    if (Settings* settings = m_cueBox.document().settings())
      m_margin = settings->textTrackMarginPercentage() / 100.0;
  }

  void layout();

 private:
  bool isOutside(const IntRect&) const;
  bool isOverlapping() const;
  LayoutUnit computeInitialPositionAdjustment(LayoutUnit&,
                                              LayoutUnit,
                                              LayoutUnit) const;
  bool shouldSwitchDirection(InlineFlowBox*, LayoutUnit, LayoutUnit) const;

  void moveBoxesBy(LayoutUnit distance) {
    m_cueBox.setLogicalTop(m_cueBox.logicalTop() + distance);
  }

  InlineFlowBox* findFirstLineBox() const;

  LayoutPoint m_specifiedPosition;
  LayoutVTTCue& m_cueBox;
  IntRect m_controlsRect;
  double m_margin;
};

InlineFlowBox* SnapToLinesLayouter::findFirstLineBox() const {
  if (!m_cueBox.firstChild()->isLayoutInline())
    return nullptr;
  return toLayoutInline(m_cueBox.firstChild())->firstLineBox();
}

LayoutUnit SnapToLinesLayouter::computeInitialPositionAdjustment(
    LayoutUnit& step,
    LayoutUnit maxDimension,
    LayoutUnit margin) const {
  DCHECK(std::isfinite(m_cueBox.snapToLinesPosition()));

  // 6. Let line be cue's computed line.
  // 7. Round line to an integer by adding 0.5 and then flooring it.
  LayoutUnit linePosition(floorf(m_cueBox.snapToLinesPosition() + 0.5f));

  WritingMode writingMode = m_cueBox.style()->getWritingMode();
  // 8. Vertical Growing Left: Add one to line then negate it.
  if (writingMode == RightToLeftWritingMode)
    linePosition = -(linePosition + 1);

  // 9. Let position be the result of multiplying step and line offset.
  LayoutUnit position = step * linePosition;

  // 10. Vertical Growing Left: Decrease position by the width of the
  // bounding box of the boxes in boxes, then increase position by step.
  if (writingMode == RightToLeftWritingMode) {
    position -= m_cueBox.size().width();
    position += step;
  }

  // 11. If line is less than zero...
  if (linePosition < 0) {
    // ... then increase position by max dimension ...
    position += maxDimension;

    // ... and negate step.
    step = -step;
  } else {
    // ... Otherwise, increase position by margin.
    position += margin;
  }
  return position;
}

// We use this helper to make sure all (bounding) boxes used for comparisons
// are relative to the same coordinate space. If we didn't the (bounding) boxes
// could be affect by transforms on an ancestor et.c, which could yield
// incorrect results.
IntRect contentBoxRelativeToAncestor(const LayoutBox& box,
                                     const LayoutBoxModelObject& ancestor) {
  FloatRect cueContentBox(box.contentBoxRect());
  // We pass UseTransforms here primarily because we use a transform for
  // non-snap-to-lines positioning (see VTTCue.cpp.)
  FloatQuad mappedContentQuad =
      box.localToAncestorQuad(cueContentBox, &ancestor, UseTransforms);
  return mappedContentQuad.enclosingBoundingBox();
}

IntRect cueBoundingBox(const LayoutBox& cueBox) {
  return contentBoxRelativeToAncestor(cueBox, *cueBox.containingBlock());
}

bool SnapToLinesLayouter::isOutside(const IntRect& titleArea) const {
  return !titleArea.contains(cueBoundingBox(m_cueBox));
}

bool SnapToLinesLayouter::isOverlapping() const {
  IntRect cueBoxRect = cueBoundingBox(m_cueBox);
  for (LayoutBox* box = m_cueBox.previousSiblingBox(); box;
       box = box->previousSiblingBox()) {
    if (cueBoxRect.intersects(cueBoundingBox(*box)))
      return true;
  }
  return cueBoxRect.intersects(m_controlsRect);
}

bool SnapToLinesLayouter::shouldSwitchDirection(InlineFlowBox* firstLineBox,
                                                LayoutUnit step,
                                                LayoutUnit margin) const {
  // 17. Horizontal: If step is negative and the top of the first line box in
  // boxes is now above the top of the title area, or if step is positive and
  // the bottom of the first line box in boxes is now below the bottom of the
  // title area, jump to the step labeled switch direction.
  // Vertical: If step is negative and the left edge of the first line
  // box in boxes is now to the left of the left edge of the title area, or
  // if step is positive and the right edge of the first line box in boxes is
  // now to the right of the right edge of the title area, jump to the step
  // labeled switch direction.
  LayoutUnit logicalTop = m_cueBox.logicalTop();
  if (step < 0 && logicalTop < margin)
    return true;
  if (step > 0 &&
      logicalTop + firstLineBox->logicalHeight() + margin >
          m_cueBox.containingBlock()->logicalHeight())
    return true;
  return false;
}

void SnapToLinesLayouter::layout() {
  // http://dev.w3.org/html5/webvtt/#dfn-apply-webvtt-cue-settings
  // Step 13, "If cue's text track cue snap-to-lines flag is set".

  InlineFlowBox* firstLineBox = findFirstLineBox();
  if (!firstLineBox)
    return;

  // 1. Horizontal: Let margin be a user-agent-defined vertical length which
  // will be used to define a margin at the top and bottom edges of the video
  // into which cues will not be placed.
  //    Vertical: Let margin be a user-agent-defined horizontal length which
  // will be used to define a margin at the top and bottom edges of the video
  // into which cues will not be placed.
  // 2. Horizontal: Let full dimension be the height of video's rendering area
  //    Vertical: Let full dimension be the width of video's rendering area.
  WritingMode writingMode = m_cueBox.style()->getWritingMode();
  LayoutBlock* parentBlock = m_cueBox.containingBlock();
  LayoutUnit fullDimension = blink::isHorizontalWritingMode(writingMode)
                                 ? parentBlock->size().height()
                                 : parentBlock->size().width();
  LayoutUnit margin(fullDimension * m_margin);

  // 3. Let max dimension be full dimension - (2 * margin)
  LayoutUnit maxDimension = fullDimension - 2 * margin;

  // 4. Horizontal: Let step be the height of the first line box in boxes.
  //    Vertical: Let step be the width of the first line box in boxes.
  LayoutUnit step = firstLineBox->logicalHeight();

  // 5. If step is zero, then jump to the step labeled done positioning below.
  if (!step)
    return;

  // Steps 6-11.
  LayoutUnit positionAdjustment =
      computeInitialPositionAdjustment(step, maxDimension, margin);

  // 12. Move all boxes in boxes ...
  // Horizontal: ... down by the distance given by position
  // Vertical: ... right by the distance given by position
  moveBoxesBy(positionAdjustment);

  // 13. Remember the position of all the boxes in boxes as their specified
  // position.
  m_specifiedPosition = m_cueBox.location();

  // XX. Let switched be false.
  bool switched = false;

  // 14. Horizontal: Let title area be a box that covers all of the video's
  // rendering area except for a height of margin at the top of the rendering
  // area and a height of margin at the bottom of the rendering area.
  // Vertical: Let title area be a box that covers all of the video’s
  // rendering area except for a width of margin at the left of the rendering
  // area and a width of margin at the right of the rendering area.
  IntRect titleArea =
      enclosingIntRect(m_cueBox.containingBlock()->contentBoxRect());
  if (blink::isHorizontalWritingMode(writingMode)) {
    titleArea.move(0, margin.toInt());
    titleArea.contract(0, (2 * margin).toInt());
  } else {
    titleArea.move(margin.toInt(), 0);
    titleArea.contract((2 * margin).toInt(), 0);
  }

  // 15. Step loop: If none of the boxes in boxes would overlap any of the
  // boxes in output, and all of the boxes in output are entirely within the
  // title area box, then jump to the step labeled done positioning below.
  while (isOutside(titleArea) || isOverlapping()) {
    // 16. Let current position score be the percentage of the area of the
    // bounding box of the boxes in boxes that is outside the title area
    // box.
    if (!shouldSwitchDirection(firstLineBox, step, margin)) {
      // 18. Horizontal: Move all the boxes in boxes down by the distance
      // given by step. (If step is negative, then this will actually
      // result in an upwards movement of the boxes in absolute terms.)
      // Vertical: Move all the boxes in boxes right by the distance
      // given by step. (If step is negative, then this will actually
      // result in a leftwards movement of the boxes in absolute terms.)
      moveBoxesBy(step);

      // 19. Jump back to the step labeled step loop.
      continue;
    }

    // 20. Switch direction: If switched is true, then remove all the boxes
    // in boxes, and jump to the step labeled done positioning below.
    if (switched) {
      // This does not "remove" the boxes, but rather just pushes them
      // out of the viewport. Otherwise we'd need to mutate the layout
      // tree during layout.
      m_cueBox.setLogicalTop(m_cueBox.containingBlock()->logicalHeight() + 1);
      break;
    }

    // 21. Otherwise, move all the boxes in boxes back to their specified
    // position as determined in the earlier step.
    m_cueBox.setLocation(m_specifiedPosition);

    // 22. Negate step.
    step = -step;

    // 23. Set switched to true.
    switched = true;

    // 24. Jump back to the step labeled step loop.
  }
}

}  // unnamed namespace

LayoutVTTCue::LayoutVTTCue(ContainerNode* node, float snapToLinesPosition)
    : LayoutBlockFlow(node), m_snapToLinesPosition(snapToLinesPosition) {}

void LayoutVTTCue::repositionCueSnapToLinesNotSet() {
  // FIXME: Implement overlapping detection when snap-to-lines is not set.
  // http://wkb.ug/84296

  // http://dev.w3.org/html5/webvtt/#dfn-apply-webvtt-cue-settings
  // Step 13, "If cue's text track cue snap-to-lines flag is not set".

  // 1. Let bounding box be the bounding box of the boxes in boxes.

  // 2. Run the appropriate steps from the following list:
  //    If the text track cue writing direction is horizontal
  //       If the text track cue line alignment is middle alignment
  //          Move all the boxes in boxes up by half of the height of
  //          bounding box.
  //       If the text track cue line alignment is end alignment
  //          Move all the boxes in boxes up by the height of bounding box.
  //
  //    If the text track cue writing direction is vertical growing left or
  //    vertical growing right
  //       If the text track cue line alignment is middle alignment
  //          Move all the boxes in boxes left by half of the width of
  //          bounding box.
  //       If the text track cue line alignment is end alignment
  //          Move all the boxes in boxes left by the width of bounding box.

  // 3. If none of the boxes in boxes would overlap any of the boxes in
  // output, and all the boxes in output are within the video's rendering
  // area, then jump to the step labeled done positioning below.

  // 4. If there is a position to which the boxes in boxes can be moved while
  // maintaining the relative positions of the boxes in boxes to each other
  // such that none of the boxes in boxes would overlap any of the boxes in
  // output, and all the boxes in output would be within the video's
  // rendering area, then move the boxes in boxes to the closest such
  // position to their current position, and then jump to the step labeled
  // done positioning below. If there are multiple such positions that are
  // equidistant from their current position, use the highest one amongst
  // them; if there are several at that height, then use the leftmost one
  // amongst them.

  // 5. Otherwise, jump to the step labeled done positioning below. (The
  // boxes will unfortunately overlap.)
}

IntRect LayoutVTTCue::computeControlsRect() const {
  // Determine the area covered by the media controls, if any. If the controls
  // are present, they are the next sibling of the text track container, which
  // is our parent. (LayoutMedia ensures that the media controls are laid out
  // before text tracks, so that the layout is up to date here.)
  DCHECK(parent()->node()->isTextTrackContainer());
  LayoutObject* controlsContainer = parent()->nextSibling();
  if (!controlsContainer)
    return IntRect();
  // Only a part of the media controls is used for overlap avoidance.
  MediaControls* controls = toMediaControls(controlsContainer->node());
  LayoutObject* controlsLayout = controls->layoutObjectForTextTrackLayout();
  // The (second part of the) following is mostly defensive - in general
  // there should be a LayoutBox representing the part of the controls that
  // are relevant for overlap avoidance. (The controls pseudo elements are
  // generally reachable from outside the shadow tree though, hence the
  // "mostly".)
  if (!controlsLayout || !controlsLayout->isBox())
    return IntRect();
  // Assume that the controls container are positioned in the same relative
  // position as the text track container. (LayoutMedia::layout ensures this.)
  return contentBoxRelativeToAncestor(toLayoutBox(*controlsLayout),
                                      toLayoutBox(*controlsContainer));
}

void LayoutVTTCue::layout() {
  LayoutBlockFlow::layout();

  DCHECK(firstChild());

  LayoutState state(*this);

  // http://dev.w3.org/html5/webvtt/#dfn-apply-webvtt-cue-settings - step 13.
  if (!std::isnan(m_snapToLinesPosition))
    SnapToLinesLayouter(*this, computeControlsRect()).layout();
  else
    repositionCueSnapToLinesNotSet();
}

}  // namespace blink
