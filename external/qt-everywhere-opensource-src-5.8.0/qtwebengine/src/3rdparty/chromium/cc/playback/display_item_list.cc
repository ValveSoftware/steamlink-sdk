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

const int kDefaultNumDisplayItemsToReserve = 100;

}  // namespace

scoped_refptr<DisplayItemList> DisplayItemList::Create(
    const gfx::Rect& layer_rect,
    const DisplayItemListSettings& settings) {
  return make_scoped_refptr(new DisplayItemList(
      layer_rect, settings,
      !settings.use_cached_picture || DisplayItemsTracingEnabled()));
}

scoped_refptr<DisplayItemList> DisplayItemList::CreateFromProto(
    const proto::DisplayItemList& proto,
    ClientPictureCache* client_picture_cache,
    std::vector<uint32_t>* used_engine_picture_ids) {
  gfx::Rect layer_rect = ProtoToRect(proto.layer_rect());
  scoped_refptr<DisplayItemList> list =
      DisplayItemList::Create(ProtoToRect(proto.layer_rect()),
                              DisplayItemListSettings(proto.settings()));

  for (int i = 0; i < proto.items_size(); i++) {
    const proto::DisplayItem& item_proto = proto.items(i);
    DisplayItemProtoFactory::AllocateAndConstruct(
        layer_rect, list.get(), item_proto, client_picture_cache,
        used_engine_picture_ids);
  }

  list->Finalize();

  return list;
}

DisplayItemList::DisplayItemList(gfx::Rect layer_rect,
                                 const DisplayItemListSettings& settings,
                                 bool retain_individual_display_items)
    : items_(LargestDisplayItemSize(),
             LargestDisplayItemSize() * kDefaultNumDisplayItemsToReserve),
      settings_(settings),
      retain_individual_display_items_(retain_individual_display_items),
      layer_rect_(layer_rect),
      is_suitable_for_gpu_rasterization_(true),
      approximate_op_count_(0),
      picture_memory_usage_(0) {
  if (settings_.use_cached_picture) {
    SkRTreeFactory factory;
    recorder_.reset(new SkPictureRecorder());

    SkCanvas* canvas = recorder_->beginRecording(
        layer_rect_.width(), layer_rect_.height(), &factory);
    canvas->translate(-layer_rect_.x(), -layer_rect_.y());
    canvas->clipRect(gfx::RectToSkRect(layer_rect_));
  }
}

DisplayItemList::~DisplayItemList() {
}

void DisplayItemList::ToProtobuf(proto::DisplayItemList* proto) {
  // The flattened SkPicture approach is going away, and the proto
  // doesn't currently support serializing that flattened picture.
  DCHECK(retain_individual_display_items_);

  RectToProto(layer_rect_, proto->mutable_layer_rect());
  settings_.ToProtobuf(proto->mutable_settings());

  DCHECK_EQ(0, proto->items_size());
  for (const auto& item : items_)
    item.ToProtobuf(proto->add_items());
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

void DisplayItemList::Raster(SkCanvas* canvas,
                             SkPicture::AbortCallback* callback) const {
  if (!settings_.use_cached_picture) {
    for (const auto& item : items_)
      item.Raster(canvas, callback);
  } else {
    DCHECK(picture_);

    canvas->save();
    canvas->translate(layer_rect_.x(), layer_rect_.y());
    if (callback) {
      // If we have a callback, we need to call |draw()|, |drawPicture()|
      // doesn't take a callback.  This is used by |AnalysisCanvas| to early
      // out.
      picture_->playback(canvas, callback);
    } else {
      // Prefer to call |drawPicture()| on the canvas since it could place the
      // entire picture on the canvas instead of parsing the skia operations.
      canvas->drawPicture(picture_.get());
    }
    canvas->restore();
  }
}

void DisplayItemList::ProcessAppendedItem(const DisplayItem* item) {
  if (settings_.use_cached_picture) {
    DCHECK(recorder_);
    item->Raster(recorder_->getRecordingCanvas(), nullptr);
  }
  if (!retain_individual_display_items_) {
    items_.Clear();
  }
}

void DisplayItemList::RasterIntoCanvas(const DisplayItem& item) {
  DCHECK(recorder_);
  DCHECK(!retain_individual_display_items_);

  item.Raster(recorder_->getRecordingCanvas(), nullptr);
}

bool DisplayItemList::RetainsIndividualDisplayItems() const {
  return retain_individual_display_items_;
}

void DisplayItemList::Finalize() {
  TRACE_EVENT0("cc", "DisplayItemList::Finalize");
  // TODO(dtrainor): Need to deal with serializing visual_rects_.
  // http://crbug.com/568757.
  DCHECK(!retain_individual_display_items_ ||
         items_.size() == visual_rects_.size())
      << "items.size() " << items_.size() << " visual_rects.size() "
      << visual_rects_.size();

  // TODO(vmpstr): Build and make use of an RTree from the visual
  // rects. For now we just clear them out since we won't ever need
  // them to stick around post-Finalize. http://crbug.com/527245
  // This clears both the vector and the vector's capacity, since visual_rects_
  // won't be used anymore.
  std::vector<gfx::Rect>().swap(visual_rects_);

  if (settings_.use_cached_picture) {
    // Convert to an SkPicture for faster rasterization.
    DCHECK(settings_.use_cached_picture);
    DCHECK(!picture_);
    picture_ = recorder_->finishRecordingAsPicture();
    DCHECK(picture_);
    picture_memory_usage_ =
        SkPictureUtils::ApproximateBytesUsed(picture_.get());
    recorder_.reset();
  }
}

bool DisplayItemList::IsSuitableForGpuRasterization() const {
  return is_suitable_for_gpu_rasterization_;
}

int DisplayItemList::ApproximateOpCount() const {
  if (retain_individual_display_items_)
    return approximate_op_count_;
  return picture_ ? picture_->approximateOpCount() : 0;
}

size_t DisplayItemList::ApproximateMemoryUsage() const {
  // We double-count in this case. Produce zero to avoid being misleading.
  if (settings_.use_cached_picture && retain_individual_display_items_)
    return 0;

  DCHECK(!settings_.use_cached_picture || picture_);

  size_t memory_usage = sizeof(*this);

  size_t external_memory_usage = 0;
  if (retain_individual_display_items_) {
    // Warning: this double-counts SkPicture data if use_cached_picture is
    // also true.
    for (const auto& item : items_) {
      external_memory_usage += item.ExternalMemoryUsage();
    }
  }

  // Memory outside this class due to |items_|.
  memory_usage += items_.GetCapacityInBytes() + external_memory_usage;

  // Memory outside this class due to |picture|.
  memory_usage += picture_memory_usage_;

  // TODO(jbroman): Does anything else owned by this class substantially
  // contribute to memory usage?

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
    for (const DisplayItem& item : items_) {
      item.AsValueInto(item_index < visual_rects_.size()
                           ? visual_rects_[item_index]
                           : gfx::Rect(),
                       state.get());
      item_index++;
    }
    state->EndArray();  // "items".
  }
  state->SetValue("layer_rect", MathUtil::AsValue(layer_rect_));
  state->EndDictionary();  // "params".

  if (!layer_rect_.IsEmpty()) {
    SkPictureRecorder recorder;
    SkCanvas* canvas =
        recorder.beginRecording(layer_rect_.width(), layer_rect_.height());
    canvas->translate(-layer_rect_.x(), -layer_rect_.y());
    canvas->clipRect(gfx::RectToSkRect(layer_rect_));
    Raster(canvas, NULL, gfx::Rect(), 1.f);
    sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

    std::string b64_picture;
    PictureDebugUtil::SerializeAsBase64(picture.get(), &b64_picture);
    state->SetString("skp64", b64_picture);
  }

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
  DCHECK(!settings_.use_cached_picture || picture_);
  if (settings_.use_cached_picture && !picture_->willPlayBackBitmaps())
    return;

  // The cached picture is translated by -layer_rect_.origin during record,
  // so we need to offset that back in order to get right positioning for
  // images.
  DiscardableImageMap::ScopedMetadataGenerator generator(
      &image_map_, gfx::Size(layer_rect_.right(), layer_rect_.bottom()));
  Raster(generator.canvas(), nullptr,
         gfx::Rect(layer_rect_.right(), layer_rect_.bottom()), 1.f);
}

void DisplayItemList::GetDiscardableImagesInRect(
    const gfx::Rect& rect,
    float raster_scale,
    std::vector<DrawImage>* images) {
  image_map_.GetDiscardableImagesInRect(rect, raster_scale, images);
}

}  // namespace cc
