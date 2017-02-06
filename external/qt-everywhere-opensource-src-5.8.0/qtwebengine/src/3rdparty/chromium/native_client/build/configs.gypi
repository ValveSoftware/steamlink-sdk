# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      #
      # Abstract base configurations to cover common
      # attributes.
      #
      # Currently only implemented for Windows.
      #
      'Common_Base': {
        'abstract': 1,
        'msvs_configuration_attributes': {
          'OutputDirectory':
              '<(DEPTH)\\native_client\\build\\$(ConfigurationName)',
          'IntermediateDirectory': '$(OutDir)\\obj\\$(ProjectName)',
          'CharacterSet': '1',
        },
      },
      'x86_Base': {
        'abstract': 1,
        'configuration_platform': 'Win32',
        'msvs_settings': {
          'VCLinkerTool': {
            'TargetMachine': '1', # x86 - 32
          },
        },
      },
      'x64_Base': {
        'abstract': 1,
        'msvs_configuration_platform': 'x64',
        'msvs_settings': {
          'VCLinkerTool': {
            'TargetMachine': '17', # x86 - 64
            'AdditionalLibraryDirectories!':
              ['<(DEPTH)/third_party/platformsdk_win7/files/Lib'],
            'AdditionalLibraryDirectories':
              ['<(DEPTH)/third_party/platformsdk_win7/files/Lib/x64'],
          },
          'VCLibrarianTool': {
            'AdditionalLibraryDirectories!':
              ['<(DEPTH)/third_party/platformsdk_win7/files/Lib'],
            'AdditionalLibraryDirectories':
            ['<(DEPTH)/third_party/platformsdk_win7/files/Lib/x64'],
          },
        },
      },
      'Debug_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',
            'PreprocessorDefinitions': ['_DEBUG'],
            'BasicRuntimeChecks': '3',

            'conditions': [
              ['component=="shared_library"', {
                'RuntimeLibrary': '3',  # 3 = /MDd (debug DLL)
              }, {
                'RuntimeLibrary': '1',  # 1 = /MTd (debug static)
              }],
            ],
          },
          # VCLinkerTool LinkIncremental values below:
          #   0 == default
          #   1 == /INCREMENTAL:NO
          #   2 == /INCREMENTAL
          # Debug links incremental, Release does not.
          'VCLinkerTool': {
            'LinkIncremental': '<(msvs_debug_link_incremental)',
          },
          'VCResourceCompilerTool': {
            'PreprocessorDefinitions': ['_DEBUG'],
          },
        },
      },
      'Release_Base': {
        'abstract': 1,
        'msvs_settings': {
          'VCCLCompilerTool': {
            'PreprocessorDefinitions': ['NDEBUG'],
            'Optimization': '2',
            'StringPooling': 'true',

            'conditions': [
              ['component=="shared_library"', {
                'RuntimeLibrary': '2',  # 2 = /MD (nondebug DLL)
              }, {
                'RuntimeLibrary': '0',  # 0 = /MT (nondebug static)
              }],
            ],
          },
          'VCLinkerTool': {
            'LinkIncremental': '1',
            'OptimizeReferences': '2',
            'EnableCOMDATFolding': '2',
            'OptimizeForWindows98': '1',
          },
          'VCResourceCompilerTool': {
            'PreprocessorDefinitions': ['NDEBUG'],
          },
        },
      },
      #
      # Concrete configurations
      #
      #
      'Debug': {
        'conditions': [
          [ 'OS=="mac"', {
            'xcode_settings': {
              'COPY_PHASE_STRIP': 'NO',
              'GCC_OPTIMIZATION_LEVEL': '0',
            }
          }],
          [ 'OS=="win"', {
            # On Windows, the default Debug target is x86
            'inherit_from': ['Common_Base', 'x86_Base', 'Debug_Base']
          }],
        ],
      },
      'Release': {
        'defines': [
          'NDEBUG',
        ],
        'conditions': [
          [ 'OS=="mac"', {
            'xcode_settings': {
              'DEAD_CODE_STRIPPING': 'YES',
            }
          }],
          [ 'OS=="win"', {
            # On Windows, the default Release target is x86
            'inherit_from': ['Common_Base', 'x86_Base', 'Release_Base']
          }],
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'Debug_x64': {
            'inherit_from': ['Common_Base', 'x64_Base', 'Debug_Base'],
          },
          'Release_x64': {
            'inherit_from': ['Common_Base', 'x64_Base', 'Release_Base'],
          },
        }],
      ],
    },
  },
}
