// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture.h"

#include <algorithm>
#include <limits>
#include <set>

#include "base/base64.h"
#include "base/debug/trace_event.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/base/util.h"
#include "cc/debug/traced_picture.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/content_layer_client.h"
#include "skia/ext/pixel_ref_utils.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkDrawFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

SkData* EncodeBitmap(size_t* offset, const SkBitmap& bm) {
  const int kJpegQuality = 80;
  std::vector<unsigned char> data;

  // If bitmap is opaque, encode as JPEG.
  // Otherwise encode as PNG.
  bool encoding_succeeded = false;
  if (bm.isOpaque()) {
    SkAutoLockPixels lock_bitmap(bm);
    if (bm.empty())
      return NULL;

    encoding_succeeded = gfx::JPEGCodec::Encode(
        reinterpret_cast<unsigned char*>(bm.getAddr32(0, 0)),
        gfx::JPEGCodec::FORMAT_SkBitmap,
        bm.width(),
        bm.height(),
        bm.rowBytes(),
        kJpegQuality,
        &data);
  } else {
    encoding_succeeded = gfx::PNGCodec::EncodeBGRASkBitmap(bm, false, &data);
  }

  if (encoding_succeeded) {
    *offset = 0;
    return SkData::NewWithCopy(&data.front(), data.size());
  }
  return NULL;
}

bool DecodeBitmap(const void* buffer, size_t size, SkBitmap* bm) {
  const unsigned char* data = static_cast<const unsigned char *>(buffer);

  // Try PNG first.
  if (gfx::PNGCodec::Decode(data, size, bm))
    return true;

  // Try JPEG.
  scoped_ptr<SkBitmap> decoded_jpeg(gfx::JPEGCodec::Decode(data, size));
  if (decoded_jpeg) {
    *bm = *decoded_jpeg;
    return true;
  }
  return false;
}

}  // namespace

scoped_refptr<Picture> Picture::Create(
    const gfx::Rect& layer_rect,
    ContentLayerClient* client,
    const SkTileGridFactory::TileGridInfo& tile_grid_info,
    bool gather_pixel_refs,
    int num_raster_threads,
    RecordingMode recording_mode) {
  scoped_refptr<Picture> picture = make_scoped_refptr(new Picture(layer_rect));

  picture->Record(client, tile_grid_info, recording_mode);
  if (gather_pixel_refs)
    picture->GatherPixelRefs(tile_grid_info);
  picture->CloneForDrawing(num_raster_threads);

  return picture;
}

Picture::Picture(const gfx::Rect& layer_rect)
  : layer_rect_(layer_rect),
    cell_size_(layer_rect.size()) {
  // Instead of recording a trace event for object creation here, we wait for
  // the picture to be recorded in Picture::Record.
}

scoped_refptr<Picture> Picture::CreateFromSkpValue(const base::Value* value) {
  // Decode the picture from base64.
  std::string encoded;
  if (!value->GetAsString(&encoded))
    return NULL;

  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  SkMemoryStream stream(decoded.data(), decoded.size());

  // Read the picture. This creates an empty picture on failure.
  SkPicture* skpicture = SkPicture::CreateFromStream(&stream, &DecodeBitmap);
  if (skpicture == NULL)
    return NULL;

  gfx::Rect layer_rect(skpicture->width(), skpicture->height());
  gfx::Rect opaque_rect(skpicture->width(), skpicture->height());

  return make_scoped_refptr(new Picture(skpicture, layer_rect, opaque_rect));
}

scoped_refptr<Picture> Picture::CreateFromValue(const base::Value* raw_value) {
  const base::DictionaryValue* value = NULL;
  if (!raw_value->GetAsDictionary(&value))
    return NULL;

  // Decode the picture from base64.
  std::string encoded;
  if (!value->GetString("skp64", &encoded))
    return NULL;

  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  SkMemoryStream stream(decoded.data(), decoded.size());

  const base::Value* layer_rect_value = NULL;
  if (!value->Get("params.layer_rect", &layer_rect_value))
    return NULL;

  gfx::Rect layer_rect;
  if (!MathUtil::FromValue(layer_rect_value, &layer_rect))
    return NULL;

  const base::Value* opaque_rect_value = NULL;
  if (!value->Get("params.opaque_rect", &opaque_rect_value))
    return NULL;

  gfx::Rect opaque_rect;
  if (!MathUtil::FromValue(opaque_rect_value, &opaque_rect))
    return NULL;

  // Read the picture. This creates an empty picture on failure.
  SkPicture* skpicture = SkPicture::CreateFromStream(&stream, &DecodeBitmap);
  if (skpicture == NULL)
    return NULL;

  return make_scoped_refptr(new Picture(skpicture, layer_rect, opaque_rect));
}

Picture::Picture(SkPicture* picture,
                 const gfx::Rect& layer_rect,
                 const gfx::Rect& opaque_rect) :
    layer_rect_(layer_rect),
    opaque_rect_(opaque_rect),
    picture_(skia::AdoptRef(picture)),
    cell_size_(layer_rect.size()) {
}

Picture::Picture(const skia::RefPtr<SkPicture>& picture,
                 const gfx::Rect& layer_rect,
                 const gfx::Rect& opaque_rect,
                 const PixelRefMap& pixel_refs) :
    layer_rect_(layer_rect),
    opaque_rect_(opaque_rect),
    picture_(picture),
    pixel_refs_(pixel_refs),
    cell_size_(layer_rect.size()) {
}

Picture::~Picture() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
    TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::Picture", this);
}

Picture* Picture::GetCloneForDrawingOnThread(unsigned thread_index) {
  // We don't need clones to draw from multiple threads with SkRecord.
  if (playback_) {
    return this;
  }

  // SkPicture is not thread-safe to rasterize with, this returns a clone
  // to rasterize with on a specific thread.
  CHECK_GE(clones_.size(), thread_index);
  return thread_index == clones_.size() ? this : clones_[thread_index].get();
}

bool Picture::IsSuitableForGpuRasterization() const {
  DCHECK(picture_);

  // TODO(alokp): SkPicture::suitableForGpuRasterization needs a GrContext.
  // Ideally this GrContext should be the same as that for rasterizing this
  // picture. But we are on the main thread while the rasterization context
  // may be on the compositor or raster thread.
  // SkPicture::suitableForGpuRasterization is not implemented yet.
  // Pass a NULL context for now and discuss with skia folks if the context
  // is really needed.
  return picture_->suitableForGpuRasterization(NULL);
}

void Picture::CloneForDrawing(int num_threads) {
  TRACE_EVENT1("cc", "Picture::CloneForDrawing", "num_threads", num_threads);

  // We don't need clones to draw from multiple threads with SkRecord.
  if (playback_) {
    return;
  }

  DCHECK(picture_);
  DCHECK(clones_.empty());

  // We can re-use this picture for one raster worker thread.
  raster_thread_checker_.DetachFromThread();

  if (num_threads > 1) {
    scoped_ptr<SkPicture[]> clones(new SkPicture[num_threads - 1]);
    picture_->clone(&clones[0], num_threads - 1);

    for (int i = 0; i < num_threads - 1; i++) {
      scoped_refptr<Picture> clone = make_scoped_refptr(
          new Picture(skia::AdoptRef(new SkPicture(clones[i])),
                      layer_rect_,
                      opaque_rect_,
                      pixel_refs_));
      clones_.push_back(clone);

      clone->EmitTraceSnapshotAlias(this);
      clone->raster_thread_checker_.DetachFromThread();
    }
  }
}

void Picture::Record(ContentLayerClient* painter,
                     const SkTileGridFactory::TileGridInfo& tile_grid_info,
                     RecordingMode recording_mode) {
  TRACE_EVENT2("cc",
               "Picture::Record",
               "data",
               AsTraceableRecordData(),
               "recording_mode",
               recording_mode);

  DCHECK(!picture_);
  DCHECK(!tile_grid_info.fTileInterval.isEmpty());

  SkTileGridFactory factory(tile_grid_info);
  SkPictureRecorder recorder;

  scoped_ptr<EXPERIMENTAL::SkRecording> recording;

  skia::RefPtr<SkCanvas> canvas;
  canvas = skia::SharePtr(recorder.beginRecording(
      layer_rect_.width(), layer_rect_.height(), &factory));

  ContentLayerClient::GraphicsContextStatus graphics_context_status =
      ContentLayerClient::GRAPHICS_CONTEXT_ENABLED;

  switch (recording_mode) {
    case RECORD_NORMALLY:
      // Already setup for normal recording.
      break;
    case RECORD_WITH_SK_NULL_CANVAS:
      canvas = skia::AdoptRef(SkCreateNullCanvas());
      break;
    case RECORD_WITH_PAINTING_DISABLED:
      // We pass a disable flag through the paint calls when perfromance
      // testing (the only time this case should ever arise) when we want to
      // prevent the Blink GraphicsContext object from consuming any compute
      // time.
      canvas = skia::AdoptRef(SkCreateNullCanvas());
      graphics_context_status = ContentLayerClient::GRAPHICS_CONTEXT_DISABLED;
      break;
    case RECORD_WITH_SKRECORD:
      recording.reset(new EXPERIMENTAL::SkRecording(layer_rect_.width(),
                                                    layer_rect_.height()));
      canvas = skia::SharePtr(recording->canvas());
      break;
    default:
      NOTREACHED();
  }

  canvas->save();
  canvas->translate(SkFloatToScalar(-layer_rect_.x()),
                    SkFloatToScalar(-layer_rect_.y()));

  SkRect layer_skrect = SkRect::MakeXYWH(layer_rect_.x(),
                                         layer_rect_.y(),
                                         layer_rect_.width(),
                                         layer_rect_.height());
  canvas->clipRect(layer_skrect);

  gfx::RectF opaque_layer_rect;
  painter->PaintContents(
      canvas.get(), layer_rect_, &opaque_layer_rect, graphics_context_status);

  canvas->restore();
  picture_ = skia::AdoptRef(recorder.endRecording());
  DCHECK(picture_);

  if (recording) {
    // SkRecording requires it's the only one holding onto canvas before we
    // may call releasePlayback().  (This helps enforce thread-safety.)
    canvas.clear();
    playback_.reset(recording->releasePlayback());
  }

  opaque_rect_ = gfx::ToEnclosedRect(opaque_layer_rect);

  EmitTraceSnapshot();
}

void Picture::GatherPixelRefs(
    const SkTileGridFactory::TileGridInfo& tile_grid_info) {
  TRACE_EVENT2("cc", "Picture::GatherPixelRefs",
               "width", layer_rect_.width(),
               "height", layer_rect_.height());

  DCHECK(picture_);
  DCHECK(pixel_refs_.empty());
  if (!WillPlayBackBitmaps())
    return;
  cell_size_ = gfx::Size(
      tile_grid_info.fTileInterval.width() +
          2 * tile_grid_info.fMargin.width(),
      tile_grid_info.fTileInterval.height() +
          2 * tile_grid_info.fMargin.height());
  DCHECK_GT(cell_size_.width(), 0);
  DCHECK_GT(cell_size_.height(), 0);

  int min_x = std::numeric_limits<int>::max();
  int min_y = std::numeric_limits<int>::max();
  int max_x = 0;
  int max_y = 0;

  skia::DiscardablePixelRefList pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture_.get(), &pixel_refs);
  for (skia::DiscardablePixelRefList::const_iterator it = pixel_refs.begin();
       it != pixel_refs.end();
       ++it) {
    gfx::Point min(
        RoundDown(static_cast<int>(it->pixel_ref_rect.x()),
                  cell_size_.width()),
        RoundDown(static_cast<int>(it->pixel_ref_rect.y()),
                  cell_size_.height()));
    gfx::Point max(
        RoundDown(static_cast<int>(std::ceil(it->pixel_ref_rect.right())),
                  cell_size_.width()),
        RoundDown(static_cast<int>(std::ceil(it->pixel_ref_rect.bottom())),
                  cell_size_.height()));

    for (int y = min.y(); y <= max.y(); y += cell_size_.height()) {
      for (int x = min.x(); x <= max.x(); x += cell_size_.width()) {
        PixelRefMapKey key(x, y);
        pixel_refs_[key].push_back(it->pixel_ref);
      }
    }

    min_x = std::min(min_x, min.x());
    min_y = std::min(min_y, min.y());
    max_x = std::max(max_x, max.x());
    max_y = std::max(max_y, max.y());
  }

  min_pixel_cell_ = gfx::Point(min_x, min_y);
  max_pixel_cell_ = gfx::Point(max_x, max_y);
}

int Picture::Raster(
    SkCanvas* canvas,
    SkDrawPictureCallback* callback,
    const Region& negated_content_region,
    float contents_scale) {
  if (!playback_)
    DCHECK(raster_thread_checker_.CalledOnValidThread());
  TRACE_EVENT_BEGIN1(
      "cc",
      "Picture::Raster",
      "data",
      AsTraceableRasterData(contents_scale));

  DCHECK(picture_);

  canvas->save();

  for (Region::Iterator it(negated_content_region); it.has_rect(); it.next())
    canvas->clipRect(gfx::RectToSkRect(it.rect()), SkRegion::kDifference_Op);

  canvas->scale(contents_scale, contents_scale);
  canvas->translate(layer_rect_.x(), layer_rect_.y());
  if (playback_) {
    playback_->draw(canvas);
  } else {
    picture_->draw(canvas, callback);
  }
  SkIRect bounds;
  canvas->getClipDeviceBounds(&bounds);
  canvas->restore();
  TRACE_EVENT_END1(
      "cc", "Picture::Raster",
      "num_pixels_rasterized", bounds.width() * bounds.height());
  return bounds.width() * bounds.height();
}

void Picture::Replay(SkCanvas* canvas) {
  if (!playback_)
    DCHECK(raster_thread_checker_.CalledOnValidThread());
  TRACE_EVENT_BEGIN0("cc", "Picture::Replay");
  DCHECK(picture_);

  if (playback_) {
    playback_->draw(canvas);
  } else {
    picture_->draw(canvas);
  }
  SkIRect bounds;
  canvas->getClipDeviceBounds(&bounds);
  TRACE_EVENT_END1("cc", "Picture::Replay",
                   "num_pixels_replayed", bounds.width() * bounds.height());
}

scoped_ptr<base::Value> Picture::AsValue() const {
  SkDynamicMemoryWStream stream;

  if (playback_) {
    // SkPlayback can't serialize itself, so re-record into an SkPicture.
    SkPictureRecorder recorder;
    skia::RefPtr<SkCanvas> canvas(skia::SharePtr(recorder.beginRecording(
        layer_rect_.width(),
        layer_rect_.height(),
        NULL)));  // Default (no) bounding-box hierarchy is fastest.
    playback_->draw(canvas.get());
    skia::RefPtr<SkPicture> picture(skia::AdoptRef(recorder.endRecording()));
    picture->serialize(&stream, &EncodeBitmap);
  } else {
    // Serialize the picture.
    picture_->serialize(&stream, &EncodeBitmap);
  }

  // Encode the picture as base64.
  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  res->Set("params.layer_rect", MathUtil::AsValue(layer_rect_).release());
  res->Set("params.opaque_rect", MathUtil::AsValue(opaque_rect_).release());

  size_t serialized_size = stream.bytesWritten();
  scoped_ptr<char[]> serialized_picture(new char[serialized_size]);
  stream.copyTo(serialized_picture.get());
  std::string b64_picture;
  base::Base64Encode(std::string(serialized_picture.get(), serialized_size),
                     &b64_picture);
  res->SetString("skp64", b64_picture);
  return res.PassAs<base::Value>();
}

void Picture::EmitTraceSnapshot() const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug") "," TRACE_DISABLED_BY_DEFAULT(
          "devtools.timeline.picture"),
      "cc::Picture",
      this,
      TracedPicture::AsTraceablePicture(this));
}

void Picture::EmitTraceSnapshotAlias(Picture* original) const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug") "," TRACE_DISABLED_BY_DEFAULT(
          "devtools.timeline.picture"),
      "cc::Picture",
      this,
      TracedPicture::AsTraceablePictureAlias(original));
}

base::LazyInstance<Picture::PixelRefs>
    Picture::PixelRefIterator::empty_pixel_refs_;

Picture::PixelRefIterator::PixelRefIterator()
    : picture_(NULL),
      current_pixel_refs_(empty_pixel_refs_.Pointer()),
      current_index_(0),
      min_point_(-1, -1),
      max_point_(-1, -1),
      current_x_(0),
      current_y_(0) {
}

Picture::PixelRefIterator::PixelRefIterator(
    const gfx::Rect& rect,
    const Picture* picture)
    : picture_(picture),
      current_pixel_refs_(empty_pixel_refs_.Pointer()),
      current_index_(0) {
  gfx::Rect layer_rect = picture->layer_rect_;
  gfx::Size cell_size = picture->cell_size_;
  DCHECK(!cell_size.IsEmpty());

  gfx::Rect query_rect(rect);
  // Early out if the query rect doesn't intersect this picture.
  if (!query_rect.Intersects(layer_rect)) {
    min_point_ = gfx::Point(0, 0);
    max_point_ = gfx::Point(0, 0);
    current_x_ = 1;
    current_y_ = 1;
    return;
  }

  // First, subtract the layer origin as cells are stored in layer space.
  query_rect.Offset(-layer_rect.OffsetFromOrigin());

  // We have to find a cell_size aligned point that corresponds to
  // query_rect. Point is a multiple of cell_size.
  min_point_ = gfx::Point(
      RoundDown(query_rect.x(), cell_size.width()),
      RoundDown(query_rect.y(), cell_size.height()));
  max_point_ = gfx::Point(
      RoundDown(query_rect.right() - 1, cell_size.width()),
      RoundDown(query_rect.bottom() - 1, cell_size.height()));

  // Limit the points to known pixel ref boundaries.
  min_point_ = gfx::Point(
      std::max(min_point_.x(), picture->min_pixel_cell_.x()),
      std::max(min_point_.y(), picture->min_pixel_cell_.y()));
  max_point_ = gfx::Point(
      std::min(max_point_.x(), picture->max_pixel_cell_.x()),
      std::min(max_point_.y(), picture->max_pixel_cell_.y()));

  // Make the current x be cell_size.width() less than min point, so that
  // the first increment will point at min_point_.
  current_x_ = min_point_.x() - cell_size.width();
  current_y_ = min_point_.y();
  if (current_y_ <= max_point_.y())
    ++(*this);
}

Picture::PixelRefIterator::~PixelRefIterator() {
}

Picture::PixelRefIterator& Picture::PixelRefIterator::operator++() {
  ++current_index_;
  // If we're not at the end of the list, then we have the next item.
  if (current_index_ < current_pixel_refs_->size())
    return *this;

  DCHECK(current_y_ <= max_point_.y());
  while (true) {
    gfx::Size cell_size = picture_->cell_size_;

    // Advance the current grid cell.
    current_x_ += cell_size.width();
    if (current_x_ > max_point_.x()) {
      current_y_ += cell_size.height();
      current_x_ = min_point_.x();
      if (current_y_ > max_point_.y()) {
        current_pixel_refs_ = empty_pixel_refs_.Pointer();
        current_index_ = 0;
        break;
      }
    }

    // If there are no pixel refs at this grid cell, keep incrementing.
    PixelRefMapKey key(current_x_, current_y_);
    PixelRefMap::const_iterator iter = picture_->pixel_refs_.find(key);
    if (iter == picture_->pixel_refs_.end())
      continue;

    // We found a non-empty list: store it and get the first pixel ref.
    current_pixel_refs_ = &iter->second;
    current_index_ = 0;
    break;
  }
  return *this;
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
    Picture::AsTraceableRasterData(float scale) const {
  scoped_ptr<base::DictionaryValue> raster_data(new base::DictionaryValue());
  raster_data->Set("picture_id", TracedValue::CreateIDRef(this).release());
  raster_data->SetDouble("scale", scale);
  return TracedValue::FromValue(raster_data.release());
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
    Picture::AsTraceableRecordData() const {
  scoped_ptr<base::DictionaryValue> record_data(new base::DictionaryValue());
  record_data->Set("picture_id", TracedValue::CreateIDRef(this).release());
  record_data->Set("layer_rect", MathUtil::AsValue(layer_rect_).release());
  return TracedValue::FromValue(record_data.release());
}

}  // namespace cc
