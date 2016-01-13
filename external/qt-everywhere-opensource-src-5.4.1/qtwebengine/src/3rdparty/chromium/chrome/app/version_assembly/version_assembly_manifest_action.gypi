# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains an action which can be used to construct a manifest file
# with the same name as the version directory so that chrome.exe identifies the
# version directory as an assembly. This will be copied over to the version
# directory by the installer script.

# To use this the following variables need to be defined:
#   version_path: string: path to file containing version data (e.g.
#                 chrome/VERSION).
#   version_py_path: string: path to file containing version script (e.g.
#                    build/util/version.py).
#   version_full: string: version string in W.X.Y.Z form.


{
  'variables': {
    'template_input_path':
        '<(DEPTH)/chrome/app/version_assembly/version_assembly_manifest.template',
  },
  'inputs': [
    '<(template_input_path)',
    '<(version_path)',
  ],
  'outputs': [
    '<(PRODUCT_DIR)/<(version_full).manifest',
  ],
  'action': [
    'python', '<(version_py_path)',
    '-f', '<(version_path)',
    '<(template_input_path)',
    '<@(_outputs)',
  ],
  'message': 'Generating <@(_outputs)',
}
