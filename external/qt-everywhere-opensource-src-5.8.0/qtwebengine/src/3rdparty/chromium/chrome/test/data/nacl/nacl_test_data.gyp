# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../ppapi/ppapi_nacl_test_common.gypi',
  ],
  'targets': [
    {
      # TODO bug 512902 this needs to be ported to GN.
      'target_name': 'shared_test_files',
      'type': 'none',
      'variables': {
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'test_files': [
          # TODO(ncbray) move into chrome/test/data/nacl when all tests are
          # converted.
          '<(DEPTH)/ppapi/native_client/tools/browser_tester/browserdata/nacltest.js',

          # Files that aren't assosiated with any particular executable.
          'bad/ppapi_bad.html',
          'bad/ppapi_bad.js',
          'bad/ppapi_bad_native.html',
          'bad/ppapi_bad_doesnotexist.nmf',
          'bad/ppapi_bad_magic.nmf',
          'bad/ppapi_bad_manifest_uses_nexes.nmf',
          'bad/ppapi_bad_manifest_bad_files.nmf',
          'bad/ppapi_bad_manifest_nexe_arch.nmf',
          'crash/ppapi_crash.js',
          'crash/ppapi_crash_via_check_failure.html',
          'crash/ppapi_crash_via_exit_call.html',
          'crash/ppapi_crash_in_callback.html',
          'crash/ppapi_crash_ppapi_off_main_thread.html',
          'crash/ppapi_crash_off_main_thread.html',
          'load_util.js',
          'manifest_file/test_file.txt',
          'progress_event_listener.js',
          'simple_cc.js',
        ],
      },
      'conditions': [
        ['(target_arch=="ia32" or target_arch=="x64") and OS=="linux"', {
          # Enable nonsfi testing on ia32-linux environment.
          # This flag causes test_files to be copied into nonsfi directory,
          # too.
          'variables': {
            'enable_x86_32_nonsfi': 1,
          },
        }],
        ['target_arch=="arm" and OS=="linux"', {
          # Enable nonsfi testing on arm-linux environment.
          # This flag causes test_files to be copied into nonsfi directory,
          # too.
          'variables': {
            'enable_arm_nonsfi': 1,
          },
        }],
      ],
    },
    {
      'target_name': 'simple_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'simple',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'sources': [
          'simple.cc',
        ],
        'test_files': [
          'nacl_load_test.html',
        ],
      },
    },
    {
      'target_name': 'exit_status_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pm_exit_status_test',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'sources': [
          'exit_status/pm_exit_status_test.cc',
        ],
        'test_files': [
          'exit_status/pm_exit_status_test.html',
        ],
      },
    },
    {
      'target_name': 'extension_validation_cache',
      'type': 'none',
      'variables': {
        'nexe_target': 'extension_validation_cache',
        # The test currently only has the test expectations for the
        # newlib and glibc cases (# validation queries/settings), and has also
        # hardcoded the newlib and glibc variants' directory path for the
        # unpacked ext.
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 0,
        # Need a new directory to not clash with with other extension
        # tests's files (e.g., manifest.json).
        'nexe_destination_dir': 'nacl_test_data/extension_vcache_test',
        'sources': [
          'simple.cc',
        ],
        'test_files': [
          # TODO(ncbray) move into chrome/test/data/nacl when all tests are
          # converted.
          '<(DEPTH)/ppapi/native_client/tools/browser_tester/browserdata/nacltest.js',
          'extension_validation_cache/extension_validation_cache.html',
          'extension_validation_cache/extension_validation_cache.js',
          # Turns the test data directory into an extension.
          # Use a different nexe_destination_dir to isolate the files.
          # Note that the .nexe names are embedded in this file.
          'extension_validation_cache/manifest.json',
          'load_util.js',
          'simple_cc.js',
        ],
      },
    },
    {
      'target_name': 'sysconf_nprocessors_onln_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'sysconf_nprocessors_onln_test',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'sources': [
          'sysconf_nprocessors_onln/sysconf_nprocessors_onln_test.cc',
        ],
        'test_files': [
          'sysconf_nprocessors_onln/sysconf_nprocessors_onln_test.html',
        ],
      },
    },
    {
      'target_name': 'ppapi_test_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libppapi_test_lib.a',
        'nso_target': 'libppapi_test_lib.so',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'sources': [
          'ppapi_test_lib/get_browser_interface.cc',
          'ppapi_test_lib/internal_utils.cc',
          'ppapi_test_lib/module_instance.cc',
          'ppapi_test_lib/test_interface.cc',
          'ppapi_test_lib/testable_callback.cc',
        ]
      },
    },
    {
      'target_name': 'ppapi_progress_events',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_progress_events',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'progress_events/ppapi_progress_events.cc',
        ],
        'test_files': [
          'progress_events/ppapi_progress_events.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_ppp_initialize',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_ppp_initialize',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_ppp_initialize.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_ppp_initialize_crash',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_ppp_initialize_crash',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_ppp_initialize_crash.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_no_ppp_instance',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_no_ppp_instance',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_no_ppp_instance.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_get_ppp_instance_crash',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_get_ppp_instance_crash',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_get_ppp_instance_crash.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_ppp_instance_didcreate',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_ppp_instance_didcreate',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_ppp_instance_didcreate.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_ppp_instance_didcreate_crash',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_ppp_instance_didcreate_crash',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_ppp_instance_didcreate_crash.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_crash_via_check_failure',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_crash_via_check_failure',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'crash/ppapi_crash_via_check_failure.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_crash_via_exit_call',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_crash_via_exit_call',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'crash/ppapi_crash_via_exit_call.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_crash_in_callback',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_crash_in_callback',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'crash/ppapi_crash_in_callback.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_crash_off_main_thread',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_crash_off_main_thread',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'crash/ppapi_crash_off_main_thread.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_crash_ppapi_off_main_thread',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_crash_ppapi_off_main_thread',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'crash/ppapi_crash_ppapi_off_main_thread.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'irt_manifest_file',
      'type': 'none',
      'variables': {
        'nexe_target': 'irt_manifest_file',
        'build_newlib': 1,
        # Linking problems - can't find __nacl_irt_query.
        'build_glibc': 0,
        # TODO(ncbray) support file injection into PNaCl manifest.
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi_cpp',
          '-lppapi',
          '-lplatform',
          '-lgio',
          '-lnacl',
        ],
        'sources': [
          'manifest_file/irt_manifest_file_test.cc',
        ],
        'create_nmf_args_portable': [
          '-xtest_file:test_file.txt',
          '-xnmf says hello world:test_file.txt',
          # There is no dummy_test_file.txt file intentionally. This is just for
          # a test case where there is a manifest entry, but no actual file.
          '-xdummy_test_file:dummy_test_file.txt',
        ],
        'test_files': [
          'manifest_file/irt_manifest_file_test.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:aot',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib',
      ],
      'conditions': [
        # These are needed to build a non-SFI nexe binary.
        # Note that these trigger building nexe files for other
        # architectures, such as x86-32 (based on enable_XXX variables).
        # As described above, although the tests for pnacl are currently
        # disabled, but building the binary should work.
        # We cannot disable building, as enable_XXX variables are also used
        # to build newlib linked nexes.
        ['(target_arch=="ia32" or target_arch=="x64") and OS=="linux"', {
          # Enable nonsfi testing on ia32-linux environment.
          'variables': {
            'build_pnacl_newlib': 1,
            'translate_pexe_with_build': 1,
            'enable_x86_32_nonsfi': 1,
          },
        }],
        ['target_arch=="arm" and OS=="linux"', {
          # Enable nonsfi testing on arm-linux environment.
          'variables': {
            'build_pnacl_newlib': 1,
            'translate_pexe_with_build': 1,
            'enable_arm_nonsfi': 1,
          },
        }],
      ],
    },
    {
      'target_name': 'irt_exception_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'irt_exception_test',
        'build_newlib': 1,
        'generate_nmf': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'build_pnacl_newlib': 1,
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
          '-lnacl_exception',
        ],
        'sources': [
          'irt_exception/irt_exception_test.cc',
        ],
        'test_files': [
          # TODO(ncbray) move into chrome/test/data/nacl when all tests are
          # converted.
          'irt_exception/irt_exception_test.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_exception_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_irt_shim.gyp:aot',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
      'conditions': [
        ['(target_arch=="ia32" or target_arch=="x64") and OS=="linux"', {
          # Enable nonsfi testing on ia32-linux environment.
          'variables': {
            'enable_x86_32_nonsfi': 1,
            'translate_pexe_with_build': 1,
          },
        }],
        ['target_arch=="arm" and OS=="linux"', {
          # Enable nonsfi testing on arm-linux environment.
          'variables': {
            'enable_arm_nonsfi': 1,
            'translate_pexe_with_build': 1,
          },
        }],
      ],
    },
    {
      'target_name': 'ppapi_extension_mime_handler',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_extension_mime_handler',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'extension_mime_handler/ppapi_extension_mime_handler.cc',
        ],
        'test_files': [
          'extension_mime_handler/ppapi_extension_mime_handler.html',
          'extension_mime_handler/mime_test_data.dat',
          # For faking the file's MIME type.
          'extension_mime_handler/mime_test_data.dat.mock-http-headers',
          # Turns the test data directory into an extension.  Hackish.
          # Note that the .nexe names are embedded in this file.
          'extension_mime_handler/manifest.json',
        ],
      },
      'dependencies': [
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'pnacl_debug_url_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pnacl_debug_url',
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'generate_nmf': 0,
        'sources': [
          'simple.cc',
        ],
        'test_files': [
          'pnacl_debug_url/pnacl_debug_url.html',
          'pnacl_debug_url/pnacl_has_debug.nmf',
          'pnacl_debug_url/pnacl_has_debug_flag_off.nmf',
          'pnacl_debug_url/pnacl_no_debug.nmf',
        ],
      },
    },
    {
      'target_name': 'pnacl_error_handling_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pnacl_errors',
        'extra_args': ['--nonstable-pnacl'],
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'sources': [
          'simple.cc',
        ],
        'generate_nmf': 0,
        'test_files': [
          'pnacl_error_handling/pnacl_error_handling.html',
          'pnacl_error_handling/bad.pexe',
          'pnacl_error_handling/pnacl_bad_pexe.nmf',
          'pnacl_error_handling/pnacl_bad_pexe_O0.nmf',
          'pnacl_error_handling/pnacl_bad_doesnotexist.nmf',
          'pnacl_error_handling/pnacl_illformed_manifest.nmf',
          'pnacl_error_handling/pnacl_nonfinal_pexe_O0.nmf',
        ],
      },
    },
    {
      'target_name': 'pnacl_mime_type_test',
      'type': 'none',
      'variables': {
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'test_files': [
          'pnacl_mime_type/pnacl_mime_type.html',
        ],
      },
    },
    {
      'target_name': 'pnacl_options_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pnacl_options',
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'generate_nmf': 0,
        'sources': [
          'simple.cc',
        ],
        'test_files': [
          'pnacl_nmf_options/pnacl_options.html',
          'pnacl_nmf_options/pnacl_o_0.nmf',
          'pnacl_nmf_options/pnacl_o_2.nmf',
          'pnacl_nmf_options/pnacl_o_large.nmf',
        ],
      },
    },
    {
      'target_name': 'pnacl_url_loader_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pnacl_url_loader',
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'generate_nmf': 1,
        'link_flags': [
          '-lppapi',
        ],
        'sources': [
          'pnacl_url_loader/pnacl_url_loader.cc',
        ],
        'test_files': [
          'pnacl_url_loader/pnacl_url_loader.html',
        ],
      },
    },
    {
      'target_name': 'pnacl_dyncode_syscall_disabled_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pnacl_dyncode_syscall_disabled',
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
          # The "_private" variant of the library calls the syscalls
          # directly, which allows us to test the syscalls directly,
          # even when the dyncode IRT interface is also disabled under
          # PNaCl.
          '-lnacl_dyncode_private',
        ],
        'sources': [
          'pnacl_dyncode_syscall_disabled/pnacl_dyncode_syscall_disabled.cc',
        ],
        'test_files': [
          'pnacl_dyncode_syscall_disabled/pnacl_dyncode_syscall_disabled.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_dyncode_private_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'pnacl_hw_eh_disabled_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pnacl_hw_eh_disabled',
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
          # The "_private" variant of the library calls the syscalls
          # directly, which allows us to test the syscalls directly,
          # even when the exception-handling IRT interface is also
          # disabled under PNaCl.
          '-lnacl_exception_private',
        ],
        'sources': [
          'pnacl_hw_eh_disabled/pnacl_hw_eh_disabled.cc',
        ],
        'test_files': [
          'pnacl_hw_eh_disabled/pnacl_hw_eh_disabled.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_exception_private_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    # Legacy NaCl PPAPI interface tests being here.
    {
      'target_name': 'ppapi_ppb_core',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_ppb_core',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'ppapi/ppb_core/ppapi_ppb_core.cc',
        ],
        'test_files': [
          'ppapi/ppb_core/ppapi_ppb_core.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_ppb_instance',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_ppb_instance',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'ppapi/ppb_instance/ppapi_ppb_instance.cc',
        ],
        'test_files': [
          'ppapi/ppb_instance/ppapi_ppb_instance.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_ppp_instance',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_ppp_instance',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'ppapi/ppp_instance/ppapi_ppp_instance.cc',
        ],
        'test_files': [
          'ppapi/ppp_instance/ppapi_ppp_instance.html',
          'ppapi/ppp_instance/ppapi_ppp_instance.js',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_nacl.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
  ],
  'conditions': [
    ['target_arch!="arm" and disable_newlib==0', {
      # Source file does not have asm for ARM.
      'targets': [
        {
          'target_name': 'partly_invalid',
          'type': 'none',
          'variables': {
            'nexe_target': 'partly_invalid',
            'build_newlib': 1,
            'build_glibc': 0,
            'build_pnacl_newlib': 0,
            'nexe_destination_dir': 'nacl_test_data',
            'sources': [
              '<(DEPTH)/native_client/tests/stubout_mode/partly_invalid.c',
            ],
          },
        },
      ],
    }],

    # Tests for non-SFI mode.
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'nonsfi_libc_free_nexe',
          'type': 'executable',
          'product_name': '>(nexe_destination_dir)/libc_free_>(arch_suffix)',
          'product_extension': 'nexe',
          'variables': {
            'arch_suffix': '<(target_arch)',
            # This is needed, because NaCl's target_default rule is
            # automatically applied to all the targets in this file, and it
            # requires nexe_destination_dir, even if it is not important.
            'nexe_destination_dir': 'nacl_test_data/libc-free',
          },
          'sources': [
            'nonsfi/libc_free.c',
          ],
          # Here, we would like to link a relocatable, libc-free executable.
          # -shared/-fPIC make this binary relocatable. -nostdlib ensures
          # this is libc-free.
          # The program does not apply any dynamic relocations at start up,
          # so it cannot rely on relocations having been applied.
          # In addition, -fvisibility=hidden avoids creating some types
          # of relocation.
          'cflags': [
            '-fPIC',
            '-fvisibility=hidden',

            # Stack-Smashing protector does not work with libc-free context.
            '-fno-stack-protector',
            # Optimizers may translate the original code to code which
            # requires builtin functions and/or relocations. Specifically,
            # the LLVM's optimizer translates for-loop based zero
            # clear to memset.
            '-O0',
          ],
          'cflags!': [
            # We filter these out because release_extra_cflags or another
            # such thing might be adding them in, and those options wind up
            # coming after the -fno-stack-protector we added above.
            '-fstack-protector',
            '-fstack-protector-all',
            '-fprofile-generate',
            '-finstrument-functions',
            # ARM GCC emits symbols like __aeabi_unwind_cpp_pr0 in
            # .exidx sections with this flag.
            '-funwind-tables',
          ],
          'ldflags': [
            '-nostdlib',
            '-shared',
            # This binary cannot relocate itself, so we should have no
            # undefined references left.
            '-Wl,--no-undefined',
          ],
          'ldflags!': [
            # Explicitly remove the -pthread flag to avoid a link time warning.
            '-pthread',
          ],
          # Do not use any sanitizers tools, which require a few symbols.
          'cflags/': [
            ['exclude', '-fsanitize'],
            ['exclude', '^-O'],  # Strip -O2, -Os etc.
          ],
          'ldflags/': [
            ['exclude', '-fsanitize'],
          ],
          'defines': [
            # The code depends on NaCl's headers. This is a macro for them.
            'NACL_LINUX=1',
          ],
          # For native_client/src/include/...
          'include_dirs': [
            '../../../..',
          ],
          'conditions': [
            # Overwrite suffix for x64 and ia32 to align NaCl's naming
            # convention.
            ['target_arch=="x64"', {
              'variables': {
                'arch_suffix': 'x86_64',
              }
            }],
            ['target_arch=="ia32"', {
              'variables': {
                'arch_suffix': 'x86_32',
              }
            }],
          ],
        },
        {
          'target_name': 'nonsfi_libc_free',
          'type': 'none',
          'variables': {
            'nexe_destination_dir': 'nacl_test_data',
            'destination_dir': '<(PRODUCT_DIR)/>(nexe_destination_dir)/libc-free',
            'test_files': [
              # TODO(ncbray) move into chrome/test/data/nacl when all tests are
              # converted.
              '<(DEPTH)/ppapi/native_client/tools/browser_tester/browserdata/nacltest.js',
              'nonsfi/libc_free.html',
              'nonsfi/libc_free.nmf',
              'nonsfi/irt_test.html',
            ],
          },
          'dependencies': [
            'nonsfi_libc_free_nexe',
          ],
          # Because we are still under development for non-SFI mode, the
          # toolchain is not yet ready, which means ppapi_nacl_common does not
          # work well for non-SFI mode yet. Instead, we manually set up the
          # testing environment here.
          'copies': [
            {
              'destination': '>(destination_dir)',
              'files': [
                '>@(test_files)',
              ],
            },
          ],
        },
      ],
    }],
  ],
}
