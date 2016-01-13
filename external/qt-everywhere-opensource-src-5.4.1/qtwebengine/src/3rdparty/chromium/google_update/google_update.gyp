# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'google_update',
      'type': 'static_library',
      'sources': [
        'google_update_idl.idl',
        '<(SHARED_INTERMEDIATE_DIR)/google_update/google_update_idl.h',
        '<(SHARED_INTERMEDIATE_DIR)/google_update/google_update_idl_i.c',
      ],
      # This target exports a hard dependency because dependent targets may
      # include google_update_idl.h, a generated header.
      'hard_dependency': 1,
      'msvs_settings': {
        'VCMIDLTool': {
          'OutputDirectory': '<(SHARED_INTERMEDIATE_DIR)/google_update',
        },
      },
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
    },
  ],
}
