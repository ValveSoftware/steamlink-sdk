/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
 */

#include "core/paint/SVGPaintContext.h"

#include "core/frame/FrameHost.h"
#include "core/layout/svg/LayoutSVGResourceFilter.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/SVGMaskPainter.h"
#include "wtf/PtrUtil.h"

namespace blink {

SVGPaintContext::~SVGPaintContext() {
  if (m_filter) {
    DCHECK(SVGResourcesCache::cachedResourcesForLayoutObject(&m_object));
    DCHECK(SVGResourcesCache::cachedResourcesForLayoutObject(&m_object)
               ->filter() == m_filter);
    DCHECK(m_filterRecordingContext);
    SVGFilterPainter(*m_filter).finishEffect(m_object,
                                             *m_filterRecordingContext);

    // Reset the paint info after the filter effect has been completed.
    m_filterPaintInfo = nullptr;
  }

  if (m_masker) {
    DCHECK(SVGResourcesCache::cachedResourcesForLayoutObject(&m_object));
    DCHECK(SVGResourcesCache::cachedResourcesForLayoutObject(&m_object)
               ->masker() == m_masker);
    SVGMaskPainter(*m_masker).finishEffect(m_object, paintInfo().context);
  }
}

bool SVGPaintContext::applyClipMaskAndFilterIfNecessary() {
#if ENABLE(ASSERT)
  DCHECK(!m_applyClipMaskAndFilterIfNecessaryCalled);
  m_applyClipMaskAndFilterIfNecessaryCalled = true;
#endif

  // When rendering clip paths as masks, only geometric operations should be
  // included so skip non-geometric operations such as compositing, masking, and
  // filtering.
  if (paintInfo().isRenderingClipPathAsMaskImage()) {
    DCHECK(!m_object.isSVGRoot());
    applyClipIfNecessary();
    return true;
  }

  bool isSVGRoot = m_object.isSVGRoot();

  // Layer takes care of root opacity and blend mode.
  if (isSVGRoot) {
    DCHECK(!(m_object.isTransparent() || m_object.styleRef().hasBlendMode()) ||
           m_object.hasLayer());
  } else {
    applyCompositingIfNecessary();
  }

  if (isSVGRoot) {
    DCHECK(!m_object.styleRef().clipPath() || m_object.hasLayer());
  } else {
    applyClipIfNecessary();
  }

  SVGResources* resources =
      SVGResourcesCache::cachedResourcesForLayoutObject(&m_object);

  if (!applyMaskIfNecessary(resources))
    return false;

  if (isSVGRoot) {
    DCHECK(!m_object.styleRef().hasFilter() || m_object.hasLayer());
  } else if (!applyFilterIfNecessary(resources)) {
    return false;
  }

  if (!isIsolationInstalled() &&
      SVGLayoutSupport::isIsolationRequired(&m_object)) {
    m_compositingRecorder = wrapUnique(new CompositingRecorder(
        paintInfo().context, m_object, SkBlendMode::kSrcOver, 1));
  }

  return true;
}

void SVGPaintContext::applyCompositingIfNecessary() {
  DCHECK(!paintInfo().isRenderingClipPathAsMaskImage());

  const ComputedStyle& style = m_object.styleRef();
  float opacity = style.opacity();
  WebBlendMode blendMode = style.hasBlendMode() && m_object.isBlendingAllowed()
                               ? style.blendMode()
                               : WebBlendModeNormal;
  if (opacity < 1 || blendMode != WebBlendModeNormal) {
    const FloatRect compositingBounds =
        m_object.visualRectInLocalSVGCoordinates();
    m_compositingRecorder = wrapUnique(new CompositingRecorder(
        paintInfo().context, m_object,
        WebCoreCompositeToSkiaComposite(CompositeSourceOver, blendMode),
        opacity, &compositingBounds));
  }
}

void SVGPaintContext::applyClipIfNecessary() {
  ClipPathOperation* clipPathOperation = m_object.styleRef().clipPath();
  if (!clipPathOperation)
    return;
  m_clipPathClipper.emplace(paintInfo().context, *clipPathOperation, m_object,
                            m_object.objectBoundingBox(), FloatPoint());
}

bool SVGPaintContext::applyMaskIfNecessary(SVGResources* resources) {
  if (LayoutSVGResourceMasker* masker =
          resources ? resources->masker() : nullptr) {
    if (!SVGMaskPainter(*masker).prepareEffect(m_object, paintInfo().context))
      return false;
    m_masker = masker;
  }
  return true;
}

static bool hasReferenceFilterOnly(const ComputedStyle& style) {
  if (!style.hasFilter())
    return false;
  const FilterOperations& operations = style.filter();
  if (operations.size() != 1)
    return false;
  return operations.at(0)->type() == FilterOperation::REFERENCE;
}

bool SVGPaintContext::applyFilterIfNecessary(SVGResources* resources) {
  if (!resources)
    return !hasReferenceFilterOnly(m_object.styleRef());

  LayoutSVGResourceFilter* filter = resources->filter();
  if (!filter)
    return true;
  m_filterRecordingContext =
      wrapUnique(new SVGFilterRecordingContext(paintInfo().context));
  m_filter = filter;
  GraphicsContext* filterContext = SVGFilterPainter(*filter).prepareEffect(
      m_object, *m_filterRecordingContext);
  if (!filterContext)
    return false;

  // Because the filter needs to cache its contents we replace the context
  // during filtering with the filter's context.
  m_filterPaintInfo = wrapUnique(new PaintInfo(*filterContext, m_paintInfo));

  // Because we cache the filter contents and do not invalidate on paint
  // invalidation rect changes, we need to paint the entire filter region
  // so elements outside the initial paint (due to scrolling, etc) paint.
  m_filterPaintInfo->m_cullRect.m_rect = LayoutRect::infiniteIntRect();
  return true;
}

bool SVGPaintContext::isIsolationInstalled() const {
  if (m_compositingRecorder)
    return true;
  if (m_masker || m_filter)
    return true;
  if (m_clipPathClipper && m_clipPathClipper->usingMask())
    return true;
  return false;
}

void SVGPaintContext::paintSubtree(GraphicsContext& context,
                                   const LayoutObject* item) {
  DCHECK(item);
  DCHECK(!item->needsLayout());

  PaintInfo info(context, LayoutRect::infiniteIntRect(), PaintPhaseForeground,
                 GlobalPaintNormalPhase, PaintLayerNoFlag);
  item->paint(info, IntPoint());
}

bool SVGPaintContext::paintForLayoutObject(
    const PaintInfo& paintInfo,
    const ComputedStyle& style,
    const LayoutObject& layoutObject,
    LayoutSVGResourceMode resourceMode,
    SkPaint& paint,
    const AffineTransform* additionalPaintServerTransform) {
  if (paintInfo.isRenderingClipPathAsMaskImage()) {
    if (resourceMode == ApplyToStrokeMode)
      return false;
    paint.setColor(SVGComputedStyle::initialFillPaintColor().rgb());
    paint.setShader(nullptr);
    return true;
  }

  SVGPaintServer paintServer =
      SVGPaintServer::requestForLayoutObject(layoutObject, style, resourceMode);
  if (!paintServer.isValid())
    return false;

  if (additionalPaintServerTransform && paintServer.isTransformDependent())
    paintServer.prependTransform(*additionalPaintServerTransform);

  const SVGComputedStyle& svgStyle = style.svgStyle();
  float paintAlpha = resourceMode == ApplyToFillMode ? svgStyle.fillOpacity()
                                                     : svgStyle.strokeOpacity();
  paintServer.applyToSkPaint(paint, paintAlpha);

  // We always set filter quality to 'low' here. This value will only have an
  // effect for patterns, which are SkPictures, so using high-order filter
  // should have little effect on the overall quality.
  paint.setFilterQuality(kLow_SkFilterQuality);

  // TODO(fs): The color filter can set when generating a picture for a mask -
  // due to color-interpolation. We could also just apply the
  // color-interpolation property from the the shape itself (which could mean
  // the paintserver if it has it specified), since that would be more in line
  // with the spec for color-interpolation. For now, just steal it from the GC
  // though.
  // Additionally, it's not really safe/guaranteed to be correct, as
  // something down the paint pipe may want to farther tweak the color
  // filter, which could yield incorrect results. (Consider just using
  // saveLayer() w/ this color filter explicitly instead.)
  paint.setColorFilter(sk_ref_sp(paintInfo.context.getColorFilter()));
  return true;
}

}  // namespace blink
