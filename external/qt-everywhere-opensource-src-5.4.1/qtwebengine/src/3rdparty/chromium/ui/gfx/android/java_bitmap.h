// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_JAVA_BITMAP_H_
#define UI_GFX_ANDROID_JAVA_BITMAP_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

namespace gfx {

// Define Bitmap Config values like BITMAP_CONFIG_ARGB_8888 in a
// way that ensures they're always the same than their Java counterpart.

enum BitmapConfig {
#define DEFINE_BITMAP_CONFIG(x, y) BITMAP_##x = y,
#include "bitmap_config_list.h"
#undef DEFINE_BITMAP_CONFIG
};

// This class wraps a JNI AndroidBitmap object to make it easier to use. It
// handles locking and unlocking of the underlying pixels, along with wrapping
// various JNI methods.
class GFX_EXPORT JavaBitmap {
 public:
  explicit JavaBitmap(jobject bitmap);
  ~JavaBitmap();

  inline void* pixels() { return pixels_; }
  inline const void* pixels() const { return pixels_; }
  inline const gfx::Size& size() const { return size_; }
  // Formats are in android/bitmap.h; e.g. ANDROID_BITMAP_FORMAT_RGBA_8888
  inline int format() const { return format_; }
  inline uint32_t stride() const { return stride_; }

  // Registers methods with JNI and returns true if succeeded.
  static bool RegisterJavaBitmap(JNIEnv* env);

 private:
  jobject bitmap_;
  void* pixels_;
  gfx::Size size_;
  int format_;
  uint32_t stride_;

  DISALLOW_COPY_AND_ASSIGN(JavaBitmap);
};

// Allocates a Java-backed bitmap (android.graphics.Bitmap) with the given
// (non-empty!) size and configuration.
GFX_EXPORT base::android::ScopedJavaLocalRef<jobject> CreateJavaBitmap(
    int width,
    int height,
    SkBitmap::Config bitmap_config);

// Loads a Java-backed bitmap (android.graphics.Bitmap) from the provided
// drawable resource identifier (e.g., android:drawable/overscroll_glow). If the
// resource loads successfully, it will be integrally scaled down, preserving
// aspect ratio, to a size no smaller than |size|. Otherwise, null is returned.
GFX_EXPORT base::android::ScopedJavaLocalRef<jobject>
    CreateJavaBitmapFromAndroidResource(const char* name, gfx::Size size);

// Converts |skbitmap| to a Java-backed bitmap (android.graphics.Bitmap).
// Note: |skbitmap| is assumed to be non-null, non-empty and one of RGBA_8888 or
// RGB_565 formats.
GFX_EXPORT base::android::ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(
    const SkBitmap* skbitmap);

// Converts |bitmap| to an SkBitmap of the same size and format.
// Note: |jbitmap| is assumed to be non-null, non-empty and of format RGBA_8888.
GFX_EXPORT SkBitmap CreateSkBitmapFromJavaBitmap(const JavaBitmap& jbitmap);

// Returns a Skia config value for the requested input java Bitmap.Config.
GFX_EXPORT SkBitmap::Config ConvertToSkiaConfig(jobject bitmap_config);

}  // namespace gfx

#endif  // UI_GFX_ANDROID_JAVA_BITMAP_H_
