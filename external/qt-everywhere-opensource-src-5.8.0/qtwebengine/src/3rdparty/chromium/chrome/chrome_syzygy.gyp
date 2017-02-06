# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'dest_dir': '<(PRODUCT_DIR)/syzygy',
  },
  'conditions': [
    ['syzyasan==1', {
      'variables': {
        'syzygy_exe_dir': '<(DEPTH)/third_party/syzygy/binaries/exe',
      },
      # Copy the SyzyASan runtime and logger to the syzygy directory.
      'targets': [
        {
          'target_name': 'copy_syzyasan_binaries',
          'type': 'none',
          'outputs': [
            '<(dest_dir)/agent_logger.exe',
            '<(dest_dir)/syzyasan_rtl.dll',
            '<(dest_dir)/syzyasan_rtl.dll.pdb',
          ],
          'copies': [
            {
              'destination': '<(dest_dir)',
              'files': [
                '<(syzygy_exe_dir)/agent_logger.exe',
                '<(syzygy_exe_dir)/minidump_symbolizer.py',
                '<(syzygy_exe_dir)/syzyasan_rtl.dll',
                '<(syzygy_exe_dir)/syzyasan_rtl.dll.pdb',
              ],
            },
          ],
        },
      ],
    }],
    ['OS=="win" and fastbuild==0', {
      'conditions': [
        ['syzygy_optimize==1 or syzyasan==1', {
          'variables': {
            'dll_name': 'chrome',
          },
          'targets': [
            # GN version: //chrome/tools/build/win/syzygy:chrome_dll_syzygy
            {
              'target_name': 'chrome_dll_syzygy',
              'type': 'none',
              'sources' : [],
              'includes': [
                'chrome_syzygy.gypi',
              ],
            },
          ],
        }],
        ['chrome_multiple_dll==1', {
          'conditions': [
            ['syzyasan==1 or syzygy_optimize==1', {
              'variables': {
                'dll_name': 'chrome_child',
              },
              'targets': [
                # GN version: //chrome/tools/build/win/syzygy:chrome_child_dll_syzygy
                {
                  'target_name': 'chrome_child_dll_syzygy',
                  'type': 'none',
                  # For the official SyzyASan builds just copy chrome_child.dll
                  # to the Syzygy directory.
                  'conditions': [
                    ['syzyasan==1 and buildtype=="Official"', {
                      'dependencies': [
                        'chrome_child_dll_syzygy_copy'
                      ],
                    }],
                  ],
                  # For the official SyzyASan builds also put an instrumented
                  # version of chrome_child.dll into syzygy/instrumented.
                  'variables': {
                    'conditions': [
                      ['syzyasan==1 and buildtype=="Official"', {
                        'dest_dir': '<(PRODUCT_DIR)/syzygy/instrumented',
                      }],
                    ],
                  },
                  'sources' : [],
                  'includes': [
                    'chrome_syzygy.gypi',
                  ],
                },
              ],
            }],
            # For the official SyzyASan builds just copy chrome_child.dll to the
            # Syzygy directory.
            ['syzyasan==1 and buildtype=="Official"', {
              'targets': [
              {
                # GN version: //chrome/tools/build/win/syzygy:chrome_child_dll_syzygy_copy
                'target_name': 'chrome_child_dll_syzygy_copy',
                'type': 'none',
                'inputs': [
                  '<(PRODUCT_DIR)/chrome_child.dll',
                  '<(PRODUCT_DIR)/chrome_child.dll.pdb',
                ],
                'outputs': [
                  '<(dest_dir)/chrome_child.dll',
                  '<(dest_dir)/chrome_child.dll.pdb',
                ],
                'copies': [
                  {
                    'destination': '<(dest_dir)',
                    'files': [
                      '<(PRODUCT_DIR)/chrome_child.dll',
                      '<(PRODUCT_DIR)/chrome_child.dll.pdb',
                    ],
                  },
                ],
              }],
            }],
          ],
        }],
      ],
    }],
  ],
}
