/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "core/paint/PaintLayerResourceInfo.h"

#include "core/paint/PaintLayer.h"
#include "platform/graphics/filters/FilterEffect.h"

namespace blink {

PaintLayerResourceInfo::PaintLayerResourceInfo(PaintLayer* layer)
    : m_layer(layer) {}

PaintLayerResourceInfo::~PaintLayerResourceInfo() {
  DCHECK(!m_layer);
}

TreeScope* PaintLayerResourceInfo::treeScope() {
  DCHECK(m_layer);
  Node* node = m_layer->layoutObject()->node();
  return node ? &node->treeScope() : nullptr;
}

void PaintLayerResourceInfo::resourceContentChanged() {
  DCHECK(m_layer);
  LayoutObject* layoutObject = m_layer->layoutObject();
  layoutObject->setShouldDoFullPaintInvalidation();
  const ComputedStyle& style = layoutObject->styleRef();
  if (style.hasFilter() && style.filter().hasReferenceFilter())
    invalidateFilterChain();
}

void PaintLayerResourceInfo::resourceElementChanged() {
  DCHECK(m_layer);
  LayoutObject* layoutObject = m_layer->layoutObject();
  layoutObject->setShouldDoFullPaintInvalidation();
  const ComputedStyle& style = layoutObject->styleRef();
  if (style.hasFilter() && style.filter().hasReferenceFilter())
    invalidateFilterChain();
}

void PaintLayerResourceInfo::setLastEffect(FilterEffect* lastEffect) {
  m_lastEffect = lastEffect;
}

FilterEffect* PaintLayerResourceInfo::lastEffect() const {
  return m_lastEffect;
}

void PaintLayerResourceInfo::invalidateFilterChain() {
  m_lastEffect = nullptr;
}

DEFINE_TRACE(PaintLayerResourceInfo) {
  visitor->trace(m_lastEffect);
  SVGResourceClient::trace(visitor);
}

}  // namespace blink
