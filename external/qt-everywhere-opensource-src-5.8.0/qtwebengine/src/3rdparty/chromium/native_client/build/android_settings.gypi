# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  # This block was taken wholesale from chromium's build/common.gypi
  # BEGIN CODE TAKEN FROM CHROME'S COMMON.GYPI
  'variables': {
    # Location of Android NDK.
    'variables': {
      'variables': {
        # Unfortunately we have to use absolute paths to the SDK/NDK because
        # they're passed to ant which uses a different relative path from
        # gyp.
        'android_ndk_root%': '<!(cd <(DEPTH) && pwd -P)/third_party/android_tools/ndk/',
        'android_ndk_experimental_root%': '<!(cd <(DEPTH) && pwd -P)/third_party/android_tools/ndk_experimental/',
        'android_sdk_root%': '<!(cd <(DEPTH) && pwd -P)/third_party/android_tools/sdk/',
        'android_host_arch%': '<!(uname -m)',
        # Android API-level of the SDK used for compilation.
        'android_sdk_version%': '21',
        'android_sdk_build_tools_version%': '21.0.1',
        'host_os%': "<!(uname -s | sed -e 's/Linux/linux/;s/Darwin/mac/')",
      },
      # Copy conditionally-set variables out one scope.
      'android_ndk_root%': '<(android_ndk_root)',
      'android_sdk_root%': '<(android_sdk_root)',
      'android_sdk_version%': '<(android_sdk_version)',
      'android_stlport_root': '<(android_ndk_root)/sources/cxx-stl/stlport',
      'host_os%': '<(host_os)',

      'android_sdk%': '<(android_sdk_root)/platforms/android-<(android_sdk_version)',
      # Android SDK build tools (e.g. dx, aapt, aidl)
      'android_sdk_tools%': '<(android_sdk_root)/build-tools/<(android_sdk_build_tools_version)',

      # Android API level 14 is ICS (Android 4.0) which is the minimum
      # platform requirement for Chrome on Android, we use it for native
      # code compilation.
      'conditions': [
        ['target_arch == "ia32"', {
          'android_app_abi%': 'x86',
          'android_gdbserver%': '<(android_ndk_root)/prebuilt/android-x86/gdbserver/gdbserver',
          'android_ndk_sysroot%': '<(android_ndk_root)/platforms/android-14/arch-x86',
          'android_ndk_lib_dir%': 'usr/lib',
          'android_toolchain%': '<(android_ndk_root)/toolchains/x86-4.8/prebuilt/<(host_os)-<(android_host_arch)/bin',
        }],
        ['target_arch == "x64"', {
          'android_app_abi%': 'x86_64',
          'android_gdbserver%': '<(android_ndk_experimental_root)/prebuilt/android-x86_64/gdbserver/gdbserver',
          'android_ndk_sysroot%': '<(android_ndk_experimental_root)/platforms/android-20/arch-x86_64',
          'android_ndk_lib_dir%': 'usr/lib64',
          'android_toolchain%': '<(android_ndk_experimental_root)/toolchains/x86_64-4.8/prebuilt/<(host_os)-<(android_host_arch)/bin',
          'android_stlport_root': '<(android_ndk_experimental_root)/sources/cxx-stl/stlport',
        }],
        ['target_arch=="arm"', {
          'conditions': [
            ['arm_version<7', {
              'android_app_abi%': 'armeabi',
            }, {
              'android_app_abi%': 'armeabi-v7a',
            }],
          ],
          'android_gdbserver%': '<(android_ndk_root)/prebuilt/android-arm/gdbserver/gdbserver',
          'android_ndk_sysroot%': '<(android_ndk_root)/platforms/android-14/arch-arm',
          'android_ndk_lib_dir%': 'usr/lib',
          'android_toolchain%': '<(android_ndk_root)/toolchains/arm-linux-androideabi-4.8/prebuilt/<(host_os)-<(android_host_arch)/bin',
        }],
        ['target_arch == "arm64"', {
          'android_app_abi%': 'arm64-v8a',
          'android_gdbserver%': '<(android_ndk_experimental_root)/prebuilt/android-arm64/gdbserver/gdbserver',
          'android_ndk_sysroot%': '<(android_ndk_experimental_root)/platforms/android-20/arch-arm64',
          'android_ndk_lib_dir%': 'usr/lib',
          'android_toolchain%': '<(android_ndk_experimental_root)/toolchains/aarch64-linux-android-4.9/prebuilt/<(host_os)-<(android_host_arch)/bin',
          'android_stlport_root': '<(android_ndk_experimental_root)/sources/cxx-stl/stlport',
        }],
        ['target_arch == "mipsel"', {
          'android_app_abi%': 'mips',
          'android_gdbserver%': '<(android_ndk_root)/prebuilt/android-mips/gdbserver/gdbserver',
          'android_ndk_sysroot%': '<(android_ndk_root)/platforms/android-14/arch-mips',
          'android_ndk_lib_dir%': 'usr/lib',
          'android_toolchain%': '<(android_ndk_root)/toolchains/mipsel-linux-android-4.8/prebuilt/<(host_os)-<(android_host_arch)/bin',
        }],
      ],
    },
    # Copy conditionally-set variables out one scope.
    'android_app_abi%': '<(android_app_abi)',
    'android_gdbserver%': '<(android_gdbserver)',
    'android_ndk_root%': '<(android_ndk_root)',
    'android_ndk_sysroot%': '<(android_ndk_sysroot)',
    'android_sdk_root%': '<(android_sdk_root)',
    'android_sdk_version%': '<(android_sdk_version)',
    'android_toolchain%': '<(android_toolchain)',

    'android_ndk_include': '<(android_ndk_sysroot)/usr/include',
    'android_ndk_lib': '<(android_ndk_sysroot)/<(android_ndk_lib_dir)',
    'android_sdk_tools%': '<(android_sdk_tools)',
    'android_sdk%': '<(android_sdk)',
    'android_sdk_jar%': '<(android_sdk)/android.jar',

    'android_stlport_root': '<(android_stlport_root)',
    'android_stlport_include': '<(android_stlport_root)/stlport',
    'android_stlport_libs_dir': '<(android_stlport_root)/libs/<(android_app_abi)',
    'host_os%': '<(host_os)',

    # Location of the "strip" binary, used by both gyp and scripts.
    'android_strip%' : '<!(/bin/echo -n <(android_toolchain)/*-strip)',

    # Location of the "readelf" binary.
    'android_readelf%' : '<!(/bin/echo -n <(android_toolchain)/*-readelf)',

    # Determines whether we should optimize JNI generation at the cost of
    # breaking assumptions in the build system that when inputs have changed
    # the outputs should always change as well.  This is meant purely for
    # developer builds, to avoid spurious re-linking of native files.
    'optimize_jni_generation%': 0,
  },
  # END CODE TAKEN FROM CHROME'S COMMON.GYPI
  # Hardcode the compiler names in the Makefile so that
  # it won't depend on the environment at make time.
  'make_global_settings': [
    ['CC', '<!(/bin/echo -n <(android_toolchain)/*-gcc)'],
    ['CXX', '<!(/bin/echo -n <(android_toolchain)/*-g++)'],
    ['CC.host', '<!(which gcc)'],
    ['CXX.host', '<!(which g++)'],
  ],
  'target_defaults': {
    'target_conditions': [
      # Settings for building device targets using Android's toolchain.
      # These are based on the setup.mk file from the Android NDK.
      #
      # The NDK Android executable link step looks as follows:
      #  $LDFLAGS
      #  $(TARGET_CRTBEGIN_DYNAMIC_O)  <-- crtbegin.o
      #  $(PRIVATE_OBJECTS)            <-- The .o that we built
      #  $(PRIVATE_STATIC_LIBRARIES)   <-- The .a that we built
      #  $(TARGET_LIBGCC)              <-- libgcc.a
      #  $(PRIVATE_SHARED_LIBRARIES)   <-- The .so that we built
      #  $(PRIVATE_LDLIBS)             <-- System .so
      #  $(TARGET_CRTEND_O)            <-- crtend.o
      #
      # For now the above are approximated for executables by adding
      # crtbegin.o to the end of the ldflags and 'crtend.o' to the end
      # of 'libraries'.
      #
      # The NDK Android shared library link step looks as follows:
      #  $LDFLAGS
      #  $(PRIVATE_OBJECTS)            <-- The .o that we built
      #  -l,--whole-archive
      #  $(PRIVATE_WHOLE_STATIC_LIBRARIES)
      #  -l,--no-whole-archive
      #  $(PRIVATE_STATIC_LIBRARIES)   <-- The .a that we built
      #  $(TARGET_LIBGCC)              <-- libgcc.a
      #  $(PRIVATE_SHARED_LIBRARIES)   <-- The .so that we built
      #  $(PRIVATE_LDLIBS)             <-- System .so
      #
      # For now, assume that whole static libraries are not needed.
      #
      # For both executables and shared libraries, add the proper
      # libgcc.a to the start of libraries which puts it in the
      # proper spot after .o and .a files get linked in.
      #
      # TODO: The proper thing to do longer-tem would be proper gyp
      # support for a custom link command line.
      ['_toolset=="target"', {
        # NOTE: The stlport header include paths below are specified in
        # cflags rather than include_dirs because they need to come
        # after include_dirs. This is because they notably contain stddef.h
        # that define things incompatibly.
        'cflags': [
          '--sysroot=<(android_ndk_sysroot)',
          '-I<(android_stlport_include)',
          '-fPIE',
          '-fvisibility=default',
          '-ffunction-sections',
          '-funwind-tables',
          '-fstack-protector',
          '-fno-short-enums',
          '-finline-limit=64',
          '-Wa,--noexecstack',
        ],
        'defines': [
          'ANDROID',
          '__GNU_SOURCE=1',  # Necessary for strerror_r()
          'USE_STLPORT=1',
          '_STLP_USE_PTR_SPECIALIZATIONS=1',
        ],
        'ldflags!': ['-pthread'],
        'ldflags': [
          '-Bdynamic',
          '-Wl,--gc-sections',
          '-Wl,-z,nocopyreloc',
          '-pie',
          '-rdynamic',
          '--sysroot=<(android_ndk_sysroot)',
          '-nostdlib',
          '-Wl,--no-undefined',
          '-L<(android_stlport_root)/libs/<(android_app_abi)',
          # Don't export symbols from statically linked libraries.
          '-Wl,--exclude-libs=ALL',
          # crtbegin_dynamic.o should be the last item in ldflags.
          '<(android_ndk_lib)/crtbegin_dynamic.o',
        ],
        'libraries': [
          '-lstlport_shared',
          # Manually link the libgcc.a that the cross compiler uses.
          '<!(<(android_toolchain)/*-gcc -print-libgcc-file-name)',
          '-lc',
          '-ldl',
          '-lm',
          # crtend_android.o needs to be the last item in libraries.
          # Do not add any libraries after this!
          '<(android_ndk_lib)/crtend_android.o',
        ],
      }],
    ],
  }
}
