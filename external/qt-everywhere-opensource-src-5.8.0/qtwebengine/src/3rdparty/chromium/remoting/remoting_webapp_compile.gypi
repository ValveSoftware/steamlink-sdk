# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# File in charge of Closure compiling remoting's webapp.

{
  'targets': [
    {
      'target_name': 'verify_remoting_webapp',
      'inputs': [
        'remoting_webapp_compile.gypi',
        'remoting_webapp_files.gypi',
      ],
      'variables': {
        'source_files': [
          '<@(remoting_webapp_crd_js_files)',
          '<@(remoting_webapp_js_proto_files)',
        ],
        'externs': ['<@(remoting_webapp_js_externs_files)'],
        'out_file': '<(PRODUCT_DIR)/<(_target_name)_jscompile.stamp',
      },
      'includes': ['compile_js.gypi'],
    },
    {
      'target_name': 'verify_remoting_webapp_with_browsertests',
      'inputs': [
        'remoting_webapp_compile.gypi',
        'remoting_webapp_files.gypi',
      ],
      'variables': {
        'source_files': [
          '<@(remoting_webapp_crd_js_files)',
          '<@(remoting_webapp_browsertest_all_js_files)',
          '<@(remoting_webapp_browsertest_js_proto_files)',
        ],
        'externs': ['<@(remoting_webapp_js_externs_files)'],
        'out_file': '<(PRODUCT_DIR)/<(_target_name)_bt_jscompile.stamp',
      },
      'includes': ['compile_js.gypi'],
    },
    {
      'target_name': 'verify_remoting_webapp_unittests',
      'inputs': [
        'remoting_webapp_compile.gypi',
        'remoting_webapp_files.gypi',
      ],
      'variables': {
        'source_files': [
          '<@(remoting_webapp_crd_js_files)',
          '<@(remoting_webapp_unittests_all_js_files)',
          '<@(remoting_webapp_unittests_js_proto_files)',
        ],
        'externs': ['<@(remoting_webapp_js_externs_files)'],
        'out_file': '<(PRODUCT_DIR)/<(_target_name)_ut_jscompile.stamp',
      },
      'includes': ['compile_js.gypi'],
    },
  ],
  'includes': ['remoting_webapp_files.gypi'],
}
