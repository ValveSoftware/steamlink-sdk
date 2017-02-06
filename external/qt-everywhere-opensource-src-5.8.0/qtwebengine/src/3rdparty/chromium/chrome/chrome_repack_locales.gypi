# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# To use this the following variables need to be defined:
#   pak_locales: string: the list of all the locales that need repacking
{
  'variables': {
    'repack_locales_path': 'tools/build/repack_locales.py',
    'repack_options%': [],
    'branding_flag': ['-b', '<(branding_path_component)',],
    'conditions': [
      ['chromeos==1', {
        'chromeos_flag': ['--chromeos=1'],
      }, {
        'chromeos_flag': ['--chromeos=0'],
      }],
    ],
    'repack_extra_flags%': [],
    'repack_shared_dir%': '<(SHARED_INTERMEDIATE_DIR)',
    'repack_output_dir%': '<(SHARED_INTERMEDIATE_DIR)',
  },
  'inputs': [
    '<(repack_locales_path)',
    '<!@pymod_do_main(repack_locales -i -p <(OS) <(branding_flag) -g <(grit_out_dir) -s <(repack_shared_dir) -x <(repack_output_dir) --use-ash <(use_ash) <(repack_extra_flags) <(chromeos_flag) --enable-extensions <(enable_extensions) <(pak_locales))'
  ],
  'outputs': [
    '<!@pymod_do_main(repack_locales -o -p <(OS) -g <(grit_out_dir) -s <(repack_shared_dir) -x <(repack_output_dir) <(pak_locales))'
  ],
  'action': [
    'python',
    '<(repack_locales_path)',
    '<@(branding_flag)',
    '-p', '<(OS)',
    '-g', '<(grit_out_dir)',
    '-s<(repack_shared_dir)',
    '-x<(repack_output_dir)/.',
    '--use-ash=<(use_ash)',
    '<@(repack_extra_flags)',
    '<@(chromeos_flag)',
    '--enable-extensions=<(enable_extensions)',
    '<@(repack_options)',
    '<@(pak_locales)',
  ],
}
