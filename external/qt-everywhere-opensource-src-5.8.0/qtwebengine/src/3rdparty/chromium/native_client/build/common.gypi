# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # .gyp files should set chromium_code to 1 if they build Chromium-specific
    # code, as opposed to external code.  This variable is used to control
    # such things as the set of warnings to enable, and whether warnings are
    # treated as errors.
    'chromium_code%': 0,

    # Variables expected to be overriden on the GYP command line (-D) or by
    # ~/.gyp/include.gypi.

    # Doing this in a sub-dict so that it can be referred to below.
    'variables': {
      # By default we assume that we are building as part of Chrome
      'variables': {
        'nacl_standalone%': 0,
      },
      'nacl_standalone%': '<(nacl_standalone)',
    },
    # These come from the above variable scope.
    'nacl_standalone%': '<(nacl_standalone)',

    'linux2%': 0,
  },

  'target_defaults': {
    'include_dirs': [
      # Putting this first so that people who include "ppapi/..." get the
      # third_party version instead of any other version (the chrome location
      # for instance).
      '../src/third_party',
      '../..',
    ],
    'variables': {
      'win_target': 'x32',
    },
    'target_conditions': [
      ['win_target=="x64"', {
        'target_arch': 'x64',
        'configurations': {
          'Common_Base': {
            'msvs_target_platform': 'x64',
          },
        },
      }],
    ],
    'conditions': [
      ['linux2==1', {
        'defines': ['LINUX2=1'],
      }],
      ['OS=="win"', {
        'defines': [ 'NOMINMAX' ]
      }],
    ],
  },
  'conditions': [
    ['OS=="linux" or OS=="android"', {
      'target_defaults': {
        'defines': [
          '_POSIX_C_SOURCE=199506',
          '_XOPEN_SOURCE=600',
          '_GNU_SOURCE=1',
          '__STDC_LIMIT_MACROS=1',
        ],
      },
    }],
    ['OS=="win"', {
      'target_defaults': {
        'variables': {
          'windows_asm_rule%': 1,
        },
        'target_conditions': [
          ['windows_asm_rule==1', {
            'rules': [
              {
                'rule_name': 'assembler (gnu-compatible)',
                'msvs_cygwin_shell': 0,
                'msvs_quote_cmd': 0,
                'extension': 'S',
                'inputs': [
                  '<(DEPTH)/native_client/tools/win_as.py',
                ],
                'outputs': [
                  '$(IntDir)/$(InputName).obj',
                ],
                'action': [
                   'python',
                   '<(DEPTH)/native_client/tools/win_as.py',
                   # target architecture: Win32 or x64
                   '-a', '$(PlatformName)',
                   # output path
                   '-o', '$(IntDir)/$(InputName).obj',
                   # path to top of tree, e.g. svn/nacl
                   '-p', '<(DEPTH)',
                   # .S file
                  '$(InputPath)'
                ],
                'message': 'Building assembly language file $(InputPath)',
                'process_outputs_as_sources': 1,
              },
            ],
          }],
        ],
        'defines': [
          '__STDC_LIMIT_MACROS=1',
        ],
        'conditions': [
          ['MSVS_VERSION=="2008"', {
            'defines': [
              '_HAS_TR1=0',
            ],
          }],
        ],
        'msvs_cygwin_dirs': ['../third_party/cygwin'],
        # TODO(bsy) remove 4355 once cross-repo
        # NACL_ALLOW_THIS_IN_INITIALIZER_LIST changes go in.
        'msvs_disabled_warnings': [4355, 4396, 4503, 4800, 4819],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'WarningLevel': '3',
          },
          'VCLinkerTool': {
            'AdditionalOptions': [
              '/safeseh:NO',
              '/dynamicbase:NO',
              '/ignore:4199',
              '/ignore:4221',
              '/nxcompat',
            ],
            'AdditionalDependencies': [
              'advapi32.lib',
            ],
            'DelayLoadDLLs': [
              'dbghelp.dll',
              'dwmapi.dll',
              'uxtheme.dll',
            ],
            'ImportLibrary': '$(OutDir)\\lib\\$(TargetName).lib',
            'TargetMachine': '1',
          },
        },
      },
    }],
    ['chromium_code==0 and nacl_standalone==0', {
      # This section must follow the other conditon sections above because
      # external_code.gypi expects to be merged into those settings.
      'includes': [
        'external_code.gypi',
      ],
    }, {
      'target_defaults': {
        # In Chromium code, we define __STDC_FORMAT_MACROS in order to get the
        # C99 macros on Mac and Linux.
        'defines': [
          '__STDC_FORMAT_MACROS',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Detect64BitPortabilityProblems': 'false',
            # TODO(new_hire): above line should go away
          },
        },
      },
    }],
  ],
  'includes': [
    'untrusted.gypi',
  ],
}
