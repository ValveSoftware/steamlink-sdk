// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutableState_h
#define CompositorMutableState_h

#include "platform/PlatformExport.h"

class SkMatrix44;

namespace cc {
class LayerImpl;
}  // namespace cc

namespace blink {

class CompositorMutation;

// This class wraps the compositor-owned, mutable state for a single element.
class PLATFORM_EXPORT CompositorMutableState {
 public:
  CompositorMutableState(CompositorMutation*,
                         cc::LayerImpl* main,
                         cc::LayerImpl* scroll);
  ~CompositorMutableState();

  double opacity() const;
  void setOpacity(double);

  const SkMatrix44& transform() const;
  void setTransform(const SkMatrix44&);

  float scrollLeft() const;
  void setScrollLeft(float);

  float scrollTop() const;
  void setScrollTop(float);

 private:
  CompositorMutation* m_mutation;
  cc::LayerImpl* m_mainLayer;
  cc::LayerImpl* m_scrollLayer;
};

}  // namespace blink

#endif  // CompositorMutableState_h
