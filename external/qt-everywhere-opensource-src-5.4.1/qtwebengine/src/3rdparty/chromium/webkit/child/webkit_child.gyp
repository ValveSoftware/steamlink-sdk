# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'conditions': [
    ['OS=="android"',
      {
        'targets': [
          {
            'target_name': 'overscroller_jni_headers',
            'type': 'none',
            'variables': {
              'jni_gen_package': 'webkit',
              'input_java_class': 'android/widget/OverScroller.class',
            },
            'includes': [ '../../build/jar_file_jni_generator.gypi' ],
          },
        ],
      }
    ],
  ],
  'targets': [
    {
      'target_name': 'webkit_child',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'defines': [
        'WEBKIT_CHILD_IMPLEMENTATION',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/base/base.gyp:base_i18n',
        '<(DEPTH)/base/base.gyp:base_static',
        '<(DEPTH)/base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/ui/native_theme/native_theme.gyp:native_theme',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
        '<(DEPTH)/webkit/common/webkit_common.gyp:webkit_common',
      ],
      'include_dirs': [
        # For JNI generated header.
        '<(SHARED_INTERMEDIATE_DIR)/webkit',
      ],
      # This target exports a hard dependency because dependent targets may
      # include the header generated above.
      'hard_dependency': 1,
      'sources': [
        'multipart_response_delegate.cc',
        'multipart_response_delegate.h',
        'resource_loader_bridge.cc',
        'resource_loader_bridge.h',
        'webkit_child_export.h',
        'weburlresponse_extradata_impl.cc',
        'weburlresponse_extradata_impl.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267 ],
      'conditions': [
        ['OS=="mac"',
          {
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
              ],
            },
          }
        ],
        ['OS=="android"',
          {
            'dependencies': [
              'overscroller_jni_headers',
            ],
          }
        ],
      ],
    },
  ],
}
