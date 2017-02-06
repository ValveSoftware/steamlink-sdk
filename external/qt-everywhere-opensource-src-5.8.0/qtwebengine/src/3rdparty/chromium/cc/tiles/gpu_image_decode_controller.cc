// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/gpu_image_decode_controller.h"

#include <inttypes.h>

#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/output/context_provider.h"
#include "cc/raster/tile_task.h"
#include "cc/resources/resource_format_utils.h"
#include "cc/tiles/mipmap_util.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu_image_decode_controller.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/trace_util.h"

namespace cc {
namespace {

static const int kMaxDiscardableItems = 2000;

// Returns true if an image would not be drawn and should therefore be
// skipped rather than decoded.
bool SkipImage(const DrawImage& draw_image) {
  if (!SkIRect::Intersects(draw_image.src_rect(), draw_image.image()->bounds()))
    return true;
  if (std::abs(draw_image.scale().width()) <
          std::numeric_limits<float>::epsilon() ||
      std::abs(draw_image.scale().height()) <
          std::numeric_limits<float>::epsilon()) {
    return true;
  }
  return false;
}

// Returns the filter quality to use for scaling the image to upload scale. For
// GPU raster, medium and high filter quality are identical for downscales.
// Upload scaling is always a downscale, so cap our filter quality to medium.
SkFilterQuality CalculateUploadScaleFilterQuality(const DrawImage& draw_image) {
  return std::min(kMedium_SkFilterQuality, draw_image.filter_quality());
}

SkImage::DeferredTextureImageUsageParams ParamsFromDrawImage(
    const DrawImage& draw_image,
    int upload_scale_mip_level) {
  SkImage::DeferredTextureImageUsageParams params;
  params.fMatrix = draw_image.matrix();
  params.fQuality = draw_image.filter_quality();
  params.fPreScaleMipLevel = upload_scale_mip_level;

  return params;
}

// Calculate the mip level to upload-scale the image to before uploading. We use
// mip levels rather than exact scales to increase re-use of scaled images.
int CalculateUploadScaleMipLevel(const DrawImage& draw_image) {
  // Images which are being clipped will have color-bleeding if scaled.
  // TODO(ericrk): Investigate uploading clipped images to handle this case and
  // provide further optimization. crbug.com/620899
  if (draw_image.src_rect() != draw_image.image()->bounds())
    return 0;

  gfx::Size base_size(draw_image.image()->width(),
                      draw_image.image()->height());
  // Ceil our scaled size so that the mip map generated is guaranteed to be
  // larger. Take the abs of the scale, as mipmap functions don't handle
  // (and aren't impacted by) negative image dimensions.
  gfx::Size scaled_size =
      gfx::ScaleToCeiledSize(base_size, std::abs(draw_image.scale().width()),
                             std::abs(draw_image.scale().height()));

  return MipMapUtil::GetLevelForSize(base_size, scaled_size);
}

// Calculates the scale factor which can be used to scale an image to a given
// mip level.
SkSize CalculateScaleFactorForMipLevel(const DrawImage& draw_image,
                                       int mip_level) {
  gfx::Size base_size(draw_image.image()->width(),
                      draw_image.image()->height());
  return MipMapUtil::GetScaleAdjustmentForLevel(base_size, mip_level);
}

// Calculates the size of a given mip level.
gfx::Size CalculateSizeForMipLevel(const DrawImage& draw_image, int mip_level) {
  gfx::Size base_size(draw_image.image()->width(),
                      draw_image.image()->height());
  return MipMapUtil::GetSizeForLevel(base_size, mip_level);
}

// Generates a uint64_t which uniquely identifies a DrawImage for the purposes
// of the |in_use_cache_|. The key is generated as follows:
//   ╔══════════════════════╤═══════════╤═══════════╗
//   ║       image_id       │ mip_level │  quality  ║
//   ╚════════32═bits═══════╧══16═bits══╧══16═bits══╝
uint64_t GenerateInUseCacheKey(const DrawImage& draw_image) {
  static_assert(
      kLast_SkFilterQuality <= std::numeric_limits<uint16_t>::max(),
      "InUseCacheKey depends on SkFilterQuality fitting in a uint16_t.");

  SkFilterQuality filter_quality =
      CalculateUploadScaleFilterQuality(draw_image);
  DCHECK_LE(filter_quality, kLast_SkFilterQuality);

  // An image has at most log_2(max(width, height)) mip levels, so given our
  // usage of 32-bit sizes for images, key.mip_level is at most 31.
  int32_t mip_level = CalculateUploadScaleMipLevel(draw_image);
  DCHECK_LT(mip_level, 32);

  return (static_cast<uint64_t>(draw_image.image()->uniqueID()) << 32) |
         (mip_level << 16) | filter_quality;
}

}  // namespace

GpuImageDecodeController::InUseCacheEntry::InUseCacheEntry(
    scoped_refptr<ImageData> image_data)
    : image_data(std::move(image_data)) {}
GpuImageDecodeController::InUseCacheEntry::InUseCacheEntry(
    const InUseCacheEntry&) = default;
GpuImageDecodeController::InUseCacheEntry::InUseCacheEntry(InUseCacheEntry&&) =
    default;
GpuImageDecodeController::InUseCacheEntry::~InUseCacheEntry() = default;

// Task which decodes an image and stores the result in discardable memory.
// This task does not use GPU resources and can be run on any thread.
class ImageDecodeTaskImpl : public TileTask {
 public:
  ImageDecodeTaskImpl(GpuImageDecodeController* controller,
                      const DrawImage& draw_image,
                      const ImageDecodeController::TracingInfo& tracing_info)
      : TileTask(true),
        controller_(controller),
        image_(draw_image),
        tracing_info_(tracing_info) {
    DCHECK(!SkipImage(draw_image));
  }

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageDecodeTaskImpl::RunOnWorkerThread", "mode", "gpu",
                 "source_prepare_tiles_id", tracing_info_.prepare_tiles_id);
    controller_->DecodeImage(image_);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    controller_->OnImageDecodeTaskCompleted(image_);
  }

 protected:
  ~ImageDecodeTaskImpl() override {}

 private:
  GpuImageDecodeController* controller_;
  DrawImage image_;
  const ImageDecodeController::TracingInfo tracing_info_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeTaskImpl);
};

// Task which creates an image from decoded data. Typically this involves
// uploading data to the GPU, which requires this task be run on the non-
// concurrent thread.
class ImageUploadTaskImpl : public TileTask {
 public:
  ImageUploadTaskImpl(GpuImageDecodeController* controller,
                      const DrawImage& draw_image,
                      scoped_refptr<TileTask> decode_dependency,
                      const ImageDecodeController::TracingInfo& tracing_info)
      : TileTask(false),
        controller_(controller),
        image_(draw_image),
        tracing_info_(tracing_info) {
    DCHECK(!SkipImage(draw_image));
    // If an image is already decoded and locked, we will not generate a
    // decode task.
    if (decode_dependency)
      dependencies_.push_back(std::move(decode_dependency));
  }

  // Override from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageUploadTaskImpl::RunOnWorkerThread", "mode", "gpu",
                 "source_prepare_tiles_id", tracing_info_.prepare_tiles_id);
    controller_->UploadImage(image_);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    controller_->OnImageUploadTaskCompleted(image_);
  }

 protected:
  ~ImageUploadTaskImpl() override {}

 private:
  GpuImageDecodeController* controller_;
  DrawImage image_;
  const ImageDecodeController::TracingInfo tracing_info_;

  DISALLOW_COPY_AND_ASSIGN(ImageUploadTaskImpl);
};

GpuImageDecodeController::DecodedImageData::DecodedImageData() = default;
GpuImageDecodeController::DecodedImageData::~DecodedImageData() {
  ResetData();
}

bool GpuImageDecodeController::DecodedImageData::Lock() {
  DCHECK(!is_locked_);
  is_locked_ = data_->Lock();
  if (is_locked_)
    ++usage_stats_.lock_count;
  return is_locked_;
}

void GpuImageDecodeController::DecodedImageData::Unlock() {
  DCHECK(is_locked_);
  data_->Unlock();
  if (usage_stats_.lock_count == 1)
    usage_stats_.first_lock_wasted = !usage_stats_.used;
  is_locked_ = false;
}

void GpuImageDecodeController::DecodedImageData::SetLockedData(
    std::unique_ptr<base::DiscardableMemory> data) {
  DCHECK(!is_locked_);
  DCHECK(data);
  DCHECK(!data_);
  data_ = std::move(data);
  is_locked_ = true;
}

void GpuImageDecodeController::DecodedImageData::ResetData() {
  DCHECK(!is_locked_);
  if (data_)
    ReportUsageStats();
  data_ = nullptr;
  usage_stats_ = UsageStats();
}

void GpuImageDecodeController::DecodedImageData::ReportUsageStats() const {
  // lock_count │ used  │ result state
  // ═══════════╪═══════╪══════════════════
  //  1         │ false │ WASTED_ONCE
  //  1         │ true  │ USED_ONCE
  //  >1        │ false │ WASTED_RELOCKED
  //  >1        │ true  │ USED_RELOCKED
  // Note that it's important not to reorder the following enums, since the
  // numerical values are used in the histogram code.
  enum State : int {
    DECODED_IMAGE_STATE_WASTED_ONCE,
    DECODED_IMAGE_STATE_USED_ONCE,
    DECODED_IMAGE_STATE_WASTED_RELOCKED,
    DECODED_IMAGE_STATE_USED_RELOCKED,
    DECODED_IMAGE_STATE_COUNT
  } state = DECODED_IMAGE_STATE_WASTED_ONCE;

  if (usage_stats_.lock_count == 1) {
    if (usage_stats_.used)
      state = DECODED_IMAGE_STATE_USED_ONCE;
    else
      state = DECODED_IMAGE_STATE_WASTED_ONCE;
  } else {
    if (usage_stats_.used)
      state = DECODED_IMAGE_STATE_USED_RELOCKED;
    else
      state = DECODED_IMAGE_STATE_WASTED_RELOCKED;
  }

  UMA_HISTOGRAM_ENUMERATION("Renderer4.GpuImageDecodeState", state,
                            DECODED_IMAGE_STATE_COUNT);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuImageDecodeState.FirstLockWasted",
                        usage_stats_.first_lock_wasted);
}

GpuImageDecodeController::UploadedImageData::UploadedImageData() = default;
GpuImageDecodeController::UploadedImageData::~UploadedImageData() {
  SetImage(nullptr);
}

void GpuImageDecodeController::UploadedImageData::SetImage(
    sk_sp<SkImage> image) {
  DCHECK(!image_ || !image);
  if (image_) {
    ReportUsageStats();
    usage_stats_ = UsageStats();
  }
  image_ = std::move(image);
}

void GpuImageDecodeController::UploadedImageData::ReportUsageStats() const {
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuImageUploadState.Used",
                        usage_stats_.used);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuImageUploadState.FirstRefWasted",
                        usage_stats_.first_ref_wasted);
}

GpuImageDecodeController::ImageData::ImageData(
    DecodedDataMode mode,
    size_t size,
    int upload_scale_mip_level,
    SkFilterQuality upload_scale_filter_quality)
    : mode(mode),
      size(size),
      upload_scale_mip_level(upload_scale_mip_level),
      upload_scale_filter_quality(upload_scale_filter_quality) {}

GpuImageDecodeController::ImageData::~ImageData() {
  // We should never delete ImageData while it is in use or before it has been
  // cleaned up.
  DCHECK_EQ(0u, upload.ref_count);
  DCHECK_EQ(0u, decode.ref_count);
  DCHECK_EQ(false, decode.is_locked());
  // This should always be cleaned up before deleting the image, as it needs to
  // be freed with the GL context lock held.
  DCHECK(!upload.image());
}

GpuImageDecodeController::GpuImageDecodeController(ContextProvider* context,
                                                   ResourceFormat decode_format,
                                                   size_t max_gpu_image_bytes)
    : format_(decode_format),
      context_(context),
      persistent_cache_(PersistentCache::NO_AUTO_EVICT),
      cached_items_limit_(kMaxDiscardableItems),
      cached_bytes_limit_(max_gpu_image_bytes),
      bytes_used_(0),
      max_gpu_image_bytes_(max_gpu_image_bytes) {
  // Acquire the context_lock so that we can safely retrieve the
  // GrContextThreadSafeProxy. This proxy can then be used with no lock held.
  {
    ContextProvider::ScopedContextLock context_lock(context_);
    context_threadsafe_proxy_ = sk_sp<GrContextThreadSafeProxy>(
        context->GrContext()->threadSafeProxy());
  }

  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::GpuImageDecodeController",
        base::ThreadTaskRunnerHandle::Get());
  }
}

GpuImageDecodeController::~GpuImageDecodeController() {
  // SetShouldAggressivelyFreeResources will zero our limits and free all
  // outstanding image memory.
  SetShouldAggressivelyFreeResources(true);

  // It is safe to unregister, even if we didn't register in the constructor.
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool GpuImageDecodeController::GetTaskForImageAndRef(
    const DrawImage& draw_image,
    const TracingInfo& tracing_info,
    scoped_refptr<TileTask>* task) {
  if (SkipImage(draw_image)) {
    *task = nullptr;
    return false;
  }

  base::AutoLock lock(lock_);
  const auto image_id = draw_image.image()->uniqueID();
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  scoped_refptr<ImageData> new_data;
  if (!image_data) {
    // We need an ImageData, create one now.
    new_data = CreateImageData(draw_image);
    image_data = new_data.get();
  } else if (image_data->is_at_raster) {
    // Image is at-raster, just return, this usage will be at-raster as well.
    *task = nullptr;
    return false;
  } else if (image_data->decode.decode_failure) {
    // We have already tried and failed to decode this image, so just return.
    *task = nullptr;
    return false;
  } else if (image_data->upload.image()) {
    // The image is already uploaded, ref and return.
    RefImage(draw_image);
    *task = nullptr;
    return true;
  } else if (image_data->upload.task) {
    // We had an existing upload task, ref the image and return the task.
    RefImage(draw_image);
    *task = image_data->upload.task;
    return true;
  }

  // Ensure that the image we're about to decode/upload will fit in memory.
  if (!EnsureCapacity(image_data->size)) {
    // Image will not fit, do an at-raster decode.
    *task = nullptr;
    return false;
  }

  // If we had to create new image data, add it to our map now that we know it
  // will fit.
  if (new_data)
    persistent_cache_.Put(image_id, std::move(new_data));

  // Ref image and create a upload and decode tasks. We will release this ref
  // in UploadTaskCompleted.
  RefImage(draw_image);
  *task = make_scoped_refptr(new ImageUploadTaskImpl(
      this, draw_image, GetImageDecodeTaskAndRef(draw_image, tracing_info),
      tracing_info));
  image_data->upload.task = *task;

  // Ref the image again - this ref is owned by the caller, and it is their
  // responsibility to release it by calling UnrefImage.
  RefImage(draw_image);
  return true;
}

void GpuImageDecodeController::UnrefImage(const DrawImage& draw_image) {
  base::AutoLock lock(lock_);
  UnrefImageInternal(draw_image);
}

DecodedDrawImage GpuImageDecodeController::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  TRACE_EVENT0("cc", "GpuImageDecodeController::GetDecodedImageForDraw");

  // We are being called during raster. The context lock must already be
  // acquired by the caller.
  context_->GetLock()->AssertAcquired();

  if (SkipImage(draw_image))
    return DecodedDrawImage(nullptr, draw_image.filter_quality());

  base::AutoLock lock(lock_);
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  if (!image_data) {
    // We didn't find the image, create a new entry.
    auto data = CreateImageData(draw_image);
    image_data = data.get();
    persistent_cache_.Put(draw_image.image()->uniqueID(), std::move(data));
  }

  if (!image_data->upload.budgeted) {
    // If image data is not budgeted by this point, it is at-raster.
    image_data->is_at_raster = true;
  }

  // Ref the image and decode so that they stay alive while we are
  // decoding/uploading.
  RefImage(draw_image);
  RefImageDecode(draw_image);

  // We may or may not need to decode and upload the image we've found, the
  // following functions early-out to if we already decoded.
  DecodeImageIfNecessary(draw_image, image_data);
  UploadImageIfNecessary(draw_image, image_data);
  // Unref the image decode, but not the image. The image ref will be released
  // in DrawWithImageFinished.
  UnrefImageDecode(draw_image);

  sk_sp<SkImage> image = image_data->upload.image();
  image_data->upload.mark_used();
  DCHECK(image || image_data->decode.decode_failure);

  SkSize scale_factor = CalculateScaleFactorForMipLevel(
      draw_image, image_data->upload_scale_mip_level);
  DecodedDrawImage decoded_draw_image(std::move(image), SkSize(), scale_factor,
                                      draw_image.filter_quality());
  decoded_draw_image.set_at_raster_decode(image_data->is_at_raster);
  return decoded_draw_image;
}

void GpuImageDecodeController::DrawWithImageFinished(
    const DrawImage& draw_image,
    const DecodedDrawImage& decoded_draw_image) {
  TRACE_EVENT0("cc", "GpuImageDecodeController::DrawWithImageFinished");

  // We are being called during raster. The context lock must already be
  // acquired by the caller.
  context_->GetLock()->AssertAcquired();

  if (SkipImage(draw_image))
    return;

  base::AutoLock lock(lock_);
  UnrefImageInternal(draw_image);

  // We are mid-draw and holding the context lock, ensure we clean up any
  // textures (especially at-raster), which may have just been marked for
  // deletion by UnrefImage.
  DeletePendingImages();
}

void GpuImageDecodeController::ReduceCacheUsage() {
  base::AutoLock lock(lock_);
  EnsureCapacity(0);
}

void GpuImageDecodeController::SetShouldAggressivelyFreeResources(
    bool aggressively_free_resources) {
  if (aggressively_free_resources) {
    ContextProvider::ScopedContextLock context_lock(context_);
    base::AutoLock lock(lock_);
    // We want to keep as little in our cache as possible. Set our memory limit
    // to zero and EnsureCapacity to clean up memory.
    cached_bytes_limit_ = 0;
    EnsureCapacity(0);

    // We are holding the context lock, so finish cleaning up deleted images
    // now.
    DeletePendingImages();
  } else {
    base::AutoLock lock(lock_);
    cached_bytes_limit_ = max_gpu_image_bytes_;
  }
}

bool GpuImageDecodeController::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  for (const auto& image_pair : persistent_cache_) {
    const ImageData* image_data = image_pair.second.get();
    const uint32_t image_id = image_pair.first;

    // If we have discardable decoded data, dump this here.
    if (image_data->decode.data()) {
      std::string discardable_dump_name = base::StringPrintf(
          "cc/image_memory/controller_0x%" PRIXPTR "/discardable/image_%d",
          reinterpret_cast<uintptr_t>(this), image_id);
      base::trace_event::MemoryAllocatorDump* dump =
          image_data->decode.data()->CreateMemoryAllocatorDump(
              discardable_dump_name.c_str(), pmd);
      // If our image is locked, dump the "locked_size" as an additional column.
      // This lets us see the amount of discardable which is contributing to
      // memory pressure.
      if (image_data->decode.is_locked()) {
        dump->AddScalar("locked_size",
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        image_data->size);
      }
    }

    // If we have an uploaded image (that is actually on the GPU, not just a CPU
    // wrapper), upload it here.
    if (image_data->upload.image() &&
        image_data->mode == DecodedDataMode::GPU) {
      std::string gpu_dump_name = base::StringPrintf(
          "cc/image_memory/controller_0x%" PRIXPTR "/gpu/image_%d",
          reinterpret_cast<uintptr_t>(this), image_id);
      base::trace_event::MemoryAllocatorDump* dump =
          pmd->CreateAllocatorDump(gpu_dump_name);
      dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      image_data->size);

      // Create a global shred GUID to associate this data with its GPU process
      // counterpart.
      GLuint gl_id = skia::GrBackendObjectToGrGLTextureInfo(
                         image_data->upload.image()->getTextureHandle(
                             false /* flushPendingGrContextIO */))
                         ->fID;
      base::trace_event::MemoryAllocatorDumpGuid guid =
          gl::GetGLTextureClientGUIDForTracing(
              context_->ContextSupport()->ShareGroupTracingGUID(), gl_id);

      // kImportance is somewhat arbitrary - we chose 3 to be higher than the
      // value used in the GPU process (1), and Skia (2), causing us to appear
      // as the owner in memory traces.
      const int kImportance = 3;
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }

  return true;
}

void GpuImageDecodeController::DecodeImage(const DrawImage& draw_image) {
  base::AutoLock lock(lock_);
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  DCHECK(image_data);
  DCHECK(!image_data->is_at_raster);
  DecodeImageIfNecessary(draw_image, image_data);
}

void GpuImageDecodeController::UploadImage(const DrawImage& draw_image) {
  ContextProvider::ScopedContextLock context_lock(context_);
  base::AutoLock lock(lock_);
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  DCHECK(image_data);
  DCHECK(!image_data->is_at_raster);
  UploadImageIfNecessary(draw_image, image_data);
}

void GpuImageDecodeController::OnImageDecodeTaskCompleted(
    const DrawImage& draw_image) {
  base::AutoLock lock(lock_);
  // Decode task is complete, remove our reference to it.
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  DCHECK(image_data);
  DCHECK(image_data->decode.task);
  image_data->decode.task = nullptr;

  // While the decode task is active, we keep a ref on the decoded data.
  // Release that ref now.
  UnrefImageDecode(draw_image);
}

void GpuImageDecodeController::OnImageUploadTaskCompleted(
    const DrawImage& draw_image) {
  base::AutoLock lock(lock_);
  // Upload task is complete, remove our reference to it.
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  DCHECK(image_data);
  DCHECK(image_data->upload.task);
  image_data->upload.task = nullptr;

  // While the upload task is active, we keep a ref on both the image it will be
  // populating, as well as the decode it needs to populate it. Release these
  // refs now.
  UnrefImageDecode(draw_image);
  UnrefImageInternal(draw_image);
}

// Checks if an existing image decode exists. If not, returns a task to produce
// the requested decode.
scoped_refptr<TileTask> GpuImageDecodeController::GetImageDecodeTaskAndRef(
    const DrawImage& draw_image,
    const TracingInfo& tracing_info) {
  lock_.AssertAcquired();

  // This ref is kept alive while an upload task may need this decode. We
  // release this ref in UploadTaskCompleted.
  RefImageDecode(draw_image);

  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  DCHECK(image_data);
  if (image_data->decode.is_locked()) {
    // We should never be creating a decode task for an at raster image.
    DCHECK(!image_data->is_at_raster);
    // We should never be creating a decode for an already-uploaded image.
    DCHECK(!image_data->upload.image());
    return nullptr;
  }

  // We didn't have an existing locked image, create a task to lock or decode.
  scoped_refptr<TileTask>& existing_task = image_data->decode.task;
  if (!existing_task) {
    // Ref image decode and create a decode task. This ref will be released in
    // DecodeTaskCompleted.
    RefImageDecode(draw_image);
    existing_task = make_scoped_refptr(
        new ImageDecodeTaskImpl(this, draw_image, tracing_info));
  }
  return existing_task;
}

void GpuImageDecodeController::RefImageDecode(const DrawImage& draw_image) {
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(GenerateInUseCacheKey(draw_image));
  DCHECK(found != in_use_cache_.end());
  ++found->second.ref_count;
  ++found->second.image_data->decode.ref_count;
  OwnershipChanged(found->second.image_data.get());
}

void GpuImageDecodeController::UnrefImageDecode(const DrawImage& draw_image) {
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(GenerateInUseCacheKey(draw_image));
  DCHECK(found != in_use_cache_.end());
  DCHECK_GT(found->second.image_data->decode.ref_count, 0u);
  DCHECK_GT(found->second.ref_count, 0u);
  --found->second.ref_count;
  --found->second.image_data->decode.ref_count;
  OwnershipChanged(found->second.image_data.get());
  if (found->second.ref_count == 0u) {
    in_use_cache_.erase(found);
  }
}

void GpuImageDecodeController::RefImage(const DrawImage& draw_image) {
  lock_.AssertAcquired();
  InUseCacheKey key = GenerateInUseCacheKey(draw_image);
  auto found = in_use_cache_.find(key);

  // If no secondary cache entry was found for the given |draw_image|, then
  // the draw_image only exists in the |persistent_cache_|. Create an in-use
  // cache entry now.
  if (found == in_use_cache_.end()) {
    auto found_image = persistent_cache_.Peek(draw_image.image()->uniqueID());
    DCHECK(found_image != persistent_cache_.end());
    DCHECK(found_image->second->upload_scale_mip_level <=
           CalculateUploadScaleMipLevel(draw_image));
    found = in_use_cache_
                .insert(InUseCache::value_type(
                    key, InUseCacheEntry(found_image->second)))
                .first;
  }

  DCHECK(found != in_use_cache_.end());
  ++found->second.ref_count;
  ++found->second.image_data->upload.ref_count;
  OwnershipChanged(found->second.image_data.get());
}

void GpuImageDecodeController::UnrefImageInternal(const DrawImage& draw_image) {
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(GenerateInUseCacheKey(draw_image));
  DCHECK(found != in_use_cache_.end());
  DCHECK_GT(found->second.image_data->upload.ref_count, 0u);
  DCHECK_GT(found->second.ref_count, 0u);
  --found->second.ref_count;
  --found->second.image_data->upload.ref_count;
  OwnershipChanged(found->second.image_data.get());
  if (found->second.ref_count == 0u) {
    in_use_cache_.erase(found);
  }
}

// Called any time an image or decode ref count changes. Takes care of any
// necessary memory budget book-keeping and cleanup.
void GpuImageDecodeController::OwnershipChanged(ImageData* image_data) {
  lock_.AssertAcquired();

  bool has_any_refs =
      image_data->upload.ref_count > 0 || image_data->decode.ref_count > 0;

  // Don't keep around orphaned images.
  if (image_data->is_orphaned && !has_any_refs) {
    images_pending_deletion_.push_back(std::move(image_data->upload.image()));
    image_data->upload.SetImage(nullptr);
  }

  // Don't keep CPU images if they are unused, these images can be recreated by
  // re-locking discardable (rather than requiring a full upload like GPU
  // images).
  if (image_data->mode == DecodedDataMode::CPU && !has_any_refs) {
    images_pending_deletion_.push_back(image_data->upload.image());
    image_data->upload.SetImage(nullptr);
  }

  if (image_data->is_at_raster && !has_any_refs) {
    // We have an at-raster image which has reached zero refs. If it won't fit
    // in our cache, delete the image to allow it to fit.
    if (image_data->upload.image() && !CanFitSize(image_data->size)) {
      images_pending_deletion_.push_back(image_data->upload.image());
      image_data->upload.SetImage(nullptr);
    }

    // We now have an at-raster image which will fit in our cache. Convert it
    // to not-at-raster.
    image_data->is_at_raster = false;
    if (image_data->upload.image()) {
      bytes_used_ += image_data->size;
      image_data->upload.budgeted = true;
    }
  }

  // If we have image refs on a non-at-raster image, it must be budgeted, as it
  // is either uploaded or pending upload.
  if (image_data->upload.ref_count > 0 && !image_data->upload.budgeted &&
      !image_data->is_at_raster) {
    // We should only be taking non-at-raster refs on images that fit in cache.
    DCHECK(CanFitSize(image_data->size));

    bytes_used_ += image_data->size;
    image_data->upload.budgeted = true;
  }

  // If we have no image refs on an image, it should only be budgeted if it has
  // an uploaded image. If no image exists (upload was cancelled), we should
  // un-budget the image.
  if (image_data->upload.ref_count == 0 && image_data->upload.budgeted &&
      !image_data->upload.image()) {
    DCHECK_GE(bytes_used_, image_data->size);
    bytes_used_ -= image_data->size;
    image_data->upload.budgeted = false;
  }

  // We should unlock the discardable memory for the image in two cases:
  // 1) The image is no longer being used (no decode or upload refs).
  // 2) This is a GPU backed image that has already been uploaded (no decode
  //    refs).
  bool should_unlock_discardable =
      !has_any_refs || (image_data->mode == DecodedDataMode::GPU &&
                        !image_data->decode.ref_count);

  if (should_unlock_discardable && image_data->decode.is_locked()) {
    DCHECK(image_data->decode.data());
    image_data->decode.Unlock();
  }

#if DCHECK_IS_ON()
  // Sanity check the above logic.
  if (image_data->upload.image()) {
    DCHECK(image_data->is_at_raster || image_data->upload.budgeted);
    if (image_data->mode == DecodedDataMode::CPU)
      DCHECK(image_data->decode.is_locked());
  } else {
    DCHECK(!image_data->upload.budgeted || image_data->upload.ref_count > 0);
  }
#endif
}

// Ensures that we can fit a new image of size |required_size| in our cache. In
// doing so, this function will free unreferenced image data as necessary to
// create rooom.
bool GpuImageDecodeController::EnsureCapacity(size_t required_size) {
  lock_.AssertAcquired();

  if (CanFitSize(required_size) && !ExceedsPreferredCount())
    return true;

  // While we are over memory or preferred item capacity, we iterate through
  // our set of cached image data in LRU order. For each image, we can do two
  // things: 1) We can free the uploaded image, reducing the memory usage of
  // the cache and 2) we can remove the entry entirely, reducing the count of
  // elements in the cache.
  for (auto it = persistent_cache_.rbegin(); it != persistent_cache_.rend();) {
    if (it->second->decode.ref_count != 0 ||
        it->second->upload.ref_count != 0) {
      ++it;
      continue;
    }

    // Current entry has no refs. Ensure it is not locked.
    DCHECK(!it->second->decode.is_locked());

    // If an image without refs is budgeted, it must have an associated image
    // upload.
    DCHECK(!it->second->upload.budgeted || it->second->upload.image());

    // Free the uploaded image if possible.
    if (it->second->upload.image()) {
      DCHECK(it->second->upload.budgeted);
      DCHECK_GE(bytes_used_, it->second->size);
      bytes_used_ -= it->second->size;
      images_pending_deletion_.push_back(it->second->upload.image());
      it->second->upload.SetImage(nullptr);
      it->second->upload.budgeted = false;
    }

    // Free the entire entry if necessary.
    if (ExceedsPreferredCount()) {
      it = persistent_cache_.Erase(it);
    } else {
      ++it;
    }

    if (CanFitSize(required_size) && !ExceedsPreferredCount())
      return true;
  }

  // Preferred count is only used as a guideline when triming the cache. Allow
  // new elements to be added as long as we are below our size limit.
  return CanFitSize(required_size);
}

bool GpuImageDecodeController::CanFitSize(size_t size) const {
  lock_.AssertAcquired();

  base::CheckedNumeric<uint32_t> new_size(bytes_used_);
  new_size += size;
  return new_size.IsValid() && new_size.ValueOrDie() <= cached_bytes_limit_;
}

bool GpuImageDecodeController::ExceedsPreferredCount() const {
  lock_.AssertAcquired();

  return persistent_cache_.size() > cached_items_limit_;
}

void GpuImageDecodeController::DecodeImageIfNecessary(
    const DrawImage& draw_image,
    ImageData* image_data) {
  lock_.AssertAcquired();

  DCHECK_GT(image_data->decode.ref_count, 0u);

  if (image_data->decode.decode_failure) {
    // We have already tried and failed to decode this image. Don't try again.
    return;
  }

  if (image_data->upload.image()) {
    // We already have an uploaded image, no reason to decode.
    return;
  }

  if (image_data->decode.data() &&
      (image_data->decode.is_locked() || image_data->decode.Lock())) {
    // We already decoded this, or we just needed to lock, early out.
    return;
  }

  TRACE_EVENT0("cc", "GpuImageDecodeController::DecodeImage");

  image_data->decode.ResetData();
  std::unique_ptr<base::DiscardableMemory> backing_memory;
  {
    base::AutoUnlock unlock(lock_);
    switch (image_data->mode) {
      case DecodedDataMode::CPU: {
        backing_memory =
            base::DiscardableMemoryAllocator::GetInstance()
                ->AllocateLockedDiscardableMemory(image_data->size);
        SkImageInfo image_info = CreateImageInfoForDrawImage(
            draw_image, image_data->upload_scale_mip_level);
        // In order to match GPU scaling quality (which uses mip-maps at high
        // quality), we want to use at most medium filter quality for the
        // scale.
        SkPixmap image_pixmap(image_info, backing_memory->data(),
                              image_info.minRowBytes());
        // Note that scalePixels falls back to readPixels if the sale is 1x, so
        // no need to special case that as an optimization.
        if (!draw_image.image()->scalePixels(
                image_pixmap, CalculateUploadScaleFilterQuality(draw_image),
                SkImage::kDisallow_CachingHint)) {
          backing_memory.reset();
        }
        break;
      }
      case DecodedDataMode::GPU: {
        backing_memory =
            base::DiscardableMemoryAllocator::GetInstance()
                ->AllocateLockedDiscardableMemory(image_data->size);
        auto params =
            ParamsFromDrawImage(draw_image, image_data->upload_scale_mip_level);
        if (!draw_image.image()->getDeferredTextureImageData(
                *context_threadsafe_proxy_.get(), &params, 1,
                backing_memory->data())) {
          backing_memory.reset();
        }
        break;
      }
    }
  }

  if (image_data->decode.data()) {
    // An at-raster task decoded this before us. Ingore our decode.
    return;
  }

  if (!backing_memory) {
    // If |backing_memory| was not populated, we had a non-decodable image.
    image_data->decode.decode_failure = true;
    return;
  }

  image_data->decode.SetLockedData(std::move(backing_memory));
}

void GpuImageDecodeController::UploadImageIfNecessary(
    const DrawImage& draw_image,
    ImageData* image_data) {
  context_->GetLock()->AssertAcquired();
  lock_.AssertAcquired();

  if (image_data->decode.decode_failure) {
    // We were unnable to decode this image. Don't try to upload.
    return;
  }

  if (image_data->upload.image()) {
    // Someone has uploaded this image before us (at raster).
    return;
  }

  TRACE_EVENT0("cc", "GpuImageDecodeController::UploadImage");
  DCHECK(image_data->decode.is_locked());
  DCHECK_GT(image_data->decode.ref_count, 0u);
  DCHECK_GT(image_data->upload.ref_count, 0u);

  // We are about to upload a new image and are holding the context lock.
  // Ensure that any images which have been marked for deletion are actually
  // cleaned up so we don't exceed our memory limit during this upload.
  DeletePendingImages();

  sk_sp<SkImage> uploaded_image;
  {
    base::AutoUnlock unlock(lock_);
    switch (image_data->mode) {
      case DecodedDataMode::CPU: {
        SkImageInfo image_info = CreateImageInfoForDrawImage(
            draw_image, image_data->upload_scale_mip_level);
        SkPixmap pixmap(image_info, image_data->decode.data()->data(),
                        image_info.minRowBytes());
        uploaded_image =
            SkImage::MakeFromRaster(pixmap, [](const void*, void*) {}, nullptr);
        break;
      }
      case DecodedDataMode::GPU: {
        uploaded_image = SkImage::MakeFromDeferredTextureImageData(
            context_->GrContext(), image_data->decode.data()->data(),
            SkBudgeted::kNo);
        break;
      }
    }
  }
  image_data->decode.mark_used();
  DCHECK(uploaded_image);

  // At-raster may have decoded this while we were unlocked. If so, ignore our
  // result.
  if (!image_data->upload.image())
    image_data->upload.SetImage(std::move(uploaded_image));
}

scoped_refptr<GpuImageDecodeController::ImageData>
GpuImageDecodeController::CreateImageData(const DrawImage& draw_image) {
  lock_.AssertAcquired();

  DecodedDataMode mode;
  int upload_scale_mip_level = CalculateUploadScaleMipLevel(draw_image);
  SkImage::DeferredTextureImageUsageParams params =
      ParamsFromDrawImage(draw_image, upload_scale_mip_level);
  size_t data_size = draw_image.image()->getDeferredTextureImageData(
      *context_threadsafe_proxy_.get(), &params, 1, nullptr);

  if (data_size == 0) {
    // Can't upload image, too large or other failure. Try to use SW fallback.
    SkImageInfo image_info =
        CreateImageInfoForDrawImage(draw_image, upload_scale_mip_level);
    data_size = image_info.getSafeSize(image_info.minRowBytes());
    mode = DecodedDataMode::CPU;
  } else {
    mode = DecodedDataMode::GPU;
  }

  return make_scoped_refptr(
      new ImageData(mode, data_size, upload_scale_mip_level,
                    CalculateUploadScaleFilterQuality(draw_image)));
}

void GpuImageDecodeController::DeletePendingImages() {
  context_->GetLock()->AssertAcquired();
  lock_.AssertAcquired();
  images_pending_deletion_.clear();
}

SkImageInfo GpuImageDecodeController::CreateImageInfoForDrawImage(
    const DrawImage& draw_image,
    int upload_scale_mip_level) const {
  gfx::Size mip_size =
      CalculateSizeForMipLevel(draw_image, upload_scale_mip_level);
  return SkImageInfo::Make(mip_size.width(), mip_size.height(),
                           ResourceFormatToClosestSkColorType(format_),
                           kPremul_SkAlphaType);
}

// Tries to find an ImageData that can be used to draw the provided
// |draw_image|. First looks for an exact entry in our |in_use_cache_|. If one
// cannot be found, it looks for a compatible entry in our |persistent_cache_|.
GpuImageDecodeController::ImageData*
GpuImageDecodeController::GetImageDataForDrawImage(
    const DrawImage& draw_image) {
  lock_.AssertAcquired();
  auto found_in_use = in_use_cache_.find(GenerateInUseCacheKey(draw_image));
  if (found_in_use != in_use_cache_.end())
    return found_in_use->second.image_data.get();

  auto found_persistent = persistent_cache_.Get(draw_image.image()->uniqueID());
  if (found_persistent != persistent_cache_.end()) {
    ImageData* image_data = found_persistent->second.get();
    if (IsCompatible(image_data, draw_image)) {
      return image_data;
    } else {
      found_persistent->second->is_orphaned = true;
      // Call OwnershipChanged before erasing the orphaned task from the
      // persistent cache. This ensures that if the orphaned task has 0
      // references, it is cleaned up safely before it is deleted.
      OwnershipChanged(image_data);
      persistent_cache_.Erase(found_persistent);
    }
  }

  return nullptr;
}

// Determines if we can draw the provided |draw_image| using the provided
// |image_data|. This is true if the |image_data| is not scaled, or if it
// is scaled at an equal or larger scale and equal or larger quality to
// the provided |draw_image|.
bool GpuImageDecodeController::IsCompatible(const ImageData* image_data,
                                            const DrawImage& draw_image) const {
  bool is_scaled = image_data->upload_scale_mip_level != 0;
  bool scale_is_compatible = CalculateUploadScaleMipLevel(draw_image) >=
                             image_data->upload_scale_mip_level;
  bool quality_is_compatible = CalculateUploadScaleFilterQuality(draw_image) <=
                               image_data->upload_scale_filter_quality;
  return !is_scaled || (scale_is_compatible && quality_is_compatible);
}

size_t GpuImageDecodeController::GetDrawImageSizeForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  scoped_refptr<ImageData> data = CreateImageData(image);
  return data->size;
}

void GpuImageDecodeController::SetImageDecodingFailedForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.image()->uniqueID());
  DCHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  image_data->decode.decode_failure = true;
}

bool GpuImageDecodeController::DiscardableIsLockedForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.image()->uniqueID());
  DCHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  return image_data->decode.is_locked();
}

}  // namespace cc
