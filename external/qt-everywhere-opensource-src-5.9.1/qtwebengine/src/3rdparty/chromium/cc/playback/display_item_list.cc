// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/display_item_list.h"

#include <stddef.h>

#include <string>

#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/debug/picture_debug_util.h"
#include "cc/debug/traced_display_item_list.h"
#include "cc/debug/traced_value.h"
#include "cc/playback/display_item_list_settings.h"
#include "cc/playback/display_item_proto_factory.h"
#include "cc/playback/drawing_display_item.h"
#include "cc/playback/largest_display_item.h"
#include "cc/proto/display_item.pb.h"
#include "cc/proto/gfx_conversions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

// We don't perform per-layer solid color analysis when there are too many skia
// operations.
const int kOpCountThatIsOkToAnalyze = 10;

bool DisplayItemsTracingEnabled() {
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items"), &tracing_enabled);
  return tracing_enabled;
}

bool GetCanvasClipBounds(SkCanvas* canvas, gfx::Rect* clip_bounds) {
  SkRect canvas_clip_bounds;
  if (!canvas->getClipBounds(&canvas_clip_bounds))
    return false;
  *clip_bounds = ToEnclosingRect(gfx::SkRectToRectF(canvas_clip_bounds));
  return true;
}

const int kDefaultNumDisplayItemsToReserve = 100;

}  // namespace

DisplayItemList::Inputs::Inputs(const DisplayItemListSettings& settings)
    : items(LargestDisplayItemSize(),
            LargestDisplayItemSize() * kDefaultNumDisplayItemsToReserve),
      settings(settings) {}

DisplayItemList::Inputs::~Inputs() {}

scoped_refptr<DisplayItemList> DisplayItemList::Create(
    const DisplayItemListSettings& settings) {
  return make_scoped_refptr(new DisplayItemList(settings));
}

scoped_refptr<DisplayItemList> DisplayItemList::CreateFromProto(
    const proto::DisplayItemList& proto,
    ClientPictureCache* client_picture_cache,
    std::vector<uint32_t>* used_engine_picture_ids) {
  scoped_refptr<DisplayItemList> list =
      DisplayItemList::Create(DisplayItemListSettings(proto.settings()));

  for (int i = 0; i < proto.items_size(); i++) {
    const proto::DisplayItem& item_proto = proto.items(i);
    const gfx::Rect visual_rect = ProtoToRect(proto.visual_rects(i));
    DisplayItemProtoFactory::AllocateAndConstruct(
        visual_rect, list.get(), item_proto, client_picture_cache,
        used_engine_picture_ids);
  }

  list->Finalize();

  return list;
}

DisplayItemList::DisplayItemList(const DisplayItemListSettings& settings)
    : inputs_(settings) {}

DisplayItemList::~DisplayItemList() {
}

void DisplayItemList::ToProtobuf(proto::DisplayItemList* proto) {
  // The flattened SkPicture approach is going away, and the proto
  // doesn't currently support serializing that flattened picture.
  inputs_.settings.ToProtobuf(proto->mutable_settings());

  DCHECK_EQ(0, proto->items_size());
  DCHECK_EQ(0, proto->visual_rects_size());
  DCHECK(inputs_.items.size() == inputs_.visual_rects.size())
      << "items.size() " << inputs_.items.size() << " visual_rects.size() "
      << inputs_.visual_rects.size();
  int i = 0;
  for (const auto& item : inputs_.items) {
    RectToProto(inputs_.visual_rects[i++], proto->add_visual_rects());
    item.ToProtobuf(proto->add_items());
  }
}

void DisplayItemList::Raster(SkCanvas* canvas,
                             SkPicture::AbortCallback* callback,
                             const gfx::Rect& canvas_target_playback_rect,
                             float contents_scale) const {
  canvas->save();
  if (!canvas_target_playback_rect.IsEmpty()) {
    // canvas_target_playback_rect is specified in device space. We can't
    // use clipRect because canvas CTM will be applied on it. Use clipRegion
    // instead because it ignores canvas CTM.
    SkRegion device_clip;
    device_clip.setRect(gfx::RectToSkIRect(canvas_target_playback_rect));
    canvas->clipRegion(device_clip);
  }
  canvas->scale(contents_scale, contents_scale);
  Raster(canvas, callback);
  canvas->restore();
}

DISABLE_CFI_PERF
void DisplayItemList::Raster(SkCanvas* canvas,
                             SkPicture::AbortCallback* callback) const {
  gfx::Rect canvas_playback_rect;
  if (!GetCanvasClipBounds(canvas, &canvas_playback_rect))
    return;

  std::vector<size_t> indices;
  rtree_.Search(canvas_playback_rect, &indices);
  for (size_t index : indices) {
    inputs_.items[index].Raster(canvas, callback);
    // We use a callback during solid color analysis on the compositor thread to
    // break out early. Since we're handling a sequence of pictures via rtree
    // query results ourselves, we have to respect the callback and early out.
    if (callback && callback->abort())
      break;
  }
}

void DisplayItemList::GrowCurrentBeginItemVisualRect(
    const gfx::Rect& visual_rect) {
  if (!inputs_.begin_item_indices.empty())
    inputs_.visual_rects[inputs_.begin_item_indices.back()].Union(visual_rect);
}

void DisplayItemList::Finalize() {
  TRACE_EVENT0("cc", "DisplayItemList::Finalize");
  // TODO(dtrainor): Need to deal with serializing inputs_.visual_rects.
  // http://crbug.com/568757.
  DCHECK(inputs_.items.size() == inputs_.visual_rects.size())
      << "items.size() " << inputs_.items.size() << " visual_rects.size() "
      << inputs_.visual_rects.size();
  rtree_.Build(inputs_.visual_rects);

  // TODO(wkorman): Restore the below, potentially with a switch to allow
  // clearing visual rects except for Blimp engine. http://crbug.com/633750
  // if (!retain_visual_rects_)
  //   // This clears both the vector and the vector's capacity, since
  //   // visual_rects won't be used anymore.
  //   std::vector<gfx::Rect>().swap(inputs_.visual_rects);
}

bool DisplayItemList::IsSuitableForGpuRasterization() const {
  // TODO(wkorman): This is more permissive than Picture's implementation, since
  // none of the items might individually trigger a veto even though they
  // collectively have enough "bad" operations that a corresponding Picture
  // would get vetoed. See crbug.com/513016.
  return inputs_.all_items_are_suitable_for_gpu_rasterization;
}

int DisplayItemList::ApproximateOpCount() const {
  return approximate_op_count_;
}

DISABLE_CFI_PERF
size_t DisplayItemList::ApproximateMemoryUsage() const {
  size_t memory_usage = sizeof(*this);

  size_t external_memory_usage = 0;
  // Warning: this double-counts SkPicture data if use_cached_picture is
  // also true.
  for (const auto& item : inputs_.items) {
    external_memory_usage += item.ExternalMemoryUsage();
  }

  // Memory outside this class due to |items_|.
  memory_usage += inputs_.items.GetCapacityInBytes() + external_memory_usage;

  // TODO(jbroman): Does anything else owned by this class substantially
  // contribute to memory usage?
  // TODO(vmpstr): Probably DiscardableImageMap is worth counting here.

  return memory_usage;
}

bool DisplayItemList::ShouldBeAnalyzedForSolidColor() const {
  return ApproximateOpCount() <= kOpCountThatIsOkToAnalyze;
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
DisplayItemList::AsValue(bool include_items) const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());

  state->BeginDictionary("params");
  if (include_items) {
    state->BeginArray("items");
    size_t item_index = 0;
    for (const DisplayItem& item : inputs_.items) {
      item.AsValueInto(item_index < inputs_.visual_rects.size()
                           ? inputs_.visual_rects[item_index]
                           : gfx::Rect(),
                       state.get());
      item_index++;
    }
    state->EndArray();  // "items".
  }
  state->SetValue("layer_rect", MathUtil::AsValue(rtree_.GetBounds()));
  state->EndDictionary();  // "params".

  SkPictureRecorder recorder;
  gfx::Rect bounds = rtree_.GetBounds();
  SkCanvas* canvas = recorder.beginRecording(bounds.width(), bounds.height());
  canvas->translate(-bounds.x(), -bounds.y());
  canvas->clipRect(gfx::RectToSkRect(bounds));
  Raster(canvas, nullptr, gfx::Rect(), 1.f);
  sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

  std::string b64_picture;
  PictureDebugUtil::SerializeAsBase64(picture.get(), &b64_picture);
  state->SetString("skp64", b64_picture);

  return std::move(state);
}

void DisplayItemList::EmitTraceSnapshot() const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items") ","
      TRACE_DISABLED_BY_DEFAULT("cc.debug.picture") ","
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.picture"),
      "cc::DisplayItemList", this,
      TracedDisplayItemList::AsTraceableDisplayItemList(this,
          DisplayItemsTracingEnabled()));
}

void DisplayItemList::GenerateDiscardableImagesMetadata() {
  // This should be only called once, and only after CreateAndCacheSkPicture.
  DCHECK(image_map_.empty());

  gfx::Rect bounds = rtree_.GetBounds();
  DiscardableImageMap::ScopedMetadataGenerator generator(
      &image_map_, gfx::Size(bounds.right(), bounds.bottom()));
  Raster(generator.canvas(), nullptr, gfx::Rect(), 1.f);
}

void DisplayItemList::GetDiscardableImagesInRect(
    const gfx::Rect& rect,
    const gfx::SizeF& raster_scales,
    std::vector<DrawImage>* images) {
  image_map_.GetDiscardableImagesInRect(rect, raster_scales, images);
}

}  // namespace cc
