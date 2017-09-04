/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/css/resolver/StyleResolverState.h"

#include "core/animation/css/CSSAnimations.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Node.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/FrameHost.h"
#include "core/layout/api/LayoutViewItem.h"

namespace blink {

StyleResolverState::StyleResolverState(
    Document& document,
    const ElementResolveContext& elementContext,
    const ComputedStyle* parentStyle)
    : m_elementContext(elementContext),
      m_document(document),
      m_style(nullptr),
      // TODO(jchaffraix): We should make m_parentStyle const
      // (https://crbug.com/468152)
      m_parentStyle(const_cast<ComputedStyle*>(parentStyle)),
      m_applyPropertyToRegularStyle(true),
      m_applyPropertyToVisitedLinkStyle(false),
      m_hasDirAutoAttribute(false),
      m_fontBuilder(document),
      m_elementStyleResources(document, document.devicePixelRatio()) {
  if (!m_parentStyle) {
    // TODO(jchaffraix): We should make m_parentStyle const
    // (https://crbug.com/468152)
    m_parentStyle = const_cast<ComputedStyle*>(m_elementContext.parentStyle());
  }

  DCHECK(document.isActive());
}

StyleResolverState::StyleResolverState(Document& document,
                                       Element* element,
                                       const ComputedStyle* parentStyle)
    : StyleResolverState(document,
                         element ? ElementResolveContext(*element)
                                 : ElementResolveContext(document),
                         parentStyle) {}

StyleResolverState::~StyleResolverState() {
  // For performance reasons, explicitly clear HeapVectors and
  // HeapHashMaps to avoid giving a pressure on Oilpan's GC.
  m_animationUpdate.clear();
}

void StyleResolverState::setStyle(PassRefPtr<ComputedStyle> style) {
  // FIXME: Improve RAII of StyleResolverState to remove this function.
  m_style = style;
  m_cssToLengthConversionData = CSSToLengthConversionData(
      m_style.get(), rootElementStyle(), document().layoutViewItem(),
      m_style->effectiveZoom());
}

CSSToLengthConversionData StyleResolverState::fontSizeConversionData() const {
  float em = parentStyle()->specifiedFontSize();
  float rem = rootElementStyle() ? rootElementStyle()->specifiedFontSize() : 1;
  CSSToLengthConversionData::FontSizes fontSizes(em, rem,
                                                 &parentStyle()->font());
  CSSToLengthConversionData::ViewportSize viewportSize(
      document().layoutViewItem());

  return CSSToLengthConversionData(style(), fontSizes, viewportSize, 1);
}

void StyleResolverState::loadPendingResources() {
  m_elementStyleResources.loadPendingResources(style());
}

void StyleResolverState::setCustomPropertySetForApplyAtRule(
    const String& string,
    StylePropertySet* customPropertySet) {
  m_customPropertySetsForApplyAtRule.set(string, customPropertySet);
}

StylePropertySet* StyleResolverState::customPropertySetForApplyAtRule(
    const String& string) {
  return m_customPropertySetsForApplyAtRule.get(string);
}

HeapHashMap<CSSPropertyID, Member<const CSSValue>>&
StyleResolverState::parsedPropertiesForPendingSubstitutionCache(
    const CSSPendingSubstitutionValue& value) const {
  HeapHashMap<CSSPropertyID, Member<const CSSValue>>* map =
      m_parsedPropertiesForPendingSubstitutionCache.get(&value);
  if (!map) {
    map = new HeapHashMap<CSSPropertyID, Member<const CSSValue>>;
    m_parsedPropertiesForPendingSubstitutionCache.set(&value, map);
  }
  return *map;
}

}  // namespace blink
