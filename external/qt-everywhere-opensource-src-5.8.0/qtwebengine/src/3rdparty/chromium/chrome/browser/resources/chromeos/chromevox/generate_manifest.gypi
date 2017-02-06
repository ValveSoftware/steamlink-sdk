# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates an output manifest based on a Jinja2 templated manifest.
# Include this file inside of your target to generate a manifest.
# The following variables must be set before including this file:
#
# template_manifest_path: a valid Jinja2 file path.
# output_manifest_path: file path for the resulting manifest.
# chromevox_extension_key: The extension key to include in the manifest.
#
# The following variables are optional:
#
# is_guest_manifest: 1 or 0; generates a manifest usable while in guest
# mode.
# chromevox_compress_js: 1 or 0; whether the javascript is compressed.

{
  'variables': {
    'generate_manifest_script_path': 'tools/generate_manifest.py',
    'is_guest_manifest%': 0,
  },
  'includes': [
    '../../../../../build/util/version.gypi',
  ],
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
        '--key=<(chromevox_extension_key)',
        '--is_js_compressed=<(chromevox_compress_js)',
        '--set_version=<(version_full)',
        '--output_manifest=<(output_manifest_path)',
        '<(template_manifest_path)',
      ],
    },
  ],
}
