// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/resource_manager_impl.h"

#include <inttypes.h>
#include <stddef.h>

#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/resources/ui_resource_manager.h"
#include "jni/ResourceManager_jni.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"

using base::android::JavaArrayOfIntArrayToIntVector;
using base::android::JavaRef;

namespace ui {

// static
ResourceManagerImpl* ResourceManagerImpl::FromJavaObject(
    const JavaRef<jobject>& jobj) {
  return reinterpret_cast<ResourceManagerImpl*>(
      Java_ResourceManager_getNativePtr(base::android::AttachCurrentThread(),
                                        jobj));
}

ResourceManagerImpl::ResourceManagerImpl(gfx::NativeWindow native_window)
    : ui_resource_manager_(nullptr) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env, Java_ResourceManager_create(env, native_window->GetJavaObject(),
                                       reinterpret_cast<intptr_t>(this))
               .obj());
  DCHECK(!java_obj_.is_null());
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "android::ResourceManagerImpl",
      base::ThreadTaskRunnerHandle::Get());
}

ResourceManagerImpl::~ResourceManagerImpl() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  Java_ResourceManager_destroy(base::android::AttachCurrentThread(), java_obj_);
}

void ResourceManagerImpl::Init(cc::UIResourceManager* ui_resource_manager) {
  DCHECK(!ui_resource_manager_);
  DCHECK(ui_resource_manager);
  ui_resource_manager_ = ui_resource_manager;
}

base::android::ScopedJavaLocalRef<jobject>
ResourceManagerImpl::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

ResourceManager::Resource* ResourceManagerImpl::GetResource(
    AndroidResourceType res_type,
    int res_id) {
  DCHECK_GE(res_type, ANDROID_RESOURCE_TYPE_FIRST);
  DCHECK_LE(res_type, ANDROID_RESOURCE_TYPE_LAST);

  std::unordered_map<int, std::unique_ptr<Resource>>::iterator item =
      resources_[res_type].find(res_id);

  if (item == resources_[res_type].end() ||
      res_type == ANDROID_RESOURCE_TYPE_DYNAMIC ||
      res_type == ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP) {
    RequestResourceFromJava(res_type, res_id);

    // Check if the resource has been added (some dynamic may not have been).
    item = resources_[res_type].find(res_id);
    if (item == resources_[res_type].end())
      return nullptr;
  }

  return item->second.get();
}

void ResourceManagerImpl::RemoveUnusedTints(
    const std::unordered_set<int>& used_tints) {
  // Iterate over the currently cached tints and remove ones that were not
  // used as defined in |used_tints|.
  for (auto it = tinted_resources_.cbegin(); it != tinted_resources_.cend();) {
    if (used_tints.find(it->first) == used_tints.end()) {
        it = tinted_resources_.erase(it);
    } else {
        ++it;
    }
  }
}

ResourceManager::Resource* ResourceManagerImpl::GetStaticResourceWithTint(
    int res_id,
    SkColor tint_color) {
  if (tinted_resources_.find(tint_color) == tinted_resources_.end()) {
    tinted_resources_[tint_color] = base::MakeUnique<ResourceMap>();
  }
  ResourceMap* resource_map = tinted_resources_[tint_color].get();

  // If the resource is already cached, use it.
  std::unordered_map<int, std::unique_ptr<Resource>>::iterator item =
      resource_map->find(res_id);
  if (item != resource_map->end())
    return item->second.get();

  std::unique_ptr<Resource> tinted_resource = base::MakeUnique<Resource>();

  ResourceManager::Resource* base_image =
      GetResource(ANDROID_RESOURCE_TYPE_STATIC, res_id);
  DCHECK(base_image);

  TRACE_EVENT0("browser", "ResourceManagerImpl::GetStaticResourceWithTint");
  SkBitmap tinted_bitmap;
  tinted_bitmap.allocPixels(SkImageInfo::MakeN32Premul(base_image->size.width(),
      base_image->size.height()));

  SkCanvas canvas(tinted_bitmap);
  canvas.clear(SK_ColorTRANSPARENT);

  // Build a color filter to use on the base resource. This filter multiplies
  // the RGB components by the components of the new color but retains the
  // alpha of the original image.
  SkPaint color_filter;
  color_filter.setColorFilter(
      SkColorFilter::MakeModeFilter(tint_color, SkBlendMode::kModulate));

  // Draw the resource and make it immutable.
  base_image->ui_resource->GetBitmap(base_image->ui_resource->id(), false)
      .DrawToCanvas(&canvas, &color_filter);
  tinted_bitmap.setImmutable();

  // Create a UI resource from the new bitmap.
  tinted_resource->size = gfx::Size(base_image->size);
  tinted_resource->padding = gfx::Rect(base_image->padding);
  tinted_resource->aperture = gfx::Rect(base_image->aperture);
  tinted_resource->ui_resource = cc::ScopedUIResource::Create(
      ui_resource_manager_, cc::UIResourceBitmap(tinted_bitmap));

  (*resource_map)[res_id].swap(tinted_resource);

  return (*resource_map)[res_id].get();
}

void ResourceManagerImpl::ClearTintedResourceCache(JNIEnv* env,
    const JavaRef<jobject>& jobj) {
  tinted_resources_.clear();
}

void ResourceManagerImpl::PreloadResource(AndroidResourceType res_type,
                                          int res_id) {
  DCHECK_GE(res_type, ANDROID_RESOURCE_TYPE_FIRST);
  DCHECK_LE(res_type, ANDROID_RESOURCE_TYPE_LAST);

  // Don't send out a query if the resource is already loaded.
  if (resources_[res_type].find(res_id) != resources_[res_type].end())
    return;

  PreloadResourceFromJava(res_type, res_id);
}

void ResourceManagerImpl::OnResourceReady(JNIEnv* env,
                                          const JavaRef<jobject>& jobj,
                                          jint res_type,
                                          jint res_id,
                                          const JavaRef<jobject>& bitmap,
                                          jint padding_left,
                                          jint padding_top,
                                          jint padding_right,
                                          jint padding_bottom,
                                          jint aperture_left,
                                          jint aperture_top,
                                          jint aperture_right,
                                          jint aperture_bottom) {
  DCHECK_GE(res_type, ANDROID_RESOURCE_TYPE_FIRST);
  DCHECK_LE(res_type, ANDROID_RESOURCE_TYPE_LAST);
  TRACE_EVENT2("ui", "ResourceManagerImpl::OnResourceReady",
               "resource_type", res_type,
               "resource_id", res_id);

  std::unordered_map<int, std::unique_ptr<Resource>>::iterator item =
      resources_[res_type].find(res_id);
  if (item == resources_[res_type].end()) {
    resources_[res_type][res_id] = base::MakeUnique<Resource>();
  }

  Resource* resource = resources_[res_type][res_id].get();

  gfx::JavaBitmap jbitmap(bitmap);
  resource->size = jbitmap.size();
  resource->padding.SetRect(padding_left, padding_top,
                            padding_right - padding_left,
                            padding_bottom - padding_top);
  resource->aperture.SetRect(aperture_left, aperture_top,
                             aperture_right - aperture_left,
                             aperture_bottom - aperture_top);

  SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(jbitmap);
  skbitmap.setImmutable();
  resource->ui_resource = cc::ScopedUIResource::Create(
      ui_resource_manager_, cc::UIResourceBitmap(skbitmap));
}

void ResourceManagerImpl::RemoveResource(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    jint res_type,
    jint res_id) {
  resources_[res_type].erase(res_id);
}

CrushedSpriteResource* ResourceManagerImpl::GetCrushedSpriteResource(
    int bitmap_res_id, int metadata_res_id) {

  CrushedSpriteResource* resource = nullptr;
  if (crushed_sprite_resources_.find(bitmap_res_id)
      != crushed_sprite_resources_.end()) {
    resource = crushed_sprite_resources_[bitmap_res_id].get();
  }

  if (!resource) {
    RequestCrushedSpriteResourceFromJava(bitmap_res_id, metadata_res_id, false);
    resource = crushed_sprite_resources_[bitmap_res_id].get();
  } else if (resource->BitmapHasBeenEvictedFromMemory()) {
    RequestCrushedSpriteResourceFromJava(bitmap_res_id, metadata_res_id, true);
  }

  return resource;
}

void ResourceManagerImpl::OnCrushedSpriteResourceReady(
    JNIEnv* env,
    const JavaRef<jobject>& jobj,
    jint bitmap_res_id,
    const JavaRef<jobject>& bitmap,
    const JavaRef<jobjectArray>& frame_rects,
    jint unscaled_sprite_width,
    jint unscaled_sprite_height,
    jfloat scaled_sprite_width,
    jfloat scaled_sprite_height) {
  // Construct source and destination rectangles for each frame from
  // |frame_rects|.
  std::vector<std::vector<int>> all_frame_rects_vector;
  JavaArrayOfIntArrayToIntVector(env, frame_rects.obj(),
                                 &all_frame_rects_vector);
  CrushedSpriteResource::SrcDstRects src_dst_rects =
      ProcessCrushedSpriteFrameRects(all_frame_rects_vector);

  SkBitmap skbitmap =
      gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(bitmap));

  std::unique_ptr<CrushedSpriteResource> resource =
      base::MakeUnique<CrushedSpriteResource>(
          skbitmap,
          src_dst_rects,
          gfx::Size(unscaled_sprite_width, unscaled_sprite_height),
          gfx::Size(scaled_sprite_width, scaled_sprite_height));

  crushed_sprite_resources_[bitmap_res_id].swap(resource);
}

CrushedSpriteResource::SrcDstRects
ResourceManagerImpl::ProcessCrushedSpriteFrameRects(
    std::vector<std::vector<int>> frame_rects_vector) {
  CrushedSpriteResource::SrcDstRects src_dst_rects;
  for (size_t i = 0; i < frame_rects_vector.size(); ++i) {
    std::vector<int> frame_ints = frame_rects_vector[i];
    CrushedSpriteResource::FrameSrcDstRects frame_src_dst_rects;

    // Create source and destination gfx::Rect's for each rectangle in
    // |frame_ints|. Each rectangle consists of 6 values:
    // i: destination x   i+1: destination y   i+2: source x   i+3: source y
    // i+4: width   i+5: height
    for (size_t j = 0; j < frame_ints.size(); j += 6) {
      gfx::Rect sprite_rect_destination(frame_ints[j],
                                        frame_ints[j+1],
                                        frame_ints[j+4],
                                        frame_ints[j+5]);
      gfx::Rect sprite_rect_source(frame_ints[j+2],
                                   frame_ints[j+3],
                                   frame_ints[j+4],
                                   frame_ints[j+5]);
      frame_src_dst_rects.push_back(std::pair<gfx::Rect, gfx::Rect>(
          sprite_rect_source, sprite_rect_destination));
    }
    src_dst_rects.push_back(frame_src_dst_rects);
  }
  return src_dst_rects;
}

bool ResourceManagerImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  size_t memory_usage =
      base::trace_event::EstimateMemoryUsage(resources_) +
      base::trace_event::EstimateMemoryUsage(crushed_sprite_resources_) +
      base::trace_event::EstimateMemoryUsage(tinted_resources_);

  base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
      base::StringPrintf("ui/resource_manager_0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(this)));
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  memory_usage);

  const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  if (system_allocator_name) {
    pmd->AddSuballocation(dump->guid(), system_allocator_name);
  }

  return true;
}

void ResourceManagerImpl::OnCrushedSpriteResourceReloaded(
    JNIEnv* env,
    const JavaRef<jobject>& jobj,
    jint bitmap_res_id,
    const JavaRef<jobject>& bitmap) {
  std::unordered_map<int, std::unique_ptr<CrushedSpriteResource>>::iterator
      item = crushed_sprite_resources_.find(bitmap_res_id);
  if (item == crushed_sprite_resources_.end()) {
    // Cannot reload a resource that has not been previously loaded.
    return;
  }
  SkBitmap skbitmap =
      gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(bitmap));
  item->second->SetBitmap(skbitmap);
}

// static
bool ResourceManagerImpl::RegisterResourceManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ResourceManagerImpl::PreloadResourceFromJava(AndroidResourceType res_type,
                                                  int res_id) {
  TRACE_EVENT2("ui", "ResourceManagerImpl::PreloadResourceFromJava",
               "resource_type", res_type,
               "resource_id", res_id);
  Java_ResourceManager_preloadResource(base::android::AttachCurrentThread(),
                                       java_obj_, res_type, res_id);
}

void ResourceManagerImpl::RequestResourceFromJava(AndroidResourceType res_type,
                                                  int res_id) {
  TRACE_EVENT2("ui", "ResourceManagerImpl::RequestResourceFromJava",
               "resource_type", res_type,
               "resource_id", res_id);
  Java_ResourceManager_resourceRequested(base::android::AttachCurrentThread(),
                                         java_obj_, res_type, res_id);
}

void ResourceManagerImpl::RequestCrushedSpriteResourceFromJava(
    int bitmap_res_id, int metadata_res_id, bool reloading) {
  TRACE_EVENT2("ui",
               "ResourceManagerImpl::RequestCrushedSpriteResourceFromJava",
               "bitmap_res_id", bitmap_res_id,
               "metadata_res_id", metadata_res_id);
  Java_ResourceManager_crushedSpriteResourceRequested(
      base::android::AttachCurrentThread(), java_obj_, bitmap_res_id,
      metadata_res_id, reloading);
}

}  // namespace ui
