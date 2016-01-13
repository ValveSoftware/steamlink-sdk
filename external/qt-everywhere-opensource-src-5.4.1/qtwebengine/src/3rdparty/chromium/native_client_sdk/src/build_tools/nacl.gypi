{
  # gyp only supports two toolchains: host and target.
  # In order to get around this limitation we use a compiler
  # wrapper than will then invoke either the glibc or the newlib
  # compiler.
  'make_global_settings': [
    ['CC.target'   , '../../toolchain/linux_x86/bin/i686-nacl-gcc'],
    ['CXX.target'  , '../../toolchain/linux_x86/bin/i686-nacl-g++'],
    ['LINK.target' , '../../toolchain/linux_x86/bin/i686-nacl-g++'],
    ['AR.target'   , '../../toolchain/linux_x86/bin/i686-nacl-ar'],
  ],

  'variables': {
    'EXECUTABLE_SUFFIX': '.nexe',
    'TOOLROOT': '../../toolchain',
    'OBJDUMP': '<(TOOLROOT)/linux_x86_glibc/bin/i686-nacl-objdump',
    'NMF_PATH1': '<(TOOLROOT)/linux_x86_glibc/x86_64-nacl/lib32',
    'NMF_PATH2': '<(TOOLROOT)/linux_x86_glibc/x86_64-nacl/lib',
  },

  'target_defaults': {
    'link_settings': { 'ldflags': ['-Wl,-as-needed'] },
    'libraries' : ['-lppapi', '-lppapi_cpp'],
    'ldflags': ['-pthread'],
    'cflags': ['-pthread', '-Wno-long-long', '-Wall', '-Wswitch-enum', '-Werror'],
  },
}
