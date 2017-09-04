// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/CompositorMutableState.h"

#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "platform/graphics/CompositorMutation.h"

namespace blink {
CompositorMutableState::CompositorMutableState(CompositorMutation* mutation,
                                               cc::LayerImpl* main,
                                               cc::LayerImpl* scroll)
    : m_mutation(mutation), m_mainLayer(main), m_scrollLayer(scroll) {}

CompositorMutableState::~CompositorMutableState() {}

double CompositorMutableState::opacity() const {
  return m_mainLayer->Opacity();
}

void CompositorMutableState::setOpacity(double opacity) {
  if (!m_mainLayer)
    return;
  m_mainLayer->layer_tree_impl()
      ->property_trees()
      ->effect_tree.OnOpacityAnimated(opacity, m_mainLayer->effect_tree_index(),
                                      m_mainLayer->layer_tree_impl());
  m_mutation->setOpacity(opacity);
}

const SkMatrix44& CompositorMutableState::transform() const {
  return m_mainLayer ? m_mainLayer->Transform().matrix() : SkMatrix44::I();
}

void CompositorMutableState::setTransform(const SkMatrix44& matrix) {
  if (!m_mainLayer)
    return;
  m_mainLayer->layer_tree_impl()
      ->property_trees()
      ->transform_tree.OnTransformAnimated(gfx::Transform(matrix),
                                           m_mainLayer->transform_tree_index(),
                                           m_mainLayer->layer_tree_impl());
  m_mutation->setTransform(matrix);
}

float CompositorMutableState::scrollLeft() const {
  return m_scrollLayer ? m_scrollLayer->CurrentScrollOffset().x() : 0.0;
}

void CompositorMutableState::setScrollLeft(float scrollLeft) {
  if (!m_scrollLayer)
    return;

  gfx::ScrollOffset offset = m_scrollLayer->CurrentScrollOffset();
  offset.set_x(scrollLeft);
  m_scrollLayer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.OnScrollOffsetAnimated(
          m_scrollLayer->id(), m_scrollLayer->transform_tree_index(),
          m_scrollLayer->scroll_tree_index(), offset,
          m_scrollLayer->layer_tree_impl());
  m_mutation->setScrollLeft(scrollLeft);
}

float CompositorMutableState::scrollTop() const {
  return m_scrollLayer ? m_scrollLayer->CurrentScrollOffset().y() : 0.0;
}

void CompositorMutableState::setScrollTop(float scrollTop) {
  if (!m_scrollLayer)
    return;
  gfx::ScrollOffset offset = m_scrollLayer->CurrentScrollOffset();
  offset.set_y(scrollTop);
  m_scrollLayer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.OnScrollOffsetAnimated(
          m_scrollLayer->id(), m_scrollLayer->transform_tree_index(),
          m_scrollLayer->scroll_tree_index(), offset,
          m_scrollLayer->layer_tree_impl());
  m_mutation->setScrollTop(scrollTop);
}

}  // namespace blink
