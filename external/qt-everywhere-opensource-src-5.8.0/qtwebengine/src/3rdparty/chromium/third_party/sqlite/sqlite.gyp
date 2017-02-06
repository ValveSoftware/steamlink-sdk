# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'use_system_sqlite%': 0,
    'required_sqlite_version': '3.6.1',
  },
  'target_defaults': {
    'defines': [
      'SQLITE_ENABLE_FTS3',
      # New unicode61 tokenizer with built-in tables.
      'SQLITE_DISABLE_FTS3_UNICODE',
      # Chromium currently does not enable fts4, disable extra code.
      'SQLITE_DISABLE_FTS4_DEFERRED',
      'SQLITE_ENABLE_ICU',
      'SQLITE_ENABLE_MEMORY_MANAGEMENT',
      'SQLITE_SECURE_DELETE',
      # Custom flag to tweak pcache pools.
      # TODO(shess): This shouldn't use faux-SQLite naming.
      'SQLITE_SEPARATE_CACHE_POOLS',
      # TODO(shess): SQLite adds mutexes to protect structures which cross
      # threads.  In theory Chromium should be able to turn this off for a
      # slight speed boost.
      'THREADSAFE',
      # SQLite can spawn threads to sort in parallel if configured
      # appropriately.  Chromium doesn't configure SQLite for that, and would
      # prefer to control distribution to worker threads.
      'SQLITE_MAX_WORKER_THREADS=0',
      # Allow 256MB mmap footprint per connection.  Should not be too open-ended
      # as that could cause memory fragmentation.  50MB encompasses the 99th
      # percentile of Chrome databases in the wild.
      # TODO(shess): A 64-bit-specific value could be 1G or more.
      # TODO(shess): Figure out if exceeding this is costly.
      'SQLITE_MAX_MMAP_SIZE=268435456',
      # Use a read-only memory map when mmap'ed I/O is enabled to prevent memory
      # stompers from directly corrupting the database.
      # TODO(shess): Upstream the ability to use this define.
      'SQLITE_MMAP_READ_ONLY=1',
      # By default SQLite pre-allocates 100 pages of pcache data, which will not
      # be released until the handle is closed.  This is contrary to Chromium's
      # memory-usage goals.
      'SQLITE_DEFAULT_PCACHE_INITSZ=0',
      # NOTE(shess): Some defines can affect the amalgamation.  Those should be
      # added to google_generate_amalgamation.sh, and the amalgamation
      # re-generated.  Usually this involves disabling features which include
      # keywords or syntax, for instance SQLITE_OMIT_VIRTUALTABLE omits the
      # virtual table syntax entirely.  Missing an item usually results in
      # syntax working but execution failing.  Review:
      #   src/src/parse.py
      #   src/tool/mkkeywordhash.c
    ],
  },
  'targets': [
    {
      'target_name': 'sqlite',
      'conditions': [
        ['use_system_sqlite', {
          'type': 'none',
          'direct_dependent_settings': {
            'defines': [
              'USE_SYSTEM_SQLITE',
            ],
          },

          'conditions': [
            ['OS == "ios"', {
              'dependencies': [
                'sqlite_recover',
                'sqlite_regexp',
              ],
              'link_settings': {
                'xcode_settings': {
                  'OTHER_LDFLAGS': [
                    '-lsqlite3',
                  ],
                },
              },
            }],
            ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android"', {
              'direct_dependent_settings': {
                'cflags': [
                  # This next command produces no output but it it will fail
                  # (and cause GYP to fail) if we don't have a recent enough
                  # version of sqlite.
                  '<!@(pkg-config --atleast-version=<(required_sqlite_version) sqlite3)',

                  '<!@(pkg-config --cflags sqlite3)',
                ],
              },
              'link_settings': {
                'ldflags': [
                  '<!@(pkg-config --libs-only-L --libs-only-other sqlite3)',
                ],
                'libraries': [
                  '<!@(pkg-config --libs-only-l sqlite3)',
                ],
              },
            }],
          ],
        }, { # !use_system_sqlite
          # "sqlite3" can cause conflicts with the system library.
          'product_name': 'chromium_sqlite3',
          'type': '<(component)',
          'sources': [
            'amalgamation/config.h',
            'amalgamation/sqlite3.h',
            'amalgamation/sqlite3.c',
            'src/src/recover_varint.c',
            'src/src/recover.c',
            'src/src/recover.h',
          ],
          'variables': {
            'clang_warning_flags': [
              # sqlite contains a few functions that are unused, at least on
              # Windows with Chromium's sqlite patches applied
              # (interiorCursorEOF fts3EvalDeferredPhrase
              # fts3EvalSelectDeferred sqlite3Fts3InitHashTable
              # sqlite3Fts3InitTok).
              '-Wno-unused-function',
            ],
          },
          'include_dirs': [
            'amalgamation',
          ],
          'dependencies': [
            '../icu/icu.gyp:icui18n',
            '../icu/icu.gyp:icuuc',
          ],
          'msvs_disabled_warnings': [
            4244, 4267, 4996,
          ],
          'conditions': [
            ['OS == "win" and component == "shared_library"', {
              'defines': ['SQLITE_API=__declspec(dllexport)'],
              'direct_dependent_settings': {
                'defines': ['SQLITE_API=__declspec(dllimport)'],
              },
            }],
            ['OS != "win" and component == "shared_library"', {
              'defines': ['SQLITE_API=__attribute__((visibility("default")))'],
            }],
            ['os_posix == 1', {
              'defines': [
                # Allow xSleep() call on Unix to use usleep() rather than
                # sleep().  Microsecond precision is better than second
                # precision.  Should only affect contended databases via the
                # busy callback.  Browser profile databases are mostly
                # exclusive, but renderer databases may allow for contention.
                'HAVE_USLEEP=1',
                # Use pread/pwrite directly rather than emulating them.
                'USE_PREAD=1',
              ],
            }],
            # Pull in config.h on Linux.  This allows use of preprocessor macros
            # which are not available to the build config.
            ['OS == "linux"', {
              'defines': [
                '_HAVE_SQLITE_CONFIG_H',
              ],
            }],
            ['OS == "linux" or OS == "android"', {
              'defines': [
                # Linux provides fdatasync(), a faster equivalent of fsync().
                'fdatasync=fdatasync',
              ],
            }],
            ['OS=="linux"', {
              'link_settings': {
                'libraries': [
                  '-ldl',
                ],
              },
            }],
            ['OS == "mac" or OS == "ios"', {
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
                  '$(SDKROOT)/System/Library/Frameworks/CoreServices.framework',
                ],
              },
            }],
            ['OS == "android"', {
              'defines': [
                'SQLITE_DEFAULT_JOURNAL_SIZE_LIMIT=1048576',
                'SQLITE_DEFAULT_AUTOVACUUM=1',
                'SQLITE_TEMP_STORE=3',
                'SQLITE_ENABLE_FTS3_BACKWARDS',
                'SQLITE_DEFAULT_FILE_FORMAT=4',
              ],
            }],
            ['os_posix == 1 and OS != "mac" and OS != "android"', {
              'cflags': [
                # SQLite doesn't believe in compiler warnings,
                # preferring testing.
                #   http://www.sqlite.org/faq.html#q17
                '-Wno-int-to-pointer-cast',
                '-Wno-pointer-to-int-cast',
              ],
            }],
          ],
        }],
      ],
      'includes': [
        # Disable LTO due to ELF section name out of range
        # crbug.com/422251
        '../../build/android/disable_gcc_lto.gypi',
      ],
    },
  ],
  'conditions': [
    ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android" and not use_system_sqlite', {
      'targets': [
        {
          'target_name': 'sqlite_shell',
          'type': 'executable',
          'dependencies': [
            '../icu/icu.gyp:icuuc',
            'sqlite',
          ],
          # So shell.c can find the correct sqlite3.h.
          'include_dirs': [
            'amalgamation',
          ],
          'sources': [
            'src/src/shell.c',
            'src/src/shell_icu_linux.c',
            # Include a dummy c++ file to force linking of libstdc++.
            'build_as_cpp.cc',
          ],
        },
      ],
    },],
    ['OS == "ios"', {
      'targets': [
        {
          # Virtual table used by sql::Recovery to recover corrupt
          # databases, for use with USE_SYSTEM_SQLITE.
          'target_name': 'sqlite_recover',
          'type': 'static_library',
          'sources': [
            # TODO(shess): Move out of the SQLite source tree, perhaps to ext/.
            'src/src/recover_varint.c',
            'src/src/recover.c',
            'src/src/recover.h',
          ],
        },
        {
          'target_name': 'sqlite_regexp',
          'type': 'static_library',
          'dependencies': [
            '../icu/icu.gyp:icui18n',
            '../icu/icu.gyp:icuuc',
          ],
          'defines': [
            # Necessary to statically compile the extension.
            'SQLITE_CORE',
          ],
          'sources': [
            'src/ext/icu/icu.c',
          ],
          'variables': {
            'clang_warning_flags_unset': [
              # icu.c uses assert(!"foo") instead of assert(false && "foo")
              '-Wstring-conversion',
            ],
          },
        },
      ],
    }],
  ],
}
