# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Shared source lists between trusted and untrusted targets are stored in
# ppapi_sources.gypi.

{
  'includes': [
    'ppapi_sources.gypi',
  ],
  'targets': [
    {
      # GN version: //ppapi/c
      'target_name': 'ppapi_c',
      'type': 'none',
      'all_dependent_settings': {
        'include_dirs+': [
          '..',
        ],
      },
      'sources': [
        '<@(c_source_files)',
      ],
    },
    {
      # GN version: //ppapi/cpp:objects
      'target_name': 'ppapi_cpp_objects',
      'type': 'static_library',
      'dependencies': [
        'ppapi_c'
      ],
      'include_dirs+': [
        '..',
      ],
      'sources': [
        '<@(cpp_source_files)',
      ],
    },
    {
      # GN version: //ppapi/cpp
      'target_name': 'ppapi_cpp',
      'type': 'static_library',
      'dependencies': [
        'ppapi_c',
        'ppapi_cpp_objects',
      ],
      'include_dirs+': [
        '..',
      ],
      'sources': [
        'cpp/module_embedder.h',
        'cpp/ppp_entrypoints.cc',
      ],
    },
    {
      # GN version: //ppapi/cpp/private:internal_module
      'target_name': 'ppapi_internal_module',
      'type': 'static_library',
      'include_dirs+': [
        '..',
      ],
      'sources': [
        'cpp/private/internal_module.cc',
        'cpp/private/internal_module.h',
      ]
    },
  ],
}
