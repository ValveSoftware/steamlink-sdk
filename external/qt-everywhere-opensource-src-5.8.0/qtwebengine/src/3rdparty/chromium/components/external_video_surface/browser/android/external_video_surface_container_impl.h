// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXTERNAL_VIDEO_SURFACE_EXTERNAL_VIDEO_SURFACE_CONTAINER_IMPL_H_
#define COMPONENTS_EXTERNAL_VIDEO_SURFACE_EXTERNAL_VIDEO_SURFACE_CONTAINER_IMPL_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/android/external_video_surface_container.h"

namespace external_video_surface {

class ExternalVideoSurfaceContainerImpl
    : public content::ExternalVideoSurfaceContainer {
 public:
  static bool RegisterJni(JNIEnv* env);

  typedef base::Callback<void(int, jobject)> SurfaceCreatedCB;
  typedef base::Callback<void(int)> SurfaceDestroyedCB;

  static ExternalVideoSurfaceContainerImpl* Create(
      content::WebContents* web_contents);

  // content::ExternalVideoSurfaceContainer implementation.
  void RequestExternalVideoSurface(
      int player_id,
      const SurfaceCreatedCB& surface_created_cb,
      const SurfaceDestroyedCB& surface_destroyed_cb) override;
  int GetCurrentPlayerId() override;
  void ReleaseExternalVideoSurface(int player_id) override;
  void OnFrameInfoUpdated() override;
  void OnExternalVideoSurfacePositionChanged(int player_id,
                                             const gfx::RectF& rect) override;

  // Methods called from Java.
  void SurfaceCreated(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jint player_id,
                      const base::android::JavaParamRef<jobject>& jsurface);
  void SurfaceDestroyed(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jint player_id);

 private:
  explicit ExternalVideoSurfaceContainerImpl(
      base::android::ScopedJavaLocalRef<jobject> java_content_view_core);
  ~ExternalVideoSurfaceContainerImpl() override;

  base::android::ScopedJavaGlobalRef<jobject> jobject_;

  SurfaceCreatedCB surface_created_cb_;
  SurfaceDestroyedCB surface_destroyed_cb_;

  DISALLOW_COPY_AND_ASSIGN(ExternalVideoSurfaceContainerImpl);
};

}  // namespace external_video_surface

#endif  // COMPONENTS_EXTERNAL_VIDEO_SURFACE_EXTERNAL_VIDEO_SURFACE_CONTAINER_IMPL_H_
