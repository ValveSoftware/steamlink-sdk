# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains an action which can be used to construct a manifest file
# declaring a dependency on other dlls. This manifest can then be merged
# into the manifest of the executable and embedded into it when it is built.

# To use this the following variables need to be defined:
#   version_path: string: path to file containing version data (e.g.
#                 chrome/VERSION).
#   version_py_path: string: path to file containing version script (e.g.
#                    build/util/version.py).

{
  'variables': {
    'template_input_path':
        '<(DEPTH)/chrome/app/version_assembly/chrome_exe_manifest.template',
  },
  'inputs': [
    '<(template_input_path)',
    '<(version_path)',
  ],
  'outputs': [
    '<(SHARED_INTERMEDIATE_DIR)/chrome/app/version_assembly/version_assembly.manifest',
  ],
  'action': [
    'python', '<(version_py_path)',
    '-f', '<(version_path)',
    '<(template_input_path)',
    '<@(_outputs)',
  ],
  'message': 'Generating <@(_outputs)',
}
