# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['chromeos==1', {
      'variables': {
        # Whether to compress the 4 main ChromeVox scripts.  Applicable if
        # use_migrated_chromevox is true.
        'chromevox_compress_js%': '1',
        'background_script_loader_file': 'chromevox/background/loader.js',
        'content_script_loader_file': 'chromevox/injected/loader.js',
        'options_script_loader_file': 'chromevox/background/options_loader.js',
        'kbexplorer_loader_file': 'chromevox/background/kbexplorer_loader.js',
      },
      'includes': [
        'chromevox_tests.gypi',
        'common.gypi',
      ],
      'targets': [
        {
          'target_name': 'chromevox',
          'type': 'none',
          'dependencies': [
            'chromevox_resources',
            'chromevox_manifest',
            'chromevox_guest_manifest',
          ],
        },
        {
          'target_name': 'chromevox_resources',
          'type': 'none',
          'dependencies': [
            'chromevox_assets',
            'chromevox_static_files',
            'chromevox_strings',
            'chromevox_uncompiled_js_files',
            '<(chromevox_third_party_dir)/chromevox.gyp:chromevox_third_party_resources',
            '../braille_ime/braille_ime.gyp:braille_ime_manifest',
          ],
          'conditions': [
            ['disable_nacl==0 and disable_nacl_untrusted==0', {
              'dependencies': [
                '<(DEPTH)/third_party/liblouis/liblouis_nacl.gyp:liblouis_nacl_wrapper_nacl',
              ],
            }],
            ['use_migrated_chromevox==1 and chromevox_compress_js==1', {
              'dependencies': [
                'chromevox_content_script',
                'chromevox_background_script',
                'chromevox_options_script',
                'chromevox_kbexplorer_script',
              ],
            }],
            ['use_migrated_chromevox==1 and chromevox_compress_js==0', {
              'dependencies': [
                'chromevox_copied_scripts',
              ],
            }],
          ],
        },
        {
          'target_name': 'chromevox_assets',
          'type': 'none',
          'includes': [
            'chromevox_assets.gypi',
          ],
        },
        {
          'target_name': 'chromevox_manifest',
          'type': 'none',
          'variables': {
            'output_manifest_path': '<(chromevox_dest_dir)/manifest.json',
          },
          'includes': [ 'generate_manifest.gypi', ],
        },
        {
          'target_name': 'chromevox_guest_manifest',
          'type': 'none',
          'variables': {
            'output_manifest_path': '<(chromevox_dest_dir)/manifest_guest.json',
            'is_guest_manifest': 1,
          },
          'includes': [ 'generate_manifest.gypi', ],
        },
        {
          'target_name': 'chromevox_static_files',
          'type': 'none',
          'copies': [
            {
              'destination': '<(chromevox_dest_dir)/chromevox/background',
              'files': [
                'chromevox/background/background.html',
                'chromevox/background/kbexplorer.html',
                'chromevox/background/options.html',
              ],
            },
          ],
        },
        {
          # JavaScript files that are always directly included into the
          # destination directory.
          'target_name': 'chromevox_uncompiled_js_files',
          'type': 'none',
          'copies': [
            {
              'destination': '<(chromevox_dest_dir)/closure',
              'files': [
                'closure/closure_preinit.js',
              ],
              'conditions': [
                ['use_migrated_chromevox==0 or chromevox_compress_js==1', {
                  'files': [ '<(closure_goog_dir)/base.js' ],
                }],
              ]
            },
            {
              'destination': '<(chromevox_dest_dir)/chromevox/injected',
              'files': [
                'chromevox/injected/api.js',
                'chromevox/injected/api_util.js',
              ],
            },
          ],
        },
        {
          'target_name': 'chromevox_strings',
          'type': 'none',
          'actions': [
            {
              'action_name': 'chromevox_strings',
              'variables': {
                'grit_grd_file': 'strings/chromevox_strings.grd',
                'grit_out_dir': '<(chromevox_dest_dir)',
                # We don't generate any RC files, so no resource_ds file is needed.
                'grit_resource_ids': '',
              },
              'includes': [ '../../../../../build/grit_action.gypi' ],
            },
          ],
        },
      ],
      'conditions': [
        ['use_migrated_chromevox==1 and chromevox_compress_js==1', {
          'targets': [
            {
              'target_name': 'chromevox_content_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxChromePageScript.js',
              },
              'sources': [ '<(content_script_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox_background_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxChromeBackgroundScript.js',
              },
              'sources': [ '<(background_script_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox_options_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxChromeOptionsScript.js',
              },
              'sources': [ '<(options_script_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox_kbexplorer_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxKbExplorerScript.js',
              },
              'sources': [ '<(kbexplorer_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
          ],
        },
        ],
        ['use_migrated_chromevox==1 and chromevox_compress_js==0', {
          'targets': [
            {
              'target_name': 'chromevox_copied_scripts',
              'type': 'none',
              'variables': {
                'dest_dir': '<(chromevox_dest_dir)',
              },
              'sources': [
                '<(background_script_loader_file)',
                '<(content_script_loader_file)',
                '<(kbexplorer_loader_file)',
                '<(options_script_loader_file)',
              ],
              'includes': [ 'copy_js.gypi', ],
            },
          ],
        }],
      ],
    }],
  ],
}
