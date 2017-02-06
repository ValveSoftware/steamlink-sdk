# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
  ],
  'conditions': [
    ['OS=="mac" or (OS=="ios" and "<(GENERATOR)"=="ninja")', {
      'targets' : [
        {
          'target_name' : 'libudis86',
          'type': 'static_library',
          'toolsets': ['host', 'target'],
          'defines': [
            'HAVE_ASSERT_H',
            'HAVE_STRING_H',
          ],
          'sources': [
            'libudis86/decode.c',
            'libudis86/decode.h',
            'libudis86/extern.h',
            'libudis86/input.c',
            'libudis86/input.h',
            'libudis86/itab.c',
            'libudis86/itab.h',
            'libudis86/syn-att.c',
            'libudis86/syn-intel.c',
            'libudis86/syn.c',
            'libudis86/syn.h',
            'libudis86/types.h',
            'libudis86/udint.h',
            'libudis86/udis86.c',
            'udis86.h',
          ],
          'sources!': [
            # The syn* files implement formatting for output, which is unused
            # by mach_override. Normally, it would be possible to let dead
            # code stripping get rid of them, but syn.c contains errors.
            # Rather than patching a file that's not relevant, disable it.
            'libudis86/syn-att.c',
            'libudis86/syn-intel.c',
            'libudis86/syn.c',
          ],
          'variables': {
            'clang_warning_flags_unset': [
              # For UD_ASSERT(!"message");
              '-Wstring-conversion',
            ],
            'clang_warning_flags': [
              # syn.c contains a switch with an assert(false) in a default:
              # block.  In release builds, the function is missing a return.
              '-Wno-return-type',
              # Fires once in decode.c.
              '-Wno-sometimes-uninitialized',
            ],
          },
        },
        {
          'target_name' : 'mach_override',
          'type': 'static_library',
          'toolsets': ['host', 'target'],
          'dependencies': [
            'libudis86',
          ],
          'sources': [
            'mach_override.c',
            'mach_override.h',
          ],
          'include_dirs': [
            '../..',
          ],
        },
      ],
    }],
  ],
}
