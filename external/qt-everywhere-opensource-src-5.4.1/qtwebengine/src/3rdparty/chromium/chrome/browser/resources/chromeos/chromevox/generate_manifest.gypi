# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates an output manifest based on a Jinja2 templated manifest.
# Include this file inside of your target to generate a manifest.
# The following variables must be set before including this file:
#
# template_manifest_path: a valid Jinja2 file path.
# output_manifest_path: file path for the resulting manifest.
#
# The following variable is optional:
#
# guest_manifest: 1 or 0; generates a manifest usable while in guest
# mode.

{
  'variables': {
    'generate_manifest_script_path': 'tools/generate_manifest.py',
    'is_guest_manifest%': 0,
  },
  'actions': [
    {
      'action_name': 'generate_manifest',
      'message': 'Generate manifest for <(_target_name)',
      'inputs': [
        '<(generate_manifest_script_path)',
        '<(template_manifest_path)',
      ],
      'outputs': [
        '<(output_manifest_path)'
      ],
      'action': [
        'python',
        '<(generate_manifest_script_path)',
        '--is_guest_manifest=<(is_guest_manifest)',
        '--use_chromevox_next=<(use_chromevox_next)',
        '-o', '<(output_manifest_path)',
        '<(template_manifest_path)',
      ],
    },
  ],
}
