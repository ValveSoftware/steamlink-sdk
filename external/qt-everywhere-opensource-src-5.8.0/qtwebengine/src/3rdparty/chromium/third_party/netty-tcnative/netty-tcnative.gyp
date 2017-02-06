# Builds the Netty fork of Tomcat Native. See http://netty.io/wiki/forked-tomcat-native.html
{
  'targets': [
    {
      'target_name': 'netty-tcnative-so',
      'product_name': 'netty-tcnative',
      'type': 'shared_library',
      'sources': [
        'src/c/address.c',
        'src/c/bb.c',
        'src/c/dir.c',
        'src/c/error.c',
        'src/c/file.c',
        'src/c/info.c',
        'src/c/jnilib.c',
        'src/c/lock.c',
        'src/c/misc.c',
        'src/c/mmap.c',
        'src/c/multicast.c',
        'src/c/network.c',
        'src/c/os.c',
        'src/c/os_unix_system.c',
        'src/c/os_unix_uxpipe.c',
        'src/c/poll.c',
        'src/c/pool.c',
        'src/c/proc.c',
        'src/c/shm.c',
        'src/c/ssl.c',
        'src/c/sslcontext.c',
        'src/c/sslinfo.c',
        'src/c/sslnetwork.c',
        'src/c/ssl_private.h',
        'src/c/sslutils.c',
        'src/c/stdlib.c',
        'src/c/tcn_api.h',
        'src/c/tcn.h',
        'src/c/tcn_version.h',
        'src/c/thread.c',
        'src/c/user.c',
      ],
      'include_dirs': [
        '../apache-portable-runtime/src/include',
      ],
      'defines': [
        'HAVE_OPENSSL',
      ],
      'cflags': [
        '-w',
      ],
      'dependencies': [
        '../apache-portable-runtime/apr.gyp:apr',
        '../boringssl/boringssl.gyp:boringssl',
      ],
      'variables': {
        'use_native_jni_exports': 1,
      },
    },
    {
      'target_name': 'netty-tcnative',
      'type': 'none',
      'variables': {
        'java_in_dir': 'src/java',
        'javac_includes': [ '**/org/apache/tomcat/jni/*.java' ],
        'run_findbugs': 0,
      },
      'includes': [ '../../build/java.gypi' ],
      'dependencies': [
        'netty-tcnative-so',
        'rename_netty_tcnative_so_file',
      ],
      'export_dependent_settings': [
        'rename_netty_tcnative_so_file',
      ],
    },
    {
      # libnetty-tcnative shared library should have a specific name when
      # it is copied to the test APK. This target renames (actually makes
      # a copy of) the 'so' file if it has a different name.
      'target_name': 'rename_netty_tcnative_so_file',
      'type': 'none',
      'conditions': [
        ['component=="shared_library"', {
          'actions': [
          {
            'action_name': 'copy',
            'inputs': ['<(PRODUCT_DIR)/lib/libnetty-tcnative.cr.so'],
            'outputs': ['<(PRODUCT_DIR)/lib/libnetty-tcnative.so'],
            'action': [
              'cp',
              '<@(_inputs)',
              '<@(_outputs)',
            ],
          }],
       }],
      ],
      'dependencies': [
        'netty-tcnative-so',
      ],
      'direct_dependent_settings': {
        'variables': {
          'netty_tcnative_so_file_location': '<(PRODUCT_DIR)/lib/libnetty-tcnative.so',
        },
      },
    },
  ],
}