# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'defines': [
      'DEQP_TARGET_NAME="chrome-gpu-command-buffer"',
      'DEQP_SUPPORT_GLES2=1',
      'DEQP_SUPPORT_EGL=1',
      'GTF_API=GTF_GLES20',
    ],
    'conditions': [
      ['OS=="linux"', {
        'defines': [
          '_XOPEN_SOURCE=500',
        ],
        'cflags!': [
          '-fno-exceptions',
        ],
        'cflags_cc!': [
          '-fno-exceptions',
        ],
        'target_conditions': [
          ['_type=="static_library"', {
            'cflags_cc!': [
              '-fno-rtti',
            ],
          }],
        ],
      }],
    ],
  },
}
