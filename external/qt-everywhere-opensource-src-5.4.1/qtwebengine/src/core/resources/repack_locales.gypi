# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'repack_extra_flags%': [],
    'repack_output_dir%': '<(SHARED_INTERMEDIATE_DIR)',
    'repack_locales_script%': ['python', '<(qtwebengine_root)/tools/buildscripts/repack_locales.py'],
  },
  'inputs': [
    '<(qtwebengine_root)/tools/buildscripts/repack_locales.py',
    '<!@pymod_do_main(repack_locales -i -p <(OS) -s <(SHARED_INTERMEDIATE_DIR) -x <(repack_output_dir) <(repack_extra_flags) <(locales))'
  ],
  'outputs': [
    '<@(locale_files)'
  ],
  'action': [
    '<@(repack_locales_script)',
    '-p', '<(OS)',
    '-s', '<(SHARED_INTERMEDIATE_DIR)',
    '-x', '<(repack_output_dir)/.',
    '<@(repack_extra_flags)',
    '<@(locales)',
  ],
}
