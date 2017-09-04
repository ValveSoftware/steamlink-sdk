// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/blimp_contents_display.h"

#include <android/native_window_jni.h>

#include "base/memory/ptr_util.h"
#include "blimp/client/app/android/blimp_client_session_android.h"
#include "blimp/client/app/compositor/browser_compositor.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/render_widget/blimp_document_manager.h"
#include "blimp/client/core/render_widget/render_widget_feature.h"
#include "blimp/client/support/compositor/compositor_dependencies_impl.h"
#include "jni/BlimpContentsDisplay_jni.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/gfx/geometry/size.h"

namespace {
const int kDummyBlimpContentsId = 0;
}  // namespace

namespace blimp {
namespace client {
namespace app {

static jlong Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const base::android::JavaParamRef<jobject>& blimp_client_session,
    jint real_width,
    jint real_height,
    jint width,
    jint height,
    jfloat dp_to_px) {
  BlimpClientSession* client_session =
      BlimpClientSessionAndroid::FromJavaObject(env, blimp_client_session);

  // TODO(dtrainor): Pull the feature object from the BlimpClientSession and
  // pass it through to the BlimpCompositor.
  ALLOW_UNUSED_LOCAL(client_session);

  return reinterpret_cast<intptr_t>(new BlimpContentsDisplay(
      env, jobj, gfx::Size(real_width, real_height), gfx::Size(width, height),
      dp_to_px, client_session->GetRenderWidgetFeature()));
}

// static
bool BlimpContentsDisplay::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

BlimpContentsDisplay::BlimpContentsDisplay(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const gfx::Size& real_size,
    const gfx::Size& size,
    float dp_to_px,
    blimp::client::RenderWidgetFeature* render_widget_feature)
    : device_scale_factor_(dp_to_px),
      current_surface_format_(0),
      window_(gfx::kNullAcceleratedWidget),
      weak_ptr_factory_(this) {
  compositor_dependencies_ = base::MakeUnique<BlimpCompositorDependencies>(
      base::MakeUnique<CompositorDependenciesImpl>());

  compositor_ = base::MakeUnique<BrowserCompositor>(
      compositor_dependencies_->GetEmbedderDependencies());
  compositor_->set_did_complete_swap_buffers_callback(
      base::Bind(&BlimpContentsDisplay::OnSwapBuffersCompleted,
                 weak_ptr_factory_.GetWeakPtr()));

  document_manager_ = base::MakeUnique<BlimpDocumentManager>(
      kDummyBlimpContentsId, render_widget_feature,
      compositor_dependencies_.get());
  compositor_->SetContentLayer(document_manager_->layer());

  java_obj_.Reset(env, jobj);
}

BlimpContentsDisplay::~BlimpContentsDisplay() {
  SetSurface(nullptr);

  // Destroy the BrowserCompositor and the BlimpCompositorManager before the
  // BlimpCompositorDependencies.
  compositor_.reset();
  document_manager_.reset();
  compositor_dependencies_.reset();
}

void BlimpContentsDisplay::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  delete this;
}

void BlimpContentsDisplay::OnContentAreaSizeChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    jint width,
    jint height,
    jfloat dpToPx) {
  compositor_->SetSize(gfx::Size(width, height));
}

void BlimpContentsDisplay::OnSurfaceChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    jint format,
    jint width,
    jint height,
    const base::android::JavaParamRef<jobject>& jsurface) {
  if (current_surface_format_ != format) {
    current_surface_format_ = format;
    SetSurface(nullptr);

    if (jsurface) {
      SetSurface(jsurface);
    }
  }
}

void BlimpContentsDisplay::OnSurfaceCreated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  current_surface_format_ = 0 /** PixelFormat.UNKNOWN */;
}

void BlimpContentsDisplay::OnSurfaceDestroyed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  current_surface_format_ = 0 /** PixelFormat.UNKNOWN */;
  SetSurface(nullptr);
}

void BlimpContentsDisplay::SetSurface(jobject surface) {
  JNIEnv* env = base::android::AttachCurrentThread();
  // Release all references to the old surface.
  if (window_ != gfx::kNullAcceleratedWidget) {
    compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
    document_manager_->SetVisible(false);
    ANativeWindow_release(window_);
    window_ = gfx::kNullAcceleratedWidget;
  }

  if (surface) {
    base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
    window_ = ANativeWindow_fromSurface(env, surface);
    compositor_->SetAcceleratedWidget(window_);
    document_manager_->SetVisible(true);
  }
}

jboolean BlimpContentsDisplay::OnTouchEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& motion_event,
    jlong time_ms,
    jint android_action,
    jint pointer_count,
    jint history_size,
    jint action_index,
    jfloat pos_x_0,
    jfloat pos_y_0,
    jfloat pos_x_1,
    jfloat pos_y_1,
    jint pointer_id_0,
    jint pointer_id_1,
    jfloat touch_major_0,
    jfloat touch_major_1,
    jfloat touch_minor_0,
    jfloat touch_minor_1,
    jfloat orientation_0,
    jfloat orientation_1,
    jfloat tilt_0,
    jfloat tilt_1,
    jfloat raw_pos_x,
    jfloat raw_pos_y,
    jint android_tool_type_0,
    jint android_tool_type_1,
    jint android_button_state,
    jint android_meta_state) {
  ui::MotionEventAndroid::Pointer pointer0(
      pointer_id_0, pos_x_0, pos_y_0, touch_major_0, touch_minor_0,
      orientation_0, tilt_0, android_tool_type_0);
  ui::MotionEventAndroid::Pointer pointer1(
      pointer_id_1, pos_x_1, pos_y_1, touch_major_1, touch_minor_1,
      orientation_1, tilt_1, android_tool_type_1);
  ui::MotionEventAndroid event(1.f / device_scale_factor_, env, motion_event,
                               time_ms, android_action, pointer_count,
                               history_size, action_index, android_button_state,
                               android_meta_state, raw_pos_x - pos_x_0,
                               raw_pos_y - pos_y_0, &pointer0, &pointer1);

  return document_manager_->OnTouchEvent(event);
}

void BlimpContentsDisplay::OnSwapBuffersCompleted() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BlimpContentsDisplay_onSwapBuffersCompleted(env, java_obj_);
}

}  // namespace app
}  // namespace client
}  // namespace blimp
