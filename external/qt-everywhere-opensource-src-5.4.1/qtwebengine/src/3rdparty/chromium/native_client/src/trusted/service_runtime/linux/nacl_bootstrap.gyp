# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../build/common.gypi',
  ],
  'conditions': [
    ['target_arch=="x64"', {
      'variables': {
        # No extra reservation.
        'nacl_reserve_top': '0x0',
      }
    }],
    ['target_arch=="ia32"', {
      'variables': {
        # 1G address space.
        'nacl_reserve_top': '0x40000000',
      }
    }],
    ['target_arch=="arm"', {
      'variables': {
        # 1G address space, plus 8K guard area above.
        'nacl_reserve_top': '0x40002000',
      }
    }],
    ['target_arch=="mipsel"', {
      'variables': {
        # 1G address space, plus 32K guard area above.
        'nacl_reserve_top': '0x40008000',
      }
    }],
  ],
  'targets': [
    {
      'target_name': 'nacl_bootstrap_munge_phdr',
      'type': 'executable',
      'toolsets': ['host'],
      'sources': [
        'nacl_bootstrap_munge_phdr.c',
      ],
      'libraries': [
        '-lelf',
      ],
      # This is an ugly kludge because gyp doesn't actually treat
      # host_arch=x64 target_arch=ia32 as proper cross compilation.
      # It still wants to compile the "host" program with -m32 et
      # al.  Though a program built that way can indeed run on the
      # x86-64 host, we cannot reliably build this program on such a
      # host because Ubuntu does not provide the full suite of
      # x86-32 libraries in packages that can be installed on an
      # x86-64 host; in particular, libelf is missing.  So here we
      # use the hack of eliding all the -m* flags from the
      # compilation lines, getting the command close to what they
      # would be if gyp were to really build properly for the host.
      # TODO(bradnelson): Clean up with proper cross support.
      'cflags/': [['exclude', '^-m.*'],
                  ['exclude', '^--sysroot=.*']],
      'ldflags/': [['exclude', '^-m.*'],
                   ['exclude', '^--sysroot=.*']],
      'cflags!': [
        # MemorySanitizer reports an error in this binary unless instrumented
        # libelf is supplied. Because libelf source code uses gcc extensions,
        # building it with MemorySanitizer (which implies clang) is challenging,
        # and it's much simpler to just disable MSan for this target.
        '-fsanitize=memory',
        '-fsanitize-memory-track-origins',
        # This causes an "unused argument" warning in C targets.
        '-stdlib=libc++',
      ],
      'ldflags!': [
        '-fsanitize=memory',
      ],
    },
    {
      'target_name': 'nacl_bootstrap_lib',
      'type': 'static_library',
      'product_dir': '<(SHARED_INTERMEDIATE_DIR)/nacl_bootstrap',
      'hard_depencency': 1,
      'include_dirs': [
        '..',
      ],
      'sources': [
        'nacl_bootstrap.c',
      ],
      'cflags': [
        # Prevent llvm-opt from replacing my_bzero with a call
        # to memset.
        '-fno-builtin',
        # The tiny standalone bootstrap program is incompatible with
        # -fstack-protector, which might be on by default.  That switch
        # requires using the standard libc startup code, which we do not.
        '-fno-stack-protector',
        # We don't want to compile it PIC (or its cousin PIE), because
        # it goes at an absolute address anyway, and because any kind
        # of PIC complicates life for the x86-32 assembly code.  We
        # append -fno-* flags here instead of using a 'cflags!' stanza
        # to remove -f* flags, just in case some system's compiler
        # defaults to using PIC for everything.
        '-fno-pic', '-fno-PIC',
        '-fno-pie', '-fno-PIE',
      ],
      'cflags!': [
        '-fsanitize=address',
        '-fsanitize=memory',
        '-w',
        # We filter these out because release_extra_cflags or another
        # such thing might be adding them in, and those options wind up
        # coming after the -fno-stack-protector we added above.
        '-fstack-protector',
        '-fstack-protector-all',
        '-fprofile-generate',
        '-finstrument-functions',
        '-funwind-tables',
        # This causes an "unused argument" warning in C targets.
        '-stdlib=libc++',
      ],
      'conditions': [
        ['clang==1', {
          'cflags': [
            # TODO(bbudge) Remove this when Clang supports -fno-pic.
            '-Qunused-arguments',
          ],
        }],
      ],
    },
    {
      'target_name': 'nacl_bootstrap_raw',
      'type': 'none',
      # This magical flag tells Gyp that the dependencies of this target
      # are nobody else's business and it should not propagate them up
      # to things that list this as a dependency.  Without this, it will
      # wind up adding the nacl_bootstrap_lib static library into the
      # link of sel_ldr or chrome just because one executable depends on
      # the other.
      # TODO(mcgrathr): Maybe one day Gyp will grow a proper target type
      # for the use we really want here: custom commands for linking an
      # executable, and no peeking inside this target just because you
      # depend on it.  Then we could stop using this utterly arcane flag
      # in favor of something vaguely self-explanatory.
      # See http://code.google.com/p/gyp/issues/detail?id=239 for the
      # history of the arcana.
      'dependencies_traverse': 0,
      'dependencies': [
        'nacl_bootstrap_lib',
      ],
      'actions': [
        {
          'action_name': 'link_with_ld_bfd',
          'variables': {
            'bootstrap_lib': '<(SHARED_INTERMEDIATE_DIR)/nacl_bootstrap/<(STATIC_LIB_PREFIX)nacl_bootstrap_lib<(STATIC_LIB_SUFFIX)',
            'linker_script': 'nacl_bootstrap.x',
          },
          'inputs': [
            '<(linker_script)',
            '<(bootstrap_lib)',
            'ld_bfd.py',
          ],
          'outputs': [
            '<(PRODUCT_DIR)/nacl_bootstrap_raw',
          ],
          'message': 'Linking nacl_bootstrap_raw',
          'conditions': [
            ['target_arch=="x64"', {
              'variables': {
                'linker_emulation': 'elf_x86_64',
              }
            }],
            ['target_arch=="ia32"', {
              'variables': {
                'linker_emulation': 'elf_i386',
              }
            }],
            ['target_arch=="arm"', {
              'variables': {
                'linker_emulation': 'armelf_linux_eabi',
              }
            }],
            ['target_arch=="mipsel"', {
              'variables': {
                'linker_emulation': 'elf32ltsmip',
              },
            }],
            # ld_bfd.py needs to know the target compiler used for the
            # build but the $CXX environment variable might only be
            # set at gyp time. This is a hacky way to bake the correct
            # CXX into the build at gyp time.
            # TODO(sbc): Do this better by providing some gyp syntax
            # for accessing the name of the configured compiler, or
            # even a better way to access gyp time environment
            # variables from within a gyp file.
            ['OS=="android"', {
              'variables': {
                'compiler': '<!(/bin/echo -n <(android_toolchain)/*-g++)',
              }
            }, {
              'variables': {
                'compiler': '<!(echo ${CXX:=g++})',
              }
            }],
          ],
          'action': ['python', 'ld_bfd.py',
                     '--compiler', '<(compiler)',
                     '-m', '<(linker_emulation)',
                     '--build-id',
                     # This program is (almost) entirely
                     # standalone.  It has its own startup code, so
                     # no crt1.o for it.  It is statically linked,
                     # and on x86 it does not use libc at all.
                     # However, on ARM it needs a few (safe) things
                     # from libc.
                     '-static',
                     # Link with custom linker script for special
                     # layout.  The script uses the symbol RESERVE_TOP.
                     '--defsym', 'RESERVE_TOP=<(nacl_reserve_top)',
                     '--script=<(linker_script)',
                     '-o', '<@(_outputs)',
                     # On x86-64, the default page size with some
                     # linkers is 2M rather than the real Linux page
                     # size of 4K.  A larger page size is incompatible
                     # with our custom linker script's special layout.
                     '-z', 'max-page-size=0x1000',
                     '--whole-archive', '<(bootstrap_lib)',
                     '--no-whole-archive',
                   ],
        }
      ],
    },
    {
      'target_name': 'nacl_helper_bootstrap',
      'dependencies': [
        'nacl_bootstrap_raw',
        'nacl_bootstrap_munge_phdr#host',
      ],
      'type': 'none',
      # See above about this magical flag.
      # It's actually redundant in practice to have it here as well.
      # But it expresses the intent: that anything that depends on
      # this target has no interest in what goes into building it.
      'dependencies_traverse': 0,
      'actions': [{
        'action_name': 'munge_phdr',
        'inputs': ['nacl_bootstrap_munge_phdr.py',
                   '<(PRODUCT_DIR)/nacl_bootstrap_munge_phdr',
                   '<(PRODUCT_DIR)/nacl_bootstrap_raw'],
        'outputs': ['<(PRODUCT_DIR)/nacl_helper_bootstrap'],
        'message': 'Munging ELF program header',
        'action': ['python', '<@(_inputs)', '<@(_outputs)']
      }],
    },
  ],
}
