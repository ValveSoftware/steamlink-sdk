# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'assert',
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'cr',
      'variables': {
        'depends': ['compiled_resources.gyp:promise_resolver'],
        'externs': ['../../../../third_party/closure_compiler/externs/chrome_send.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'event_tracker',
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'i18n_template_no_process',
      'variables': {
        'depends': ['compiled_resources.gyp:load_time_data'],
        'externs': ['../../../../third_party/closure_compiler/externs/pending_compiler_externs.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'i18n_template',
      'variables': {
        'depends': ['compiled_resources.gyp:load_time_data'],
        'externs': ['../../../../third_party/closure_compiler/externs/pending_compiler_externs.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'load_time_data',
      'variables': {
        'depends': [
          '../../../../third_party/jstemplate/util.js',
          '../../../../third_party/jstemplate/jsevalcontext.js',
          '../../../../third_party/jstemplate/jstemplate.js',
        ],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'parse_html_subset',
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'promise_resolver',
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
    {
      'target_name': 'util',
      'variables': {
        'depends': ['compiled_resources.gyp:cr', 'assert.js'],
        # TODO(jlklein): Get <(VARIABLES) in transient externs/depends working.
        'externs': ['../../../../third_party/closure_compiler/externs/chrome_send.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
