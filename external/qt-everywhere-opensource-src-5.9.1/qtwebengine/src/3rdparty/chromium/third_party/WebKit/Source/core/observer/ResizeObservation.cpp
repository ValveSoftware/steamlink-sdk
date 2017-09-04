// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/observer/ResizeObservation.h"

#include "core/layout/LayoutBox.h"
#include "core/observer/ResizeObserver.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGGraphicsElement.h"

namespace blink {

ResizeObservation::ResizeObservation(Element* target, ResizeObserver* observer)
    : m_target(target),
      m_observer(observer),
      m_observationSize(0, 0),
      m_elementSizeChanged(true) {
  DCHECK(m_target);
  m_observer->elementSizeChanged();
}

bool ResizeObservation::observationSizeOutOfSync() {
  return m_elementSizeChanged && m_observationSize != computeTargetSize();
}

void ResizeObservation::setObservationSize(const LayoutSize& observationSize) {
  m_observationSize = observationSize;
  m_elementSizeChanged = false;
}

size_t ResizeObservation::targetDepth() {
  unsigned depth = 0;
  for (Element* parent = m_target; parent; parent = parent->parentElement())
    ++depth;
  return depth;
}

LayoutSize ResizeObservation::computeTargetSize() const {
  if (m_target) {
    if (m_target->isSVGElement() &&
        toSVGElement(m_target)->isSVGGraphicsElement()) {
      SVGGraphicsElement& svg = toSVGGraphicsElement(*m_target);
      return LayoutSize(svg.getBBox().size());
    }
    LayoutBox* layout = m_target->layoutBox();
    if (layout)
      return layout->contentSize();
  }
  return LayoutSize();
}

LayoutPoint ResizeObservation::computeTargetLocation() const {
  if (m_target && !m_target->isSVGElement()) {
    if (LayoutBox* layout = m_target->layoutBox())
      return LayoutPoint(layout->paddingLeft(), layout->paddingTop());
  }
  return LayoutPoint();
}

void ResizeObservation::elementSizeChanged() {
  m_elementSizeChanged = true;
  m_observer->elementSizeChanged();
}

DEFINE_TRACE(ResizeObservation) {
  visitor->trace(m_target);
  visitor->trace(m_observer);
}

}  // namespace blink
