// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_ANDROID_BLIMP_CONTENTS_DISPLAY_H_
#define BLIMP_CLIENT_APP_ANDROID_BLIMP_CONTENTS_DISPLAY_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Size;
}

namespace blimp {
namespace client {
class BlimpCompositorDependencies;
class BlimpDocumentManager;
class BrowserCompositor;
class RenderWidgetFeature;

namespace app {

// The native component of org.chromium.blimp.BlimpContentsDisplay. This builds
// and maintains a BlimpCompositorAndroid and handles notifying the compositor
// of SurfaceView surface changes (size, creation, destruction, etc.).
class BlimpContentsDisplay {
 public:
  static bool RegisterJni(JNIEnv* env);

  // |real_size| is the total display area including system decorations (see
  // android.view.Display.getRealSize()).  |size| is the total display
  // area not including system decorations (see android.view.Display.getSize()).
  // |dp_to_px| is the scale factor that is required to convert dp (device
  // pixels) to px.
  BlimpContentsDisplay(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const gfx::Size& real_size,
      const gfx::Size& size,
      float dp_to_px,
      blimp::client::RenderWidgetFeature* render_widget_feature);

  // Methods called from Java via JNI.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);
  void OnContentAreaSizeChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      jint width,
      jint height,
      jfloat dpToPx);
  void OnSurfaceChanged(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jobj,
                        jint format,
                        jint width,
                        jint height,
                        const base::android::JavaParamRef<jobject>& jsurface);
  void OnSurfaceCreated(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jobj);
  void OnSurfaceDestroyed(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& jobj);
  jboolean OnTouchEvent(
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
      jint android_meta_state);

 private:
  virtual ~BlimpContentsDisplay();

  void OnSwapBuffersCompleted();

  void SetSurface(jobject surface);

  // Reference to the Java object which owns this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  const float device_scale_factor_;

  std::unique_ptr<BlimpCompositorDependencies> compositor_dependencies_;
  std::unique_ptr<BlimpDocumentManager> document_manager_;
  std::unique_ptr<BrowserCompositor> compositor_;

  // The format of the current surface owned by |compositor_|.  See
  // android.graphics.PixelFormat.java.
  int current_surface_format_;

  gfx::AcceleratedWidget window_;

  base::WeakPtrFactory<BlimpContentsDisplay> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsDisplay);
};

}  // namespace app
}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_ANDROID_BLIMP_CONTENTS_DISPLAY_H_
