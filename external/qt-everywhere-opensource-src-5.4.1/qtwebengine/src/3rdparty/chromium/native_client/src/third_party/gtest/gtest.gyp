# -*- python -*-
# Copyright 2012, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'target_defaults': {
    'variables': {
      'target_base': 'none',
    },
    'target_conditions': [
      ['target_base=="gtest"', {
        'include_dirs': [
          '<(DEPTH)/testing/gtest/',
          '<(DEPTH)/testing/gtest/include',
        ],
        'cflags_cc!': [
          '-fno-rtti',
          '-Wno-missing-field-initializers'
        ],
        'cflags!': [
          '-pedantic',
          '-Wswitch-enum',
          '-Wno-missing-field-initializers'
        ],
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            '-pedantic',
            '-Wswitch-enum'
          ]
        },
        'sources': [
          '<(DEPTH)/testing/gtest/src/gtest-all.cc',
        ],
        'conditions': [
          ['OS=="mac"', {
            'xcode_settings': {
              'GCC_ENABLE_CPP_RTTI': 'YES',   # override -fno-rtti
            },
          }],
        ],
      }]
    ],
  },
  'targets': [
    {
      'target_name': 'gtest',
      'type': 'static_library',
      'variables': {
        'target_base': 'gtest',
      },
    },
  ],
  'conditions': [
    ['OS=="win" and target_arch=="ia32"', {
      'targets': [
        {
          'target_name': 'gtest64',
          'type': 'static_library',
          'variables': {
            'target_base': 'gtest',
            'win_target': 'x64',
          },
        },
      ],
    }],
  ],
}
