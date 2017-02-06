# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This is shared between system_webview_glue_java and system_webview_glue_next_java
{
  'type': 'none',
  'dependencies': [
    '<(DEPTH)/android_webview/android_webview.gyp:android_webview_java',
    '<(DEPTH)/android_webview/android_webview.gyp:android_webview_pak',
  ],
  'variables': {
    'resource_rewriter_package': 'com.android.webview.chromium',
    'resource_rewriter_dir': '<(intermediate_dir)/resource_rewriter',
    'resource_rewriter_path': '<(resource_rewriter_dir)/com/android/webview/chromium/ResourceRewriter.java',
    'additional_input_paths': ['<(resource_rewriter_path)'],
    'generated_src_dirs': ['<(resource_rewriter_dir)'],
  },
  'actions': [
    # Generate ResourceRewriter.java
    {
      'action_name': 'generate_resource_rewriter',
      'message': 'generate ResourceRewriter for <(_target_name)',
      'inputs':[
        '<(DEPTH)/build/android/gyp/util/build_utils.py',
        '<(DEPTH)/build/android/gyp/generate_resource_rewriter.py',
        '>@(dependencies_res_zip_paths)',
      ],
      'outputs': [
        '<(resource_rewriter_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/gyp/generate_resource_rewriter.py',
        '--package-name', '<(resource_rewriter_package)',
        '--dep-packages', '>(additional_res_packages)',
        '--output-dir', '<(resource_rewriter_dir)',
      ],
    },
  ],
  'includes': [ '../../build/java.gypi' ],
}
