# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  ######################################################################
  'includes': [
    'build/common.gypi',
  ],
  ######################################################################
  'targets': [
    {
      'target_name': 'hello_world_nexe',
      'type': 'none',
      'dependencies': [
        'src/untrusted/nacl/nacl.gyp:nacl_lib',
        'src/untrusted/irt/irt.gyp:irt_core_nexe'
      ],
      'variables': {
        'nexe_target': 'hello_world',
        'build_glibc': 0,
        'build_newlib': 1,
        'build_pnacl_newlib': 1,
        'translate_pexe_with_build': 1,
        'extra_args': [
          '--strip-all',
        ],
      },
      'sources': [
        'tests/hello_world/hello_world.c',
      ],
    },
    # Compile a file in the output directory to make sure build_nexe can handle
    # generated code.
    {
      'target_name': 'copy_hello_world',
      'type': 'none',
      'copies': [
        {
          'destination': '<(SHARED_INTERMEDIATE_DIR)',
          'files': [
            'tests/hello_world/hello_world.c',
          ],
        },
      ],
    },
    {
      'target_name': 'generated_hello_world_nexe',
      'type': 'none',
      'dependencies': [
        'src/untrusted/nacl/nacl.gyp:nacl_lib',
        'src/untrusted/irt/irt.gyp:irt_core_nexe',
        'copy_hello_world',
      ],
      'variables': {
        'nexe_target': 'generated_hello_world',
        'build_glibc': 0,
        'build_newlib': 1,
        'build_pnacl_newlib': 0,
        'extra_args': [
          '--strip-all',
        ],
      },
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/hello_world.c',
      ],
    },
    # Build simple_thread_test to verify that __thread linkage works
    # correctly with gyp-built libraries:
    # https://code.google.com/p/chromium/issues/detail?id=3461
    {
      'target_name': 'simple_thread_test',
      'type': 'none',
      'dependencies': [
        'src/untrusted/nacl/nacl.gyp:nacl_lib',
        'src/untrusted/irt/irt.gyp:irt_core_nexe'
      ],
      'link_flags': ['-lpthread'],
      # Bug 3461 only occurs when linking -fPIC objects so we use
      # -fPIC here even though it isn't strictly necessary.
      'compile_flags': ['-fPIC'],
      'variables': {
        'nexe_target': 'simple_thread_test',
        'build_glibc': 0,
        'build_newlib': 1,
        'build_pnacl_newlib': 0,
      },
      'sources': [
        'tests/threads/simple_thread_test.c',
      ],
    },
  ],
  'conditions': [
    ['OS!="android" and target_arch!="arm" and target_arch!="mipsel"', {
      'targets': [
        # If the target ABI is not compatible with host ABI,
        # only build the tests, but don't try to run them.
        {
          'target_name': 'test_hello_world_nexe',
          'type': 'none',
          'dependencies': [
            'hello_world_nexe',
            'src/trusted/service_runtime/service_runtime.gyp:sel_ldr',
          ],
          'variables': {
            'arch': '--arch=<(target_arch)',
            'name': '--name=hello_world',
            'path': '--path=<(PRODUCT_DIR)',
            'script': '<(DEPTH)/native_client/build/test_build.py',
            'disable_glibc%': 0,
          },
          'conditions': [
            ['OS=="win" and target_arch=="ia32"', {
              'dependencies': [
                'src/trusted/service_runtime/service_runtime.gyp:sel_ldr64',
              ],
            }],
            ['disable_glibc==0',{
              'variables': {
                'tools': '--tools=newlib',
              },
            }, {
              'variables': {
                'tools': '--tools=newlib',
              },
            }],
          ],
          'actions': [
            {
              'action_name': 'test build',
              'msvs_cygwin_shell': 0,
              'description': 'Testing NACL build',
              'inputs': [
                '<!@(python <(script) -i <(arch) <(name) <(tools))',
              ],
              # Add a bogus output file, to cause this step to always fire.
              'outputs': [
                '<(PRODUCT_DIR)/test-output/hello_world.stamp'
              ],
              'action': [
                'python',
                '<(script)',
                '-r',
                '<(arch)',
                '<(name)',
                # TODO(bradnelson): Hack here to prevent gyp path ending with \"
                #               being passed to python which incorrectly
                #               interprets this as escaped quote.
                # http://code.google.com/p/chromium/issues/detail?id=141463
                '<(path)/hack',
                '<(tools)',
                '--stamp', '<@(_outputs)',
              ],
            },
          ],
        },
        { # Test the hello world translated from pexe to nexe
          'target_name': 'test_hello_world_pnacl_x86_64_nexe',
          'type': 'none',
          'dependencies': [
            'hello_world_nexe',
            'src/trusted/service_runtime/service_runtime.gyp:sel_ldr',
          ],
          'variables': {
            'arch': '--arch=<(target_arch)',
            'name': '--name=hello_world',
            'path': '--path=<(PRODUCT_DIR)',
            'tools': '--tools=pnacl_newlib',
            'script': '<(DEPTH)/native_client/build/test_build.py',
          },
          'conditions': [
            ['OS=="win" and target_arch=="ia32"', {
              'dependencies': [
                'src/trusted/service_runtime/service_runtime.gyp:sel_ldr64',
              ],
            }],
          ],
          'actions': [
            {
              'action_name': 'test pnacl nexe build',
              'msvs_cygwin_shell': 0,
              'description': 'Testing PNaCl translated Nexe build',
              'inputs': [
                '<!@(python <(script) -i <(arch) <(name) <(tools))',
              ],
              # Add a bogus output file, to cause this step to always fire.
              'outputs': [
                '<(PRODUCT_DIR)/test-output/hello_world_pnacl.stamp'
              ],
              'action': [
                'python',
                '<(script)',
                '-r',
                '<(arch)',
                '<(name)',
                # TODO(bradnelson): Hack here to prevent gyp path ending with \"
                #               being passed to python which incorrectly
                #               interprets this as escaped quote.
                # http://code.google.com/p/chromium/issues/detail?id=141463
                '<(path)/hack',
                '<(tools)',
                '--stamp', '<@(_outputs)',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
