// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGFilterPainter.h"

#include "core/layout/svg/LayoutSVGResourceFilter.h"
#include "core/paint/FilterEffectBuilder.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/svg/SVGFilterElement.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "wtf/PtrUtil.h"

namespace blink {

GraphicsContext* SVGFilterRecordingContext::beginContent(
    FilterData* filterData) {
  DCHECK_EQ(filterData->m_state, FilterData::Initial);

  // Create a new context so the contents of the filter can be drawn and cached.
  m_paintController = PaintController::create();
  m_context = wrapUnique(new GraphicsContext(*m_paintController));

  filterData->m_state = FilterData::RecordingContent;
  return m_context.get();
}

void SVGFilterRecordingContext::endContent(FilterData* filterData) {
  DCHECK_EQ(filterData->m_state, FilterData::RecordingContent);

  Filter* filter = filterData->lastEffect->getFilter();
  SourceGraphic* sourceGraphic = filter->getSourceGraphic();
  DCHECK(sourceGraphic);

  // Use the context that contains the filtered content.
  DCHECK(m_paintController);
  DCHECK(m_context);
  m_context->beginRecording(filter->filterRegion());
  m_paintController->commitNewDisplayItems();
  m_paintController->paintArtifact().replay(*m_context);

  SkiaImageFilterBuilder::buildSourceGraphic(sourceGraphic,
                                             m_context->endRecording());

  // Content is cached by the source graphic so temporaries can be freed.
  m_paintController = nullptr;
  m_context = nullptr;

  filterData->m_state = FilterData::ReadyToPaint;
}

static void paintFilteredContent(GraphicsContext& context,
                                 const LayoutObject& object,
                                 FilterData* filterData) {
  if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(
          context, object, DisplayItem::kSVGFilter))
    return;

  FloatRect filterBounds =
      filterData ? filterData->lastEffect->getFilter()->filterRegion()
                 : FloatRect();
  LayoutObjectDrawingRecorder recorder(context, object, DisplayItem::kSVGFilter,
                                       filterBounds);
  if (!filterData || filterData->m_state != FilterData::ReadyToPaint)
    return;
  DCHECK(filterData->lastEffect->getFilter()->getSourceGraphic());

  filterData->m_state = FilterData::PaintingFilter;

  FilterEffect* lastEffect = filterData->lastEffect;
  sk_sp<SkImageFilter> imageFilter =
      SkiaImageFilterBuilder::build(lastEffect, ColorSpaceDeviceRGB);
  context.save();

  // Clip drawing of filtered image to the minimum required paint rect.
  context.clipRect(lastEffect->mapRect(object.strokeBoundingBox()));

  context.beginLayer(1, SkBlendMode::kSrcOver, &filterBounds, ColorFilterNone,
                     std::move(imageFilter));
  context.endLayer();
  context.restore();

  filterData->m_state = FilterData::ReadyToPaint;
}

GraphicsContext* SVGFilterPainter::prepareEffect(
    const LayoutObject& object,
    SVGFilterRecordingContext& recordingContext) {
  m_filter.clearInvalidationMask();

  if (FilterData* filterData = m_filter.getFilterDataForLayoutObject(&object)) {
    // If the filterData already exists we do not need to record the content
    // to be filtered. This can occur if the content was previously recorded
    // or we are in a cycle.
    if (filterData->m_state == FilterData::PaintingFilter)
      filterData->m_state = FilterData::PaintingFilterCycleDetected;

    if (filterData->m_state == FilterData::RecordingContent)
      filterData->m_state = FilterData::RecordingContentCycleDetected;

    return nullptr;
  }

  SVGFilterGraphNodeMap* nodeMap = SVGFilterGraphNodeMap::create();
  FilterEffectBuilder builder(nullptr, object.objectBoundingBox(), 1);
  Filter* filter = builder.buildReferenceFilter(
      toSVGFilterElement(*m_filter.element()), nullptr, nodeMap);
  if (!filter || !filter->lastEffect())
    return nullptr;

  IntRect sourceRegion = enclosingIntRect(
      intersection(filter->filterRegion(), object.strokeBoundingBox()));
  filter->getSourceGraphic()->setSourceRect(sourceRegion);

  FilterData* filterData = FilterData::create();
  filterData->lastEffect = filter->lastEffect();
  filterData->nodeMap = nodeMap;

  // TODO(pdr): Can this be moved out of painter?
  m_filter.setFilterDataForLayoutObject(const_cast<LayoutObject*>(&object),
                                        filterData);
  return recordingContext.beginContent(filterData);
}

void SVGFilterPainter::finishEffect(
    const LayoutObject& object,
    SVGFilterRecordingContext& recordingContext) {
  FilterData* filterData = m_filter.getFilterDataForLayoutObject(&object);
  if (filterData) {
    // A painting cycle can occur when an FeImage references a source that
    // makes use of the FeImage itself. This is the first place we would hit
    // the cycle so we reset the state and continue.
    if (filterData->m_state == FilterData::PaintingFilterCycleDetected)
      filterData->m_state = FilterData::PaintingFilter;

    // Check for RecordingContent here because we may can be re-painting
    // without re-recording the contents to be filtered.
    if (filterData->m_state == FilterData::RecordingContent)
      recordingContext.endContent(filterData);

    if (filterData->m_state == FilterData::RecordingContentCycleDetected)
      filterData->m_state = FilterData::RecordingContent;
  }
  paintFilteredContent(recordingContext.paintingContext(), object, filterData);
}

}  // namespace blink
