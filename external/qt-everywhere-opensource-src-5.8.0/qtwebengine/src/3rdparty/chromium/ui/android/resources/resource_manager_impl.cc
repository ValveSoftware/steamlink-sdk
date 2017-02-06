// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/resource_manager_impl.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/trace_event/trace_event.h"
#include "cc/resources/scoped_ui_resource.h"
#include "jni/ResourceManager_jni.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/rect.h"

using base::android::JavaArrayOfIntArrayToIntVector;
using base::android::JavaRef;

namespace ui {

// static
ResourceManagerImpl* ResourceManagerImpl::FromJavaObject(jobject jobj) {
  return reinterpret_cast<ResourceManagerImpl*>(
      Java_ResourceManager_getNativePtr(base::android::AttachCurrentThread(),
                                        jobj));
}

ResourceManagerImpl::ResourceManagerImpl(gfx::NativeWindow native_window)
    : host_(nullptr) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_ResourceManager_create(
                           env, native_window->GetJavaObject().obj(),
                           reinterpret_cast<intptr_t>(this))
                           .obj());
  DCHECK(!java_obj_.is_null());
}

ResourceManagerImpl::~ResourceManagerImpl() {
  Java_ResourceManager_destroy(base::android::AttachCurrentThread(),
                               java_obj_.obj());
}

void ResourceManagerImpl::Init(cc::LayerTreeHost* host) {
  DCHECK(!host_);
  DCHECK(host);
  host_ = host;
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

  Resource* resource = resources_[res_type].Lookup(res_id);

  if (!resource || res_type == ANDROID_RESOURCE_TYPE_DYNAMIC ||
      res_type == ANDROID_RESOURCE_TYPE_DYNAMIC_BITMAP) {
    RequestResourceFromJava(res_type, res_id);
    resource = resources_[res_type].Lookup(res_id);
  }

  return resource;
}

void ResourceManagerImpl::PreloadResource(AndroidResourceType res_type,
                                          int res_id) {
  DCHECK_GE(res_type, ANDROID_RESOURCE_TYPE_FIRST);
  DCHECK_LE(res_type, ANDROID_RESOURCE_TYPE_LAST);

  // Don't send out a query if the resource is already loaded.
  if (resources_[res_type].Lookup(res_id))
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

  Resource* resource = resources_[res_type].Lookup(res_id);
  if (!resource) {
    resource = new Resource();
    resources_[res_type].AddWithID(resource, res_id);
  }

  gfx::JavaBitmap jbitmap(bitmap.obj());
  resource->size = jbitmap.size();
  resource->padding.SetRect(padding_left, padding_top,
                            padding_right - padding_left,
                            padding_bottom - padding_top);
  resource->aperture.SetRect(aperture_left, aperture_top,
                             aperture_right - aperture_left,
                             aperture_bottom - aperture_top);

  SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(jbitmap);
  skbitmap.setImmutable();
  resource->ui_resource =
      cc::ScopedUIResource::Create(host_, cc::UIResourceBitmap(skbitmap));
}

CrushedSpriteResource* ResourceManagerImpl::GetCrushedSpriteResource(
    int bitmap_res_id, int metadata_res_id) {
  CrushedSpriteResource* resource =
      crushed_sprite_resources_.Lookup(bitmap_res_id);
  if (!resource) {
    RequestCrushedSpriteResourceFromJava(bitmap_res_id, metadata_res_id, false);
    resource = crushed_sprite_resources_.Lookup(bitmap_res_id);
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
      gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(bitmap.obj()));

  CrushedSpriteResource* resource = new CrushedSpriteResource(
      skbitmap,
      src_dst_rects,
      gfx::Size(unscaled_sprite_width, unscaled_sprite_height),
      gfx::Size(scaled_sprite_width, scaled_sprite_height));

  if (crushed_sprite_resources_.Lookup(bitmap_res_id)) {
    crushed_sprite_resources_.Replace(bitmap_res_id, resource);
  } else {
    crushed_sprite_resources_.AddWithID(resource, bitmap_res_id);
  }
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

void ResourceManagerImpl::OnCrushedSpriteResourceReloaded(
    JNIEnv* env,
    const JavaRef<jobject>& jobj,
    jint bitmap_res_id,
    const JavaRef<jobject>& bitmap) {
  CrushedSpriteResource* resource =
      crushed_sprite_resources_.Lookup(bitmap_res_id);
  if (!resource) {
    // Cannot reload a resource that has not been previously loaded.
    return;
  }
  SkBitmap skbitmap =
      gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(bitmap.obj()));
  resource->SetBitmap(skbitmap);
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
                                       java_obj_.obj(), res_type, res_id);
}

void ResourceManagerImpl::RequestResourceFromJava(AndroidResourceType res_type,
                                                  int res_id) {
  TRACE_EVENT2("ui", "ResourceManagerImpl::RequestResourceFromJava",
               "resource_type", res_type,
               "resource_id", res_id);
  Java_ResourceManager_resourceRequested(base::android::AttachCurrentThread(),
                                         java_obj_.obj(), res_type, res_id);
}

void ResourceManagerImpl::RequestCrushedSpriteResourceFromJava(
    int bitmap_res_id, int metadata_res_id, bool reloading) {
  TRACE_EVENT2("ui",
               "ResourceManagerImpl::RequestCrushedSpriteResourceFromJava",
               "bitmap_res_id", bitmap_res_id,
               "metadata_res_id", metadata_res_id);
  Java_ResourceManager_crushedSpriteResourceRequested(
      base::android::AttachCurrentThread(), java_obj_.obj(),
      bitmap_res_id, metadata_res_id, reloading);
}

}  // namespace ui
