// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/external_video_surface/browser/android/external_video_surface_container_impl.h"

#include "base/android/jni_android.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/ExternalVideoSurfaceContainer_jni.h"
#include "ui/gfx/geometry/rect_f.h"

using base::android::AttachCurrentThread;
using content::ContentViewCore;

namespace external_video_surface {

// static
bool ExternalVideoSurfaceContainerImpl::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
ExternalVideoSurfaceContainerImpl* ExternalVideoSurfaceContainerImpl::Create(
    content::WebContents* web_contents) {
  ContentViewCore* cvc = ContentViewCore::FromWebContents(web_contents);
  if (!cvc)
    return nullptr;
  base::android::ScopedJavaLocalRef<jobject> jcvc = cvc->GetJavaObject();
  if (jcvc.is_null())
    return nullptr;
  return new ExternalVideoSurfaceContainerImpl(jcvc);
}

ExternalVideoSurfaceContainerImpl::ExternalVideoSurfaceContainerImpl(
    base::android::ScopedJavaLocalRef<jobject> java_content_view_core) {
  JNIEnv* env = AttachCurrentThread();
  jobject_.Reset(Java_ExternalVideoSurfaceContainer_create(
      env, reinterpret_cast<intptr_t>(this), java_content_view_core.obj()));
}

ExternalVideoSurfaceContainerImpl::~ExternalVideoSurfaceContainerImpl() {
  JNIEnv* env = AttachCurrentThread();
  Java_ExternalVideoSurfaceContainer_destroy(env, jobject_.obj());
  jobject_.Reset();
}

void ExternalVideoSurfaceContainerImpl::RequestExternalVideoSurface(
    int player_id,
    const SurfaceCreatedCB& surface_created_cb,
    const SurfaceDestroyedCB& surface_destroyed_cb) {
  surface_created_cb_ = surface_created_cb;
  surface_destroyed_cb_ = surface_destroyed_cb;

  JNIEnv* env = AttachCurrentThread();
  Java_ExternalVideoSurfaceContainer_requestExternalVideoSurface(
      env, jobject_.obj(), static_cast<jint>(player_id));
}

int ExternalVideoSurfaceContainerImpl::GetCurrentPlayerId() {
  JNIEnv* env = AttachCurrentThread();

  int current_player = static_cast<int>(
      Java_ExternalVideoSurfaceContainer_getCurrentPlayerId(
          env, jobject_.obj()));

  if (current_player < 0)
    return kInvalidPlayerId;
  else
    return current_player;
}

void ExternalVideoSurfaceContainerImpl::ReleaseExternalVideoSurface(
    int player_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_ExternalVideoSurfaceContainer_releaseExternalVideoSurface(
      env, jobject_.obj(), static_cast<jint>(player_id));

  surface_created_cb_.Reset();
  surface_destroyed_cb_.Reset();
}

void ExternalVideoSurfaceContainerImpl::OnFrameInfoUpdated() {
  JNIEnv* env = AttachCurrentThread();
  Java_ExternalVideoSurfaceContainer_onFrameInfoUpdated(env, jobject_.obj());
}

void ExternalVideoSurfaceContainerImpl::OnExternalVideoSurfacePositionChanged(
    int player_id, const gfx::RectF& rect) {
  JNIEnv* env = AttachCurrentThread();
  Java_ExternalVideoSurfaceContainer_onExternalVideoSurfacePositionChanged(
      env,
      jobject_.obj(),
      static_cast<jint>(player_id),
      static_cast<jfloat>(rect.x()),
      static_cast<jfloat>(rect.y()),
      static_cast<jfloat>(rect.x() + rect.width()),
      static_cast<jfloat>(rect.y() + rect.height()));
}

// Methods called from Java.
void ExternalVideoSurfaceContainerImpl::SurfaceCreated(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint player_id,
    const JavaParamRef<jobject>& jsurface) {
  if (!surface_created_cb_.is_null())
    surface_created_cb_.Run(static_cast<int>(player_id), jsurface);
}

void ExternalVideoSurfaceContainerImpl::SurfaceDestroyed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint player_id) {
  if (!surface_destroyed_cb_.is_null())
    surface_destroyed_cb_.Run(static_cast<int>(player_id));
}

}  // namespace external_video_surface
