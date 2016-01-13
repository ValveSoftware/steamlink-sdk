// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_

#include <jni.h>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

class BrowserMediaPlayerManager;
class PowerSaveBlocker;

// Native mirror of ContentVideoView.java. This class is responsible for
// creating the Java video view and pass all the player status change to
// it. It accepts media control from Java class, and forwards it to
// MediaPlayerManagerImpl.
class ContentVideoView {
 public:
  // Construct a ContentVideoView object. The |manager| will handle all the
  // playback controls from the Java class.
  explicit ContentVideoView(BrowserMediaPlayerManager* manager);

  ~ContentVideoView();

  // To open another video on existing ContentVideoView.
  void OpenVideo();

  static bool RegisterContentVideoView(JNIEnv* env);
  static void KeepScreenOn(bool screen_on);

  // Return the singleton object or NULL.
  static ContentVideoView* GetInstance();

  // Getter method called by the Java class to get the media information.
  int GetVideoWidth(JNIEnv*, jobject obj) const;
  int GetVideoHeight(JNIEnv*, jobject obj) const;
  int GetDurationInMilliSeconds(JNIEnv*, jobject obj) const;
  int GetCurrentPosition(JNIEnv*, jobject obj) const;
  bool IsPlaying(JNIEnv*, jobject obj);
  void RequestMediaMetadata(JNIEnv*, jobject obj);

  // Called when the Java fullscreen view is destroyed. If
  // |release_media_player| is true, |manager_| needs to release the player
  // as we are quitting the app.
  void ExitFullscreen(JNIEnv*, jobject, jboolean release_media_player);

  // Media control method called by the Java class.
  void SeekTo(JNIEnv*, jobject obj, jint msec);
  void Play(JNIEnv*, jobject obj);
  void Pause(JNIEnv*, jobject obj);

  // Called by the Java class to pass the surface object to the player.
  void SetSurface(JNIEnv*, jobject obj, jobject surface);

  // Method called by |manager_| to inform the Java class about player status
  // change.
  void UpdateMediaMetadata();
  void OnMediaPlayerError(int errorType);
  void OnVideoSizeChanged(int width, int height);
  void OnBufferingUpdate(int percent);
  void OnPlaybackComplete();
  void OnExitFullscreen();

  // Return the corresponing ContentVideoView Java object if any.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject(JNIEnv* env);

 private:
  // Creates the corresponding ContentVideoView Java object.
  JavaObjectWeakGlobalRef CreateJavaObject();

  // Returns the associated NativeView
  gfx::NativeView GetNativeView();

  void CreatePowerSaveBlocker();

  // Object that manages the fullscreen media player. It is responsible for
  // handling all the playback controls.
  BrowserMediaPlayerManager* manager_;

  // PowerSaveBlock to keep screen on for fullscreen video.
  // There is already blocker when inline video started, and it requires the
  // ContentView's container displayed to take effect; but in WebView, apps
  // could use another container to hold ContentVideoView, and the blocker in
  // ContentView's container can not keep screen on; so we need another blocker
  // here, it is no harm, just an additonal blocker.
  scoped_ptr<PowerSaveBlocker> power_save_blocker_;

  // Weak reference of corresponding Java object.
  JavaObjectWeakGlobalRef j_content_video_view_;

  // Weak pointer for posting tasks.
  base::WeakPtrFactory<ContentVideoView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentVideoView);
};

} // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_
