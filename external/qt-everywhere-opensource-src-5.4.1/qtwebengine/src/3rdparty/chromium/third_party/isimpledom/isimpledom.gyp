# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'isimpledom',
      'type': 'static_library',
      'variables': {
        'midl_out_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/isimpledom',
      },
      'sources': [
        'ISimpleDOMDocument.idl',
        'ISimpleDOMNode.idl',
        'ISimpleDOMText.idl',
        '<(midl_out_dir)/ISimpleDOMDocument.h',
        '<(midl_out_dir)/ISimpleDOMDocument_i.c',
        '<(midl_out_dir)/ISimpleDOMNode.h',
        '<(midl_out_dir)/ISimpleDOMNode_i.c',
        '<(midl_out_dir)/ISimpleDOMText.h',
        '<(midl_out_dir)/ISimpleDOMText_i.c',
      ],
      'hard_dependency': 1,
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'msvs_settings': {
        'VCMIDLTool': {
          'GenerateTypeLibrary': 'false',
          'OutputDirectory': '<(midl_out_dir)',
        },
      },
    },
  ],
}
