# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'variables': {
      'application_name%': '<(application_name)',
      'application_type%': '<(application_type)',
      'base_manifest%': 'none',
      'packaged_manifests%': []
    },
    'application_type%': '<(application_type)',
    'application_name%': '<(application_name)',
    'base_manifest%': '<(base_manifest)',
    'manifest_collator_script%':
        '<(DEPTH)/mojo/public/tools/manifest/manifest_collator.py',
    'packaged_manifests%': '<(packaged_manifests)',
    'source_manifest%': '<(source_manifest)',
    'conditions': [
      ['application_type=="mojo"', {
        'output_manifest%': '<(PRODUCT_DIR)/Mojo Applications/<(application_name)/manifest.json',
      }, {
        'output_manifest%': '<(PRODUCT_DIR)/<(application_name)_manifest.json',
      }],
      ['base_manifest!="none"', {
        'extra_args%': [
          '--base-manifest=<(base_manifest)',
          '<@(packaged_manifests)',
        ],
      }, {
        'extra_args%': [
          '<@(packaged_manifests)',
        ],
      }]
    ],
  },
  'actions': [{
    'action_name': '<(_target_name)_collation',
    'inputs': [
      '<(manifest_collator_script)',
      '<(source_manifest)',
    ],
    'outputs': [
      '<(output_manifest)',
    ],
    'action': [
      'python',
      '<(manifest_collator_script)',
      '--application-name', '<(application_name)',
      '--parent=<(source_manifest)',
      '--output=<(output_manifest)',
      '<@(extra_args)',
    ],
  }],
}
