# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['chromeos==1', {
      'variables': {
        # Whether to compress the 4 main ChromeVox scripts.
        'chromevox_compress_js%': '1',
      },
      'includes': [
        'chromevox_assets.gypi',
        'chromevox_tests.gypi',
        'chromevox_vars.gypi',
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
            ['chromevox_compress_js==1', {
              'dependencies': [
                'chromevox_background_script',
                'chromevox_content_script',
                'chromevox_kbexplorer_script',
                'chromevox_min_content_script',
                'chromevox_options_script',
                'chromevox_panel_script',
              ],
            }, {  # chromevox_compress_js==0
              'dependencies': [
                'chromevox_copied_scripts',
                'chromevox_deps',
              ],
            }],
          ],
        },
        {
          'target_name': 'chromevox_static_files',
          'type': 'none',
          'copies': [
            {
              'destination': '<(chromevox_dest_dir)/chromevox/background',
              'files': [
                'chromevox/background/kbexplorer.html',
                'chromevox/background/options.html',
              ],
            },
            {
              'destination': '<(chromevox_dest_dir)/cvox2/background',
              'files': [
                'cvox2/background/background.html',
                'cvox2/background/panel.css',
                'cvox2/background/panel.html',
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
              'destination': '<(chromevox_dest_dir)/chromevox/injected',
              'files': [
                'chromevox/injected/api.js',
              ],
              'conditions': [
                [ 'chromevox_compress_js==1', {
                  'files': [
                    # api_util.js is copied by the chromevox_copied_scripts
                    # target in the non-compressed case.
                    'chromevox/injected/api_util.js',
                  ],
                }],
              ],
            },
          ],
          'conditions': [
            [ 'chromevox_compress_js==0', {
              'copies': [
                {
                  'destination': '<(chromevox_dest_dir)/closure',
                  'files': [
                    'closure/closure_preinit.js',
                  ],
                },
              ],
            }],
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
        {
          'target_name': 'chromevox_deps',
          'type': 'none',
          'variables': {
            'deps_js_output_file': '<(chromevox_dest_dir)/deps.js',
          },
          'sources': [
            '<(chromevox_content_script_loader_file)',
            '<(chromevox_kbexplorer_loader_file)',
            '<(chromevox_options_script_loader_file)',
            '<(chromevox_background_script_loader_file)',
            '<(chromevox_panel_script_loader_file)',
          ],
          'includes': ['generate_deps.gypi'],
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
      ],
      'conditions': [
        ['chromevox_compress_js==1', {
          'targets': [
            {
              'target_name': 'chromevox_content_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxChromePageScript.js',
              },
              'sources': [ '<(chromevox_content_script_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox_options_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxChromeOptionsScript.js',
              },
              'sources': [ '<(chromevox_options_script_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox_kbexplorer_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxKbExplorerScript.js',
              },
              'sources': [ '<(chromevox_kbexplorer_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox_background_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVox2ChromeBackgroundScript.js',
              },
              'sources': [
                '<(chromevox_background_script_loader_file)',
              ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox_panel_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVoxPanelScript.js',
              },
              'sources': [ '<(chromevox_panel_script_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
            {
              'target_name': 'chromevox_min_content_script',
              'type': 'none',
              'variables': {
                'output_file': '<(chromevox_dest_dir)/chromeVox2ChromePageScript.js',
              },
              'sources': [ '<(chromevox_min_content_script_loader_file)' ],
              'includes': [ 'compress_js.gypi', ],
            },
          ],
        }, {  # chromevox_compress_js==0
          'targets': [
            {
              'target_name': 'chromevox_copied_scripts',
              'type': 'none',
              'variables': {
                'dest_dir': '<(chromevox_dest_dir)',
              },
              'sources': [
                '<(chromevox_content_script_loader_file)',
                '<(chromevox_kbexplorer_loader_file)',
                '<(chromevox_options_script_loader_file)',
                '<(chromevox_background_script_loader_file)',
                '<(chromevox_panel_script_loader_file)',
              ],
              'includes': [ 'copy_js.gypi', ],
            },
          ],
        }],
      ],
    }],
  ],
}
