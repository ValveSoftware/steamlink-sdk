# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'externs': ['../third_party/closure_compiler/externs/chrome_extensions.js',
      '../third_party/closure_compiler/externs/metrics_private.js'],
    'script_args': ['--no-single-file'],
    'closure_args': [
      'jscomp_error=duplicate',
      'jscomp_error=misplacedTypeAnnotation',
    ],
    'disabled_closure_args': [],
  },
  'includes': ['../third_party/closure_compiler/compile_js.gypi'],
}
