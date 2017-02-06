# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # TODO(jrg): Update this action and other jni generators to only
  # require specifying the java directory and generate the rest.
  # TODO(jrg): when doing the above, make sure we support multiple
  # output directories (e.g. browser/jni and common/jni if needed).
  'sources': [
    'public/android/java/src/org/chromium/content/app/ChildProcessServiceImpl.java',
    'public/android/java/src/org/chromium/content/app/ContentMain.java',
    'public/android/java/src/org/chromium/content/browser/accessibility/BrowserAccessibilityManager.java',
    'public/android/java/src/org/chromium/content/browser/BackgroundSyncNetworkObserver.java',
    'public/android/java/src/org/chromium/content/browser/BrowserStartupController.java',
    'public/android/java/src/org/chromium/content/browser/ChildProcessLauncher.java',
    'public/android/java/src/org/chromium/content/browser/ContentVideoView.java',
    'public/android/java/src/org/chromium/content/browser/ContentViewCore.java',
    'public/android/java/src/org/chromium/content/browser/ContentViewRenderView.java',
    'public/android/java/src/org/chromium/content/browser/ContentViewStatics.java',
    'public/android/java/src/org/chromium/content/browser/DeviceSensors.java',
    'public/android/java/src/org/chromium/content/browser/input/HandleViewResources.java',
    'public/android/java/src/org/chromium/content/browser/input/ImeAdapter.java',
    'public/android/java/src/org/chromium/content/browser/input/DateTimeChooserAndroid.java',
    'public/android/java/src/org/chromium/content/browser/input/PopupTouchHandleDrawable.java',
    'public/android/java/src/org/chromium/content/browser/InterstitialPageDelegateAndroid.java',
    'public/android/java/src/org/chromium/content/browser/LocationProviderAdapter.java',
    'public/android/java/src/org/chromium/content/browser/MediaSessionDelegate.java',
    'public/android/java/src/org/chromium/content/browser/MediaResourceGetter.java',
    'public/android/java/src/org/chromium/content/browser/MediaThrottler.java',
    'public/android/java/src/org/chromium/content/browser/MotionEventSynthesizer.java',
    'public/android/java/src/org/chromium/content/browser/ServiceRegistrar.java',
    'public/android/java/src/org/chromium/content/browser/ServiceRegistry.java',
    'public/android/java/src/org/chromium/content/browser/ScreenOrientationProvider.java',
    'public/android/java/src/org/chromium/content/browser/SpeechRecognition.java',
    'public/android/java/src/org/chromium/content/browser/TimeZoneMonitor.java',
    'public/android/java/src/org/chromium/content/browser/TracingControllerAndroid.java',
    'public/android/java/src/org/chromium/content/browser/framehost/NavigationControllerImpl.java',
    'public/android/java/src/org/chromium/content/browser/webcontents/WebContentsImpl.java',
    'public/android/java/src/org/chromium/content/browser/webcontents/WebContentsObserverProxy.java',
    'public/android/java/src/org/chromium/content_public/browser/LoadUrlParams.java',
    'public/android/java/src/org/chromium/content_public/common/MediaMetadata.java',
    'public/android/java/src/org/chromium/content_public/common/ResourceRequestBody.java',
   ],
  'variables': {
    'jni_gen_package': 'content',
  },
  'includes': [ '../build/jni_generator.gypi' ],
}
