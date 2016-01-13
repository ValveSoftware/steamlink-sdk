# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'midl_out_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/iaccessible2',
  },
  'targets': [
    {
      'target_name': 'iaccessible2',
      'type': 'static_library',
      'sources': [
        'ia2_api_all.idl',
        '<(midl_out_dir)/ia2_api_all.h',
        '<(midl_out_dir)/ia2_api_all_i.c',
      ],
      'hard_dependency': 1,
      'msvs_settings': {
        'VCMIDLTool': {
          'OutputDirectory': '<(midl_out_dir)',
          'DLLDataFileName': 'dlldata.c',
         },
      },
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
    {
      'target_name': 'IAccessible2Proxy',
      'type': 'shared_library',
      'defines': [ 'REGISTER_PROXY_DLL' ],
      'dependencies': [ 'iaccessible2' ],
      'sources': [
        'IAccessible2Proxy.def',
        '<(midl_out_dir)/dlldata.c',
        '<(midl_out_dir)/ia2_api_all_p.c',
      ],
      'link_settings': {
        'libraries': [
          '-lrpcrt4.lib',
        ],
      },
    },
  ],
}
