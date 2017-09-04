// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollState_h
#define ScrollState_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/page/scrolling/ScrollStateInit.h"
#include "platform/scroll/ScrollStateData.h"
#include "wtf/Forward.h"
#include <deque>
#include <memory>

namespace blink {

class Element;

class CORE_EXPORT ScrollState final
    : public GarbageCollectedFinalized<ScrollState>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ScrollState* create(ScrollStateInit);
  static ScrollState* create(std::unique_ptr<ScrollStateData>);

  ~ScrollState() {}

  // Web exposed methods.

  // Reduce deltas by x, y.
  void consumeDelta(double x, double y, ExceptionState&);
  // Pops the first element off of |m_scrollChain| and calls |distributeScroll|
  // on it.
  void distributeToScrollChainDescendant();
  int positionX() { return m_data->position_x; };
  int positionY() { return m_data->position_y; };
  // Positive when scrolling right.
  double deltaX() const { return m_data->delta_x; };
  // Positive when scrolling down.
  double deltaY() const { return m_data->delta_y; };
  // Indicates the smallest delta the input device can produce. 0 for
  // unquantized inputs.
  double deltaGranularity() const { return m_data->delta_granularity; };
  // Positive if moving right.
  double velocityX() const { return m_data->velocity_x; };
  // Positive if moving down.
  double velocityY() const { return m_data->velocity_y; };
  // True for events dispatched after the users's gesture has finished.
  bool inInertialPhase() const { return m_data->is_in_inertial_phase; };
  // True if this is the first event for this scroll.
  bool isBeginning() const { return m_data->is_beginning; };
  // True if this is the last event for this scroll.
  bool isEnding() const { return m_data->is_ending; };
  // True if this scroll is the direct result of user input.
  bool fromUserInput() const { return m_data->from_user_input; };
  // True if this scroll is the result of the user interacting directly with
  // the screen, e.g., via touch.
  bool isDirectManipulation() const { return m_data->is_direct_manipulation; }
  // True if this scroll is allowed to bubble upwards.
  bool shouldPropagate() const { return m_data->should_propagate; };

  // Non web exposed methods.
  void consumeDeltaNative(double x, double y);

  // TODO(tdresser): this needs to be web exposed. See crbug.com/483091.
  void setScrollChain(std::deque<int> scrollChain) {
    m_scrollChain = scrollChain;
  }

  Element* currentNativeScrollingElement() const;
  void setCurrentNativeScrollingElement(Element*);

  void setCurrentNativeScrollingElementById(int elementId);

  bool deltaConsumedForScrollSequence() const {
    return m_data->delta_consumed_for_scroll_sequence;
  }

  // Scroll begin and end must propagate to all nodes to ensure
  // their state is updated.
  bool fullyConsumed() const {
    return !m_data->delta_x && !m_data->delta_y && !m_data->is_ending &&
           !m_data->is_beginning;
  }

  ScrollStateData* data() const { return m_data.get(); }

  DEFINE_INLINE_TRACE() {}

 private:
  ScrollState();
  explicit ScrollState(std::unique_ptr<ScrollStateData>);

  std::unique_ptr<ScrollStateData> m_data;
  std::deque<int> m_scrollChain;
};

}  // namespace blink

#endif  // ScrollState_h
