use_relative_paths = True

vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'pdfium_git': 'https://pdfium.googlesource.com',

  'android_ndk_revision': '5022f40f12953c02b2614c5f7beb981ec5d0e833',
  'build_revision': '9bbfcd5cce0290376e46f3aeaca3475e96a67fdb',
  'buildtools_revision': '099f1da55bfe8caa12266371a7eb983698fb1d87',
  'catapult_revision': '8ff23141e1df724317e9f558644ce2a7d8afde54',
  'clang_revision': 'b6f620b311665e2d96d0921833f54295b9bbf925',
  'cygwin_revision': 'c89e446b273697fadf3a10ff1007a97c0b7de6df',
  'gen_library_loader_revision': '916d4acd8b2cde67a390737dfba90b3c37de23a1',
  'gmock_revision': '29763965ab52f24565299976b936d1265cb6a271',
  'gtest_revision': '8245545b6dc9c4703e6496d1efd19e975ad2b038',
  'icu_revision': 'c291cde264469b20ca969ce8832088acb21e0c48',
  'pdfium_tests_revision': '6c769320872e6ca82da4adaec1a497237f71b543',
  'skia_revision': '7942f22c607caf826a6a609b89338a569d0a30e7',
  'tools_memory_revision': '427f10475e1a8d72424c29d00bf689122b738e5d',
  'trace_event_revision': '54b8455be9505c2cb0cf5c26bb86739c236471aa',
  'v8_revision': '453b746d88e3d047d3f6c4363f0fe42b6bd13638',
}

deps = {
  "build":
    Var('chromium_git') + "/chromium/src/build.git@" + Var('build_revision'),

  "buildtools":
    Var('chromium_git') + "/chromium/buildtools.git@" + Var('buildtools_revision'),

  "testing/corpus":
    Var('pdfium_git') + "/pdfium_tests@" + Var('pdfium_tests_revision'),

  "testing/gmock":
    Var('chromium_git') + "/external/googlemock.git@" + Var('gmock_revision'),

  "testing/gtest":
    Var('chromium_git') + "/external/googletest.git@" + Var('gtest_revision'),

  "third_party/icu":
    Var('chromium_git') + "/chromium/deps/icu.git@" + Var('icu_revision'),

  "third_party/skia":
    Var('chromium_git') + '/skia.git' + '@' +  Var('skia_revision'),

  "tools/clang":
    Var('chromium_git') + "/chromium/src/tools/clang@" +  Var('clang_revision'),

  "tools/generate_library_loader":
    Var('chromium_git') + "/chromium/src/tools/generate_library_loader@" +
        Var('gen_library_loader_revision'),

  "tools/gyp":
    Var('chromium_git') + "/external/gyp",

  "tools/memory":
    Var('chromium_git') + "/chromium/src/tools/memory@" +
        Var('tools_memory_revision'),

  "v8":
    Var('chromium_git') + "/v8/v8.git@" + Var('v8_revision'),

  "v8/base/trace_event/common":
    Var('chromium_git') + "/chromium/src/base/trace_event/common.git@" +
        Var('trace_event_revision'),
}

deps_os = {
  "android": {
    "third_party/android_ndk":
      Var('chromium_git') + "/android_ndk.git@" + Var('android_ndk_revision'),
    "third_party/catapult":
      Var('chromium_git') + "/external/github.com/catapult-project/catapult.git@" + Var('catapult_revision'),
  },
  "win": {
    "v8/third_party/cygwin":
      Var('chromium_git') + "/chromium/deps/cygwin@" + Var('cygwin_revision'),
  },
}

include_rules = [
  # Basic stuff that everyone can use.
  # Note: public is not here because core cannot depend on public.
  '+testing',
  '+third_party/base',
]

specific_include_rules = {
  # Allow embedder tests to use public APIs.
  "(.*embeddertest\.cpp)": [
      "+public",
  ]
}

hooks = [
  # Pull GN binaries. This needs to be before running GYP below.
  {
    'name': 'gn_win',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'pdfium/buildtools/win/gn.exe.sha1',
    ],
  },
  {
    'name': 'gn_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'pdfium/buildtools/mac/gn.sha1',
    ],
  },
  {
    'name': 'gn_linux64',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-gn',
                '-s', 'pdfium/buildtools/linux64/gn.sha1',
    ],
  },
  {
    # Downloads the current stable linux sysroot to build/linux/ if needed.
    # This sysroot updates at about the same rate that the chrome build deps
    # change. This script is a no-op except for linux users who are doing
    # official chrome builds or cross compiling.
    'name': 'sysroot',
    'pattern': '.',
    'action': ['python',
               'pdfium/build/linux/sysroot_scripts/install-sysroot.py',
               '--running-as-hook'
    ],
  },
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    'name': 'gyp',
    'pattern': '.',
    'action': ['python', 'pdfium/build_gyp/gyp_pdfium'],
  },
  # Pull clang-format binaries using checked-in hashes.
  {
    'name': 'clang_format_win',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=win32',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'pdfium/buildtools/win/clang-format.exe.sha1',
    ],
  },
  {
    'name': 'clang_format_mac',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=darwin',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'pdfium/buildtools/mac/clang-format.sha1',
    ],
  },
  {
    'name': 'clang_format_linux',
    'pattern': '.',
    'action': [ 'download_from_google_storage',
                '--no_resume',
                '--platform=linux*',
                '--no_auth',
                '--bucket', 'chromium-clang-format',
                '-s', 'pdfium/buildtools/linux64/clang-format.sha1',
    ],
  },
  {
    # Pull clang if needed or requested via GYP_DEFINES.
    'name': 'clang',
    'pattern': '.',
    'action': ['python',
               'pdfium/tools/clang/scripts/update.py',
               '--if-needed'
    ],
  },
  {
    # Update the Windows toolchain if necessary.
    'name': 'win_toolchain',
    'pattern': '.',
    'action': ['python', 'pdfium/build/vs_toolchain.py', 'update'],
  },
]
