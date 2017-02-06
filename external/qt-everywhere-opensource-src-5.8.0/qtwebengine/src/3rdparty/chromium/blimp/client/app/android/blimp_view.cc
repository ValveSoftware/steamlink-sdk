// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/blimp_view.h"

#include <android/native_window_jni.h>

#include "blimp/client/app/android/blimp_client_session_android.h"
#include "jni/BlimpView_jni.h"
#include "ui/events/android/motion_event_android.h"
#include "ui/gfx/geometry/size.h"

namespace blimp {
namespace client {

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& jobj,
                  const JavaParamRef<jobject>& blimp_client_session,
                  jint real_width,
                  jint real_height,
                  jint width,
                  jint height,
                  jfloat dp_to_px) {
  BlimpClientSession* client_session =
      BlimpClientSessionAndroid::FromJavaObject(env,
                                                blimp_client_session.obj());

  // TODO(dtrainor): Pull the feature object from the BlimpClientSession and
  // pass it through to the BlimpCompositor.
  ALLOW_UNUSED_LOCAL(client_session);

  return reinterpret_cast<intptr_t>(new BlimpView(
      env, jobj, gfx::Size(real_width, real_height), gfx::Size(width, height),
      dp_to_px, client_session->GetRenderWidgetFeature(),
      client_session->GetBlimpConnectionStatistics()));
}

// static
bool BlimpView::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

BlimpView::BlimpView(JNIEnv* env,
                     const JavaParamRef<jobject>& jobj,
                     const gfx::Size& real_size,
                     const gfx::Size& size,
                     float dp_to_px,
                     RenderWidgetFeature* render_widget_feature,
                     BlimpConnectionStatistics* blimp_connection_statistics)
    : device_scale_factor_(dp_to_px),
      compositor_manager_(
          BlimpCompositorManagerAndroid::Create(real_size,
                                                size,
                                                render_widget_feature,
                                                this)),
      current_surface_format_(0),
      window_(gfx::kNullAcceleratedWidget),
      blimp_connection_statistics_(blimp_connection_statistics) {
  java_obj_.Reset(env, jobj);
}

BlimpView::~BlimpView() {
  ReleaseAcceleratedWidget();
}

void BlimpView::Destroy(JNIEnv* env, const JavaParamRef<jobject>& jobj) {
  delete this;
}

void BlimpView::SetNeedsComposite(JNIEnv* env,
                                  const JavaParamRef<jobject>& jobj) {}

void BlimpView::OnSurfaceChanged(JNIEnv* env,
                                 const JavaParamRef<jobject>& jobj,
                                 jint format,
                                 jint width,
                                 jint height,
                                 const JavaParamRef<jobject>& jsurface) {
  if (current_surface_format_ != format) {
    current_surface_format_ = format;
    ReleaseAcceleratedWidget();

    if (jsurface) {
      base::android::ScopedJavaLocalFrame scoped_local_reference_frame(env);
      window_ = ANativeWindow_fromSurface(env, jsurface);
      compositor_manager_->SetAcceleratedWidget(window_);
      compositor_manager_->SetVisible(true);
    }
  }
}

void BlimpView::OnSurfaceCreated(JNIEnv* env,
                                 const JavaParamRef<jobject>& jobj) {
  current_surface_format_ = 0 /** PixelFormat.UNKNOWN */;
}

void BlimpView::OnSurfaceDestroyed(JNIEnv* env,
                                   const JavaParamRef<jobject>& jobj) {
  current_surface_format_ = 0 /** PixelFormat.UNKNOWN */;
  ReleaseAcceleratedWidget();
}

void BlimpView::SetVisibility(JNIEnv* env,
                              const JavaParamRef<jobject>& jobj,
                              jboolean visible) {
  compositor_manager_->SetVisible(visible);
}

void BlimpView::ReleaseAcceleratedWidget() {
  if (window_ == gfx::kNullAcceleratedWidget)
    return;

  compositor_manager_->ReleaseAcceleratedWidget();
  ANativeWindow_release(window_);
  window_ = gfx::kNullAcceleratedWidget;
}

jboolean BlimpView::OnTouchEvent(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 const JavaParamRef<jobject>& motion_event,
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
  ui::MotionEventAndroid::Pointer pointer0(pointer_id_0,
                                           pos_x_0,
                                           pos_y_0,
                                           touch_major_0,
                                           touch_minor_0,
                                           orientation_0,
                                           tilt_0,
                                           android_tool_type_0);
  ui::MotionEventAndroid::Pointer pointer1(pointer_id_1,
                                           pos_x_1,
                                           pos_y_1,
                                           touch_major_1,
                                           touch_minor_1,
                                           orientation_1,
                                           tilt_1,
                                           android_tool_type_1);
  ui::MotionEventAndroid event(1.f / device_scale_factor_,
                               env,
                               motion_event,
                               time_ms,
                               android_action,
                               pointer_count,
                               history_size,
                               action_index,
                               android_button_state,
                               android_meta_state,
                               raw_pos_x - pos_x_0,
                               raw_pos_y - pos_y_0,
                               pointer0,
                               pointer1);

  return compositor_manager_->OnTouchEvent(event);
}

void BlimpView::OnSwapBuffersCompleted() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BlimpView_onSwapBuffersCompleted(env, java_obj_.obj());
}

void BlimpView::DidCommitAndDrawFrame() {
  DCHECK(blimp_connection_statistics_);
  blimp_connection_statistics_->Add(BlimpConnectionStatistics::COMMIT, 1);
}

}  // namespace client
}  // namespace blimp
