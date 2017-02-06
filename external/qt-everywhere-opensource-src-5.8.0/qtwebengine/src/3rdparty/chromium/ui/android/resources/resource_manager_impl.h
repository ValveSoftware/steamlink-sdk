// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_RESOURCE_MANAGER_IMPL_H_
#define UI_ANDROID_RESOURCES_RESOURCE_MANAGER_IMPL_H_

#include "base/id_map.h"
#include "base/macros.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/ui_android_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class UI_ANDROID_EXPORT ResourceManagerImpl : public ResourceManager {
 public:
  static ResourceManagerImpl* FromJavaObject(jobject jobj);

  explicit ResourceManagerImpl(gfx::NativeWindow native_window);
  ~ResourceManagerImpl() override;

  void Init(cc::LayerTreeHost* host);

  // ResourceManager implementation.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
  Resource* GetResource(AndroidResourceType res_type, int res_id) override;
  void PreloadResource(AndroidResourceType res_type, int res_id) override;
  CrushedSpriteResource* GetCrushedSpriteResource(
      int bitmap_res_id, int metadata_res_id) override;

  // Called from Java
  // ----------------------------------------------------------
  void OnResourceReady(JNIEnv* env,
                       const base::android::JavaRef<jobject>& jobj,
                       jint res_type,
                       jint res_id,
                       const base::android::JavaRef<jobject>& bitmap,
                       jint padding_left,
                       jint padding_top,
                       jint padding_right,
                       jint padding_bottom,
                       jint aperture_left,
                       jint aperture_top,
                       jint aperture_right,
                       jint aperture_bottom);
  void OnCrushedSpriteResourceReady(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      jint bitmap_res_id,
      const base::android::JavaRef<jobject>& bitmap,
      const base::android::JavaRef<jobjectArray>& frame_rects,
      jint unscaled_sprite_width,
      jint unscaled_sprite_height,
      jfloat scaled_sprite_width,
      jfloat scaled_sprite_height);
  void OnCrushedSpriteResourceReloaded(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj,
      jint bitmap_res_id,
      const base::android::JavaRef<jobject>& bitmap);

  static bool RegisterResourceManager(JNIEnv* env);

  // Helper method for processing crushed sprite metadata; public for testing.
  CrushedSpriteResource::SrcDstRects ProcessCrushedSpriteFrameRects(
      std::vector<std::vector<int>> frame_rects_vector);

 private:
  friend class TestResourceManagerImpl;

  // Start loading the resource. virtual for testing.
  virtual void PreloadResourceFromJava(AndroidResourceType res_type,
                                       int res_id);
  virtual void RequestResourceFromJava(AndroidResourceType res_type,
                                       int res_id);
  virtual void RequestCrushedSpriteResourceFromJava(int bitmap_res_id,
                                                    int metadata_res_id,
                                                    bool reloading);

  typedef IDMap<Resource, IDMapOwnPointer> ResourceMap;
  typedef IDMap<CrushedSpriteResource, IDMapOwnPointer>
      CrushedSpriteResourceMap;

  cc::LayerTreeHost* host_;
  ResourceMap resources_[ANDROID_RESOURCE_TYPE_COUNT];
  CrushedSpriteResourceMap crushed_sprite_resources_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(ResourceManagerImpl);
};

}  // namespace ui

#endif  // UI_ANDROID_RESOURCES_RESOURCE_MANAGER_IMPL_H_
