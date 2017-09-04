/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc.
 *               All rights reserved.
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

#include "core/layout/LayoutEmbeddedObject.h"

#include "core/CSSValueKeywords.h"
#include "core/HTMLNames.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/LayoutView.h"
#include "core/page/Page.h"
#include "core/paint/EmbeddedObjectPaintInvalidator.h"
#include "core/paint/EmbeddedObjectPainter.h"
#include "core/plugins/PluginView.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

using namespace HTMLNames;

LayoutEmbeddedObject::LayoutEmbeddedObject(Element* element)
    : LayoutPart(element) {
  view()->frameView()->setIsVisuallyNonEmpty();
}

LayoutEmbeddedObject::~LayoutEmbeddedObject() {}

PaintLayerType LayoutEmbeddedObject::layerTypeRequired() const {
  // This can't just use LayoutPart::layerTypeRequired, because
  // PaintLayerCompositor doesn't loop through LayoutEmbeddedObjects the way it
  // does frames in order to update the self painting bit on their Layer.
  // Also, unlike iframes, embeds don't used the usesCompositing bit on
  // LayoutView in requiresAcceleratedCompositing.
  if (requiresAcceleratedCompositing())
    return NormalPaintLayer;
  return LayoutPart::layerTypeRequired();
}

static String localizedUnavailablePluginReplacementText(
    Node* node,
    LayoutEmbeddedObject::PluginAvailability availability) {
  Locale& locale = node ? toElement(node)->locale() : Locale::defaultLocale();
  switch (availability) {
    case LayoutEmbeddedObject::PluginAvailable:
      break;
    case LayoutEmbeddedObject::PluginMissing:
      return locale.queryString(WebLocalizedString::MissingPluginText);
    case LayoutEmbeddedObject::PluginBlockedByContentSecurityPolicy:
      return locale.queryString(WebLocalizedString::BlockedPluginText);
  }
  NOTREACHED();
  return String();
}

void LayoutEmbeddedObject::setPluginAvailability(
    PluginAvailability availability) {
  DCHECK_EQ(PluginAvailable, m_pluginAvailability);
  m_pluginAvailability = availability;

  m_unavailablePluginReplacementText =
      localizedUnavailablePluginReplacementText(node(), availability);

  // node() is nullptr when LayoutPart is being destroyed.
  if (node())
    setShouldDoFullPaintInvalidation();
}

bool LayoutEmbeddedObject::showsUnavailablePluginIndicator() const {
  return m_pluginAvailability != PluginAvailable;
}

void LayoutEmbeddedObject::paintContents(const PaintInfo& paintInfo,
                                         const LayoutPoint& paintOffset) const {
  Element* element = toElement(node());
  if (!isHTMLPlugInElement(element))
    return;

  LayoutPart::paintContents(paintInfo, paintOffset);
}

void LayoutEmbeddedObject::paint(const PaintInfo& paintInfo,
                                 const LayoutPoint& paintOffset) const {
  if (showsUnavailablePluginIndicator()) {
    LayoutReplaced::paint(paintInfo, paintOffset);
    return;
  }

  LayoutPart::paint(paintInfo, paintOffset);
}

void LayoutEmbeddedObject::paintReplaced(const PaintInfo& paintInfo,
                                         const LayoutPoint& paintOffset) const {
  EmbeddedObjectPainter(*this).paintReplaced(paintInfo, paintOffset);
}

PaintInvalidationReason LayoutEmbeddedObject::invalidatePaintIfNeeded(
    const PaintInvalidatorContext& context) const {
  return EmbeddedObjectPaintInvalidator(*this, context)
      .invalidatePaintIfNeeded();
}

void LayoutEmbeddedObject::layout() {
  ASSERT(needsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  updateLogicalWidth();
  updateLogicalHeight();

  m_overflow.reset();
  addVisualEffectOverflow();

  updateLayerTransformAfterLayout();

  Widget* widget = this->widget();
  if (!widget && frameView())
    frameView()->addPartToUpdate(*this);

  clearNeedsLayout();
}

ScrollResult LayoutEmbeddedObject::scroll(ScrollGranularity granularity,
                                          const FloatSize&) {
  return ScrollResult();
}

CompositingReasons LayoutEmbeddedObject::additionalCompositingReasons() const {
  if (requiresAcceleratedCompositing())
    return CompositingReasonPlugin;
  return CompositingReasonNone;
}

LayoutReplaced* LayoutEmbeddedObject::embeddedReplacedContent() const {
  if (!node() || !widget() || !widget()->isFrameView())
    return nullptr;
  return toFrameView(widget())->embeddedReplacedContent();
}

}  // namespace blink
