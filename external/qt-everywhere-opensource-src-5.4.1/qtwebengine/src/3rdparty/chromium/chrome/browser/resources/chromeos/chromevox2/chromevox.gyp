# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['chromeos==1 and disable_nacl==0 and disable_nacl_untrusted==0', {
      'includes': [ '../chromevox/common.gypi', ],
      'targets': [
        {
          'target_name': 'chromevox2',
          'type': 'none',
          'dependencies': [
            'chromevox2_copied_scripts',
            'chromevox2_deps',
            'chromevox2_manifest',
            'chromevox2_guest_manifest',
            'chromevox2_resources',
            '../chromevox/chromevox.gyp:chromevox_resources',
          ],
        },
        {
          'target_name': 'chromevox2_copied_scripts',
          'type': 'none',
          'variables': {
            'dest_dir': '<(chromevox_dest_dir)',
            'js_root_flags': [
              '-r', '.',
              '-r', '<(closure_goog_dir)',
            ],
          },
          'sources': [
            'cvox2/background/loader.js',
            'cvox2/injected/loader.js',
          ],
          'includes': [ '../chromevox/copy_js.gypi', ],
        },
        {
          'target_name': 'chromevox2_deps',
          'type': 'none',
          'variables': {
            'deps_js_output_file': '<(chromevox_dest_dir)/deps.js',
          },
          'sources': [
            'cvox2/background/loader.js',
            'cvox2/injected/loader.js',
          ],
          'includes': ['../chromevox/generate_deps.gypi'],
        },
        {
          'target_name': 'chromevox2_resources',
          'type': 'none',
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/cvox2/background',
              'files': [
                'cvox2/background/background.html',
              ],
            },
          ],
        },
        {
          'target_name': 'chromevox2_manifest',
          'type': 'none',
          'variables': {
            'output_manifest_path': '<(chromevox_dest_dir)/manifest.json'
          },
          'includes': [ '../chromevox/generate_manifest.gypi', ],
        },
        {
          'target_name': 'chromevox2_guest_manifest',
          'type': 'none',
          'variables': {
            'output_manifest_path': '<(chromevox_dest_dir)/manifest_guest.json',
            'is_guest_manifest': 1,
          },
          'includes': [ '../chromevox/generate_manifest.gypi', ],
        },
      ],
    }],
  ],
}
