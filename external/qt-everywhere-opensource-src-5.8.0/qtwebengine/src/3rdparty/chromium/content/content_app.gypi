# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'include_dirs': [
    '..',
  ],
  'dependencies': [
    '../base/base.gyp:base',
    '../base/base.gyp:base_i18n',
    '../crypto/crypto.gyp:crypto',
    '../mojo/mojo_edk.gyp:mojo_system_impl',
    '../ui/base/ui_base.gyp:ui_base',
    '../ui/gfx/gfx.gyp:gfx',
    '../ui/gfx/gfx.gyp:gfx_geometry',
    'content_common_mojo_bindings.gyp:content_common_mojo_bindings',
  ],
  'export_dependent_settings': [
    'content_common_mojo_bindings.gyp:content_common_mojo_bindings',
  ],
  'sources': [
    'app/android/app_jni_registrar.cc',
    'app/android/app_jni_registrar.h',
    'app/android/child_process_service_impl.cc',
    'app/android/child_process_service_impl.h',
    'app/android/content_jni_onload.cc',
    'app/android/content_main.cc',
    'app/android/content_main.h',
    'app/android/download_main.cc',
    'app/android/library_loader_hooks.cc',
    'app/android/library_loader_hooks.h',
    'app/content_main.cc',
    'app/content_main_runner.cc',
    'app/mac/mac_init.mm',
    'app/mac/mac_init.h',
    'app/mojo/mojo_init.cc',
    'app/mojo/mojo_init.h',
    'public/app/content_jni_onload.h',
    'public/app/content_main.h',
    'public/app/content_main_delegate.cc',
    'public/app/content_main_delegate.h',
    'public/app/content_main_runner.h',
  ],
  'conditions': [
    ['OS=="android"', {
      'sources!': [
        'app/content_main.cc',
      ],
      'dependencies': [
        'content.gyp:content_jni_headers',
        '../build/android/ndk.gyp:cpu_features',
        '../device/usb/usb.gyp:device_usb',
        '../gpu/gpu.gyp:gpu_ipc_common',
        '../skia/skia.gyp:skia',
        '../ui/android/ui_android.gyp:ui_android',
      ],
    }],
    ['OS=="win"', {
      'dependencies': [
        'content.gyp:sandbox_helper_win',
      ],
    }],
  ],
}
