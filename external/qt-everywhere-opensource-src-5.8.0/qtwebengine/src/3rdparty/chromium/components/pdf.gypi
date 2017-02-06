# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [{
    'target_name': 'pdf_common',
    'type': 'static_library',
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/content/content.gyp:content_common',
      '<(DEPTH)/ipc/ipc.gyp:ipc',
      '<(DEPTH)/url/url.gyp:url_lib',
      '<(DEPTH)/url/ipc/url_ipc.gyp:url_ipc',
    ],
    'sources': [
      'pdf/common/pdf_message_generator.cc',
      'pdf/common/pdf_message_generator.h',
      'pdf/common/pdf_messages.h',
    ],
  }, {
    'target_name': 'pdf_browser',
    'type': 'static_library',
    'dependencies': [
      '<(DEPTH)/content/content.gyp:content_browser',
      'pdf_common',
    ],
    'sources': [
      'pdf/browser/open_pdf_in_reader_prompt_client.h',
      'pdf/browser/pdf_web_contents_helper.cc',
      'pdf/browser/pdf_web_contents_helper.h',
      'pdf/browser/pdf_web_contents_helper_client.h',
    ],
  }, {
    'target_name': 'pdf_renderer',
    'type': 'static_library',
    'dependencies': [
      '<(DEPTH)/content/content.gyp:content_renderer',
      '<(DEPTH)/gin/gin.gyp:gin',
      '<(DEPTH)/ppapi/ppapi_internal.gyp:ppapi_shared',
      '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
      '<(DEPTH)/v8/src/v8.gyp:v8',
      '<(DEPTH)/third_party/WebKit/public/blink.gyp:blink',
      'components_resources.gyp:components_resources',
      'components_strings.gyp:components_strings',
      'pdf_common',
    ],
    'sources': [
      'pdf/renderer/pdf_accessibility_tree.cc',
      'pdf/renderer/pdf_accessibility_tree.h',
      'pdf/renderer/pepper_pdf_host.cc',
      'pdf/renderer/pepper_pdf_host.h',
    ],
    'conditions': [
      ['OS=="win"', {
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
      ],
    ],
  }],
}
