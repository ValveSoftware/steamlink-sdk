# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,

    'linux_link_kerberos%': 0,
    'use_tracing_cache_backend%': 0,
    'conditions': [
      ['chromeos==1 or embedded==1 or OS=="android" or OS=="ios"', {
        # Disable Kerberos on ChromeOS, Android and iOS, at least for now.
        # It needs configuration (krb5.conf and so on).
        'use_kerberos%': 0,
      }, {  # chromeos == 0 and embedded==0 and OS!="android" and OS!="ios"
        'use_kerberos%': 1,
      }],
      ['OS=="android" and target_arch != "ia32"', {
        # The way the cache uses mmap() is inefficient on some Android devices.
        # If this flag is set, we hackily avoid using mmap() in the disk cache.
        # We are pretty confident that mmap-ing the index would not hurt any
        # existing x86 android devices, but we cannot be so sure about the
        # variety of ARM devices. So enable it for x86 only for now.
        'posix_avoid_mmap%': 1,
      }, {
        'posix_avoid_mmap%': 0,
      }],
      ['OS=="ios"', {
        # Websockets and socket stream are not used on iOS.
        'enable_websockets%': 0,
        # iOS does not use V8.
        'use_v8_in_net%': 0,
        'enable_built_in_dns%': 0,
      }, {
        'enable_websockets%': 1,
        'use_v8_in_net%': 1,
        'enable_built_in_dns%': 1,
      }],
    ],
  },
  'includes': [
    '../build/win_precompile.gypi',
    'net.gypi',
  ],
  'targets': [
    {
      'target_name': 'net_derived_sources',
      'type': 'none',
      'sources': [
        'base/registry_controlled_domains/effective_tld_names.gperf',
        'base/registry_controlled_domains/effective_tld_names_unittest1.gperf',
        'base/registry_controlled_domains/effective_tld_names_unittest2.gperf',
        'base/registry_controlled_domains/effective_tld_names_unittest3.gperf',
        'base/registry_controlled_domains/effective_tld_names_unittest4.gperf',
        'base/registry_controlled_domains/effective_tld_names_unittest5.gperf',
        'base/registry_controlled_domains/effective_tld_names_unittest6.gperf',
      ],
      'rules': [
        {
          'rule_name': 'dafsa',
          'extension': 'gperf',
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/net/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-inc.cc',
          ],
          'inputs': [
            'tools/tld_cleanup/make_dafsa.py',
          ],
          'action': [
            'python',
            'tools/tld_cleanup/make_dafsa.py',
            '<(RULE_INPUT_PATH)',
            '<(SHARED_INTERMEDIATE_DIR)/net/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-inc.cc',
          ],
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)'
        ],
      },
    },
    {
      'target_name': 'net',
      'type': '<(component)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../crypto/crypto.gyp:crypto',
        '../sdch/sdch.gyp:sdch',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/zlib/zlib.gyp:zlib',
        '../url/url.gyp:url_lib',
        'net_derived_sources',
        'net_resources',
      ],
      'sources': [
        '<@(net_nacl_common_sources)',
        '<@(net_non_nacl_sources)',
      ],
      'defines': [
        'NET_IMPLEMENTATION',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
      ],
      'conditions': [
        ['chromeos==1', {
          'sources!': [
             'base/network_change_notifier_linux.cc',
             'base/network_change_notifier_linux.h',
             'base/network_change_notifier_netlink_linux.cc',
             'base/network_change_notifier_netlink_linux.h',
             'proxy/proxy_config_service_linux.cc',
             'proxy/proxy_config_service_linux.h',
          ],
        }],
        ['use_kerberos==1', {
          'defines': [
            'USE_KERBEROS',
          ],
          'conditions': [
            ['OS=="openbsd"', {
              'include_dirs': [
                '/usr/include/kerberosV'
              ],
            }],
            ['linux_link_kerberos==1', {
              'link_settings': {
                'ldflags': [
                  '<!@(krb5-config --libs gssapi)',
                ],
              },
            }, { # linux_link_kerberos==0
              'defines': [
                'DLOPEN_KERBEROS',
              ],
            }],
          ],
        }, { # use_kerberos == 0
          'sources!': [
            'http/http_auth_gssapi_posix.cc',
            'http/http_auth_gssapi_posix.h',
            'http/http_auth_handler_negotiate.h',
            'http/http_auth_handler_negotiate.cc',
          ],
        }],
        ['posix_avoid_mmap==1', {
          'defines': [
            'POSIX_AVOID_MMAP',
          ],
          'direct_dependent_settings': {
            'defines': [
              'POSIX_AVOID_MMAP',
            ],
          },
          'sources!': [
            'disk_cache/blockfile/mapped_file_posix.cc',
          ],
        }, { # else
          'sources!': [
            'disk_cache/blockfile/mapped_file_avoid_mmap_posix.cc',
          ],
        }],
        ['disable_file_support==1', {
          # TODO(mmenke):  Should probably get rid of the dependency on
          # net_resources in this case (It's used in net_util, to format
          # directory listings.  Also used outside of net/).
          'sources!': [
            'base/directory_lister.cc',
            'base/directory_lister.h',
            'url_request/url_request_file_dir_job.cc',
            'url_request/url_request_file_dir_job.h',
            'url_request/url_request_file_job.cc',
            'url_request/url_request_file_job.h',
            'url_request/file_protocol_handler.cc',
            'url_request/file_protocol_handler.h',
          ],
        }],
        ['disable_ftp_support==1', {
          'sources/': [
            ['exclude', '^ftp/'],
          ],
          'sources!': [
            'url_request/ftp_protocol_handler.cc',
            'url_request/ftp_protocol_handler.h',
            'url_request/url_request_ftp_job.cc',
            'url_request/url_request_ftp_job.h',
          ],
        }],
        ['enable_built_in_dns==1', {
          'defines': [
            'ENABLE_BUILT_IN_DNS',
          ]
        }, { # else
          'sources!': [
            'dns/address_sorter_posix.cc',
            'dns/address_sorter_posix.h',
            'dns/dns_client.cc',
          ],
        }],
        ['use_tracing_cache_backend==1', {
          'defines': [
            'USE_TRACING_CACHE_BACKEND'
          ],
         }],
        ['use_openssl==1', {
            'sources!': [
              'base/crypto_module_nss.cc',
              'base/keygen_handler_nss.cc',
              'base/nss_memio.c',
              'base/nss_memio.h',
              'cert/cert_database_nss.cc',
              'cert/cert_verify_proc_nss.cc',
              'cert/cert_verify_proc_nss.h',
              'cert/ct_log_verifier_nss.cc',
              'cert/ct_objects_extractor_nss.cc',
              'cert/jwk_serializer_nss.cc',
              'cert/nss_cert_database.cc',
              'cert/nss_cert_database.h',
              'cert/nss_cert_database_chromeos.cc',
              'cert/nss_cert_database_chromeos.h',
              'cert/nss_profile_filter_chromeos.cc',
              'cert/nss_profile_filter_chromeos.h',
              'cert/scoped_nss_types.h',
              'cert/test_root_certs_nss.cc',
              'cert/x509_certificate_nss.cc',
              'cert/x509_util_nss.cc',
              'cert/x509_util_nss.h',
              'ocsp/nss_ocsp.cc',
              'ocsp/nss_ocsp.h',
              'quic/crypto/aead_base_decrypter_nss.cc',
              'quic/crypto/aead_base_encrypter_nss.cc',
              'quic/crypto/aes_128_gcm_12_decrypter_nss.cc',
              'quic/crypto/aes_128_gcm_12_encrypter_nss.cc',
              'quic/crypto/chacha20_poly1305_decrypter_nss.cc',
              'quic/crypto/chacha20_poly1305_encrypter_nss.cc',
              'quic/crypto/channel_id_nss.cc',
              'quic/crypto/p256_key_exchange_nss.cc',
              'socket/nss_ssl_util.cc',
              'socket/nss_ssl_util.h',
              'socket/ssl_client_socket_nss.cc',
              'socket/ssl_client_socket_nss.h',
              'socket/ssl_server_socket_nss.cc',
              'socket/ssl_server_socket_nss.h',
              'third_party/mozilla_security_manager/nsKeygenHandler.cpp',
              'third_party/mozilla_security_manager/nsKeygenHandler.h',
              'third_party/mozilla_security_manager/nsNSSCertificateDB.cpp',
              'third_party/mozilla_security_manager/nsNSSCertificateDB.h',
              'third_party/mozilla_security_manager/nsPKCS12Blob.cpp',
              'third_party/mozilla_security_manager/nsPKCS12Blob.h',
            ],
            'dependencies': [
              '../third_party/openssl/openssl.gyp:openssl',
            ],
          },
          {  # else !use_openssl: remove the unneeded files
            'sources!': [
              'base/crypto_module_openssl.cc',
              'cert/ct_log_verifier_openssl.cc',
              'cert/ct_objects_extractor_openssl.cc',
              'cert/jwk_serializer_openssl.cc',
              'cert/x509_util_openssl.cc',
              'cert/x509_util_openssl.h',
              'quic/crypto/aead_base_decrypter_openssl.cc',
              'quic/crypto/aead_base_encrypter_openssl.cc',
              'quic/crypto/aes_128_gcm_12_decrypter_openssl.cc',
              'quic/crypto/aes_128_gcm_12_encrypter_openssl.cc',
              'quic/crypto/chacha20_poly1305_decrypter_openssl.cc',
              'quic/crypto/chacha20_poly1305_encrypter_openssl.cc',
              'quic/crypto/channel_id_openssl.cc',
              'quic/crypto/p256_key_exchange_openssl.cc',
              'quic/crypto/scoped_evp_aead_ctx.cc',
              'quic/crypto/scoped_evp_aead_ctx.h',
              'socket/openssl_ssl_util.cc',
              'socket/openssl_ssl_util.h',
              'socket/ssl_client_socket_openssl.cc',
              'socket/ssl_client_socket_openssl.h',
              'socket/ssl_server_socket_openssl.cc',
              'socket/ssl_server_socket_openssl.h',
              'socket/ssl_session_cache_openssl.cc',
              'socket/ssl_session_cache_openssl.h',
            ],
          },
        ],
        [ 'use_openssl_certs == 0', {
            'sources!': [
              'base/keygen_handler_openssl.cc',
              'base/openssl_private_key_store.h',
              'base/openssl_private_key_store_android.cc',
              'base/openssl_private_key_store_memory.cc',
              'cert/cert_database_openssl.cc',
              'cert/cert_verify_proc_openssl.cc',
              'cert/cert_verify_proc_openssl.h',
              'cert/test_root_certs_openssl.cc',
              'cert/x509_certificate_openssl.cc',
              'ssl/openssl_client_key_store.cc',
              'ssl/openssl_client_key_store.h',
            ],
        }],
        [ 'use_glib == 1', {
            'dependencies': [
              '../build/linux/system.gyp:gconf',
              '../build/linux/system.gyp:gio',
            ],
        }],
        [ 'desktop_linux == 1 or chromeos == 1 or qt_os == "embedded_linux"', {
            'conditions': [
              ['use_openssl == 0', {
                 # use NSS
                'dependencies': [
                  '../build/linux/system.gyp:ssl',
                ],
              }],
              ['os_bsd==1', {
                'sources!': [
                  'base/network_change_notifier_linux.cc',
                  'base/network_change_notifier_netlink_linux.cc',
                  'proxy/proxy_config_service_linux.cc',
                ],
              },{
                'dependencies': [
                  '../build/linux/system.gyp:libresolv',
                ],
              }],
              ['OS=="solaris"', {
                'link_settings': {
                  'ldflags': [
                    '-R/usr/lib/mps',
                  ],
                },
              }],
            ],
          },
          {  # else: OS is not in the above list
            'sources!': [
              'base/crypto_module_nss.cc',
              'base/keygen_handler_nss.cc',
              'cert/cert_database_nss.cc',
              'cert/nss_cert_database.cc',
              'cert/nss_cert_database.h',
              'cert/test_root_certs_nss.cc',
              'cert/x509_certificate_nss.cc',
              'ocsp/nss_ocsp.cc',
              'ocsp/nss_ocsp.h',
              'third_party/mozilla_security_manager/nsKeygenHandler.cpp',
              'third_party/mozilla_security_manager/nsKeygenHandler.h',
              'third_party/mozilla_security_manager/nsNSSCertificateDB.cpp',
              'third_party/mozilla_security_manager/nsNSSCertificateDB.h',
              'third_party/mozilla_security_manager/nsPKCS12Blob.cpp',
              'third_party/mozilla_security_manager/nsPKCS12Blob.h',
            ],
          },
        ],
        [ 'use_nss != 1', {
            'sources!': [
              'cert/cert_verify_proc_nss.cc',
              'cert/cert_verify_proc_nss.h',
              'ssl/client_cert_store_nss.cc',
              'ssl/client_cert_store_nss.h',
              'ssl/client_cert_store_chromeos.cc',
              'ssl/client_cert_store_chromeos.h',
            ],
        }],
        [ 'enable_websockets != 1', {
            'sources/': [
              ['exclude', '^socket_stream/'],
              ['exclude', '^websockets/'],
            ],
            'sources!': [
              'spdy/spdy_websocket_stream.cc',
              'spdy/spdy_websocket_stream.h',
            ],
        }],
        [ 'enable_mdns != 1', {
            'sources!' : [
              'dns/mdns_cache.cc',
              'dns/mdns_cache.h',
              'dns/mdns_client.cc',
              'dns/mdns_client.h',
              'dns/mdns_client_impl.cc',
              'dns/mdns_client_impl.h',
              'dns/record_parsed.cc',
              'dns/record_parsed.h',
              'dns/record_rdata.cc',
              'dns/record_rdata.h',
            ]
        }],
        [ 'OS == "win"', {
            'sources!': [
              'http/http_auth_handler_ntlm_portable.cc',
              'socket/tcp_socket_libevent.cc',
              'socket/tcp_socket_libevent.h',
              'udp/udp_socket_libevent.cc',
              'udp/udp_socket_libevent.h',
            ],
            'dependencies': [
              '../third_party/nss/nss.gyp:nspr',
              '../third_party/nss/nss.gyp:nss',
              'third_party/nss/ssl.gyp:libssl',
            ],
            # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
            'msvs_disabled_warnings': [4267, ],
          }, { # else: OS != "win"
            'sources!': [
              'base/winsock_init.cc',
              'base/winsock_init.h',
              'base/winsock_util.cc',
              'base/winsock_util.h',
              'proxy/proxy_resolver_winhttp.cc',
              'proxy/proxy_resolver_winhttp.h',
            ],
          },
        ],
        [ 'OS == "mac"', {
            'conditions': [
              [ 'use_openssl == 0', {
                'dependencies': [
                  # defaults to nss
                  '../third_party/nss/nss.gyp:nspr',
                  '../third_party/nss/nss.gyp:nss',
                  'third_party/nss/ssl.gyp:libssl',
                ],
              }],
            ],
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
                '$(SDKROOT)/System/Library/Frameworks/Security.framework',
                '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
                '$(SDKROOT)/usr/lib/libresolv.dylib',
              ]
            },
          },
        ],
        [ 'OS == "ios"', {
            'dependencies': [
              '../third_party/nss/nss.gyp:nss',
              'third_party/nss/ssl.gyp:libssl',
            ],
            'sources!': [
              'disk_cache/blockfile/file_posix.cc',
            ],
            'link_settings': {
              'libraries': [
                '$(SDKROOT)/System/Library/Frameworks/CFNetwork.framework',
                '$(SDKROOT)/System/Library/Frameworks/MobileCoreServices.framework',
                '$(SDKROOT)/System/Library/Frameworks/Security.framework',
                '$(SDKROOT)/System/Library/Frameworks/SystemConfiguration.framework',
                '$(SDKROOT)/usr/lib/libresolv.dylib',
              ],
            },
          },
        ],
        ['OS=="android" and _toolset=="target" and android_webview_build == 0', {
          'dependencies': [
             'net_java',
          ],
        }],
        [ 'OS == "android"', {
            'dependencies': [
              '../third_party/openssl/openssl.gyp:openssl',
              'net_jni_headers',
            ],
            'sources!': [
              'base/openssl_private_key_store_memory.cc',
              'cert/cert_database_openssl.cc',
              'cert/cert_verify_proc_openssl.cc',
              'cert/test_root_certs_openssl.cc',
            ],
            # The net/android/keystore_openssl.cc source file needs to
            # access an OpenSSL-internal header.
            'include_dirs': [
              '../third_party/openssl',
            ],
          },
        ],
        [ 'use_icu_alternatives_on_android == 1', {
            'dependencies!': [
              '../base/base.gyp:base_i18n',
              '../third_party/icu/icu.gyp:icui18n',
              '../third_party/icu/icu.gyp:icuuc',
            ],
            'sources!': [
              'base/filename_util_icu.cc',
              'base/net_string_util_icu.cc',
              'base/net_util_icu.cc',
            ],
            'sources': [
              'base/net_string_util_icu_alternatives_android.cc',
              'base/net_string_util_icu_alternatives_android.h',
            ],
          },
        ],
      ],
      'target_conditions': [
        # These source files are excluded by default platform rules, but they
        # are needed in specific cases on other platforms. Re-including them can
        # only be done in target_conditions as it is evaluated after the
        # platform rules.
        ['OS == "android"', {
          'sources/': [
            ['include', '^base/platform_mime_util_linux\\.cc$'],
            ['include', '^base/address_tracker_linux\\.cc$'],
            ['include', '^base/address_tracker_linux\\.h$'],
          ],
        }],
        ['OS == "ios"', {
          'sources/': [
            ['include', '^base/network_change_notifier_mac\\.cc$'],
            ['include', '^base/network_config_watcher_mac\\.cc$'],
            ['include', '^base/platform_mime_util_mac\\.mm$'],
            # The iOS implementation only partially uses NSS and thus does not
            # defines |use_nss|. In particular the |USE_NSS| preprocessor
            # definition is not used. The following files are needed though:
            ['include', '^cert/cert_verify_proc_nss\\.cc$'],
            ['include', '^cert/cert_verify_proc_nss\\.h$'],
            ['include', '^cert/test_root_certs_nss\\.cc$'],
            ['include', '^cert/x509_util_nss\\.cc$'],
            ['include', '^cert/x509_util_nss\\.h$'],
            ['include', '^proxy/proxy_resolver_mac\\.cc$'],
            ['include', '^proxy/proxy_server_mac\\.cc$'],
            ['include', '^ocsp/nss_ocsp\\.cc$'],
            ['include', '^ocsp/nss_ocsp\\.h$'],
          ],
        }],
      ],
    },
    {
      'target_name': 'net_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../crypto/crypto.gyp:crypto',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/zlib/zlib.gyp:zlib',
        '../url/url.gyp:url_lib',
        'balsa',
        'http_server',
        'net',
        'net_derived_sources',
        'net_test_support',
      ],
      'sources': [
        '<@(net_test_sources)',
      ],
      'conditions': [
        ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android"', {
          'dependencies': [
            'epoll_server',
            'flip_in_mem_edsm_server_base',
            'quic_base',
          ],
          'sources': [
            '<@(net_linux_test_sources)',
          ],
        }],
        ['chromeos==1', {
          'sources!': [
            'base/network_change_notifier_linux_unittest.cc',
            'proxy/proxy_config_service_linux_unittest.cc',
          ],
        }],
        [ 'OS == "android"', {
          'sources!': [
            # See bug http://crbug.com/344533.
            'disk_cache/blockfile/index_table_v3_unittest.cc',
            # No res_ninit() et al on Android, so this doesn't make a lot of
            # sense.
            'dns/dns_config_service_posix_unittest.cc',
          ],
          'dependencies': [
            'net_javatests',
            'net_test_jni_headers',
          ],
        }],
        [ 'use_nss != 1', {
          'sources!': [
            'ssl/client_cert_store_nss_unittest.cc',
            'ssl/client_cert_store_chromeos_unittest.cc',
          ],
        }],
        [ 'use_openssl == 1', {
          # Avoid compiling/linking with the system library.
          'dependencies': [
            '../third_party/openssl/openssl.gyp:openssl',
          ],
        }, {  # use_openssl == 0
          'conditions': [
            [ 'desktop_linux == 1 or chromeos == 1', {
              'dependencies': [
                '../build/linux/system.gyp:ssl',
              ],
            }, {  # desktop_linux == 0 and chromeos == 0
              'sources!': [
                'cert/nss_cert_database_unittest.cc',
              ],
            }],
          ],
        }],
        [ 'os_posix == 1 and OS != "mac" and OS != "android" and OS != "ios"', {
          'conditions': [
            ['use_allocator!="none"', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        [ 'use_kerberos==1', {
          'defines': [
            'USE_KERBEROS',
          ],
        }, { # use_kerberos == 0
          'sources!': [
            'http/http_auth_gssapi_posix_unittest.cc',
            'http/http_auth_handler_negotiate_unittest.cc',
            'http/mock_gssapi_library_posix.cc',
            'http/mock_gssapi_library_posix.h',
          ],
        }],
        [ 'use_openssl == 1 or (desktop_linux == 0 and chromeos == 0 and OS != "ios")', {
          # Only include this test when on Posix and using NSS for
          # cert verification or on iOS (which also uses NSS for certs).
          'sources!': [
            'ocsp/nss_ocsp_unittest.cc',
          ],
        }],
        [ 'use_openssl==1', {
            # When building for OpenSSL, we need to exclude NSS specific tests
            # or functionality not supported by OpenSSL yet.
            # TODO(bulach): Add equivalent tests when the underlying
            #               functionality is ported to OpenSSL.
            'sources!': [
              'cert/ct_objects_extractor_unittest.cc',
              'cert/multi_log_ct_verifier_unittest.cc',
              'cert/nss_cert_database_unittest.cc',
              'cert/nss_cert_database_chromeos_unittest.cc',
              'cert/nss_profile_filter_chromeos_unittest.cc',
              'cert/x509_util_nss_unittest.cc',
              'quic/test_tools/crypto_test_utils_nss.cc',
            ],
          }, {  # else !use_openssl: remove the unneeded files
            'sources!': [
              'cert/x509_util_openssl_unittest.cc',
              'quic/test_tools/crypto_test_utils_openssl.cc',
              'socket/ssl_client_socket_openssl_unittest.cc',
              'socket/ssl_session_cache_openssl_unittest.cc',
            ],
          },
        ],
        [ 'use_openssl_certs == 0', {
            'sources!': [
              'ssl/openssl_client_key_store_unittest.cc',
            ],
        }],
        [ 'enable_websockets != 1', {
            'sources/': [
              ['exclude', '^socket_stream/'],
              ['exclude', '^websockets/'],
              ['exclude', '^spdy/spdy_websocket_stream_unittest\\.cc$'],
            ],
        }],
        ['disable_file_support==1', {
          'sources!': [
            'base/directory_lister_unittest.cc',
            'url_request/url_request_file_job_unittest.cc',
          ],
        }],
        [ 'disable_ftp_support==1', {
            'sources/': [
              ['exclude', '^ftp/'],
            ],
            'sources!': [
              'url_request/url_request_ftp_job_unittest.cc',
            ],
          },
        ],
        [ 'enable_built_in_dns!=1', {
            'sources!': [
              'dns/address_sorter_posix_unittest.cc',
              'dns/address_sorter_unittest.cc',
            ],
          },
        ],
        # Always need use_v8_in_net to be 1 to run gyp on Android, so just
        # remove net_unittest's dependency on v8 when using icu alternatives
        # instead of setting use_v8_in_net to 0.
        [ 'use_v8_in_net==1 and use_icu_alternatives_on_android==0', {
            'dependencies': [
              'net_with_v8',
            ],
          }, {  # else: !use_v8_in_net
            'sources!': [
              'proxy/proxy_resolver_v8_unittest.cc',
              'proxy/proxy_resolver_v8_tracing_unittest.cc',
            ],
          },
        ],

        [ 'enable_mdns != 1', {
            'sources!' : [
              'dns/mdns_cache_unittest.cc',
              'dns/mdns_client_unittest.cc',
              'dns/mdns_query_unittest.cc',
              'dns/record_parsed_unittest.cc',
              'dns/record_rdata_unittest.cc',
            ],
        }],
        [ 'OS == "win"', {
            'sources!': [
              'dns/dns_config_service_posix_unittest.cc',
              'http/http_auth_gssapi_posix_unittest.cc',
            ],
            'dependencies': [
              '../third_party/nss/nss.gyp:nspr',
              '../third_party/nss/nss.gyp:nss',
              'third_party/nss/ssl.gyp:libssl',
            ],
            'conditions': [
              [ 'icu_use_data_file_flag == 0', {
                # This is needed to trigger the dll copy step on windows.
                # TODO(mark): Specifying this here shouldn't be necessary.
                'dependencies': [
                  '../third_party/icu/icu.gyp:icudata',
                ],
              }],
            ],
            # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
            'msvs_disabled_warnings': [4267, ],
          },
        ],
        [ 'OS == "mac" and use_openssl == 0', {
            'dependencies': [
              '../third_party/nss/nss.gyp:nspr',
              '../third_party/nss/nss.gyp:nss',
              'third_party/nss/ssl.gyp:libssl',
            ],
          },
        ],
        [ 'OS == "ios"', {
            'dependencies': [
              '../third_party/nss/nss.gyp:nss',
            ],
            'actions': [
              {
                'action_name': 'copy_test_data',
                'variables': {
                  'test_data_files': [
                    'data/ssl/certificates/',
                    'data/test.html',
                    'data/url_request_unittest/',
                  ],
                  'test_data_prefix': 'net',
                },
                'includes': [ '../build/copy_test_data_ios.gypi' ],
              },
            ],
            'sources!': [
              # TODO(droger): The following tests are disabled because the
              # implementation is missing or incomplete.
              # KeygenHandler::GenKeyAndSignChallenge() is not ported to iOS.
              'base/keygen_handler_unittest.cc',
              'disk_cache/backend_unittest.cc',
              'disk_cache/blockfile/block_files_unittest.cc',
              # Need to read input data files.
              'filter/gzip_filter_unittest.cc',
              'socket/ssl_server_socket_unittest.cc',
              'spdy/fuzzing/hpack_fuzz_util_test.cc',
              # Need TestServer.
              'proxy/proxy_script_fetcher_impl_unittest.cc',
              'socket/ssl_client_socket_unittest.cc',
              'url_request/url_fetcher_impl_unittest.cc',
              'url_request/url_request_context_builder_unittest.cc',
              # Needs GetAppOutput().
              'test/python_utils_unittest.cc',

              # The following tests are disabled because they don't apply to
              # iOS.
              # OS is not "linux" or "freebsd" or "openbsd".
              'socket/unix_domain_socket_posix_unittest.cc',

              # See bug http://crbug.com/344533.
              'disk_cache/blockfile/index_table_v3_unittest.cc',
            ],
        }],
        [ 'OS == "android"', {
            'dependencies': [
              '../third_party/openssl/openssl.gyp:openssl',
            ],
            'sources!': [
              'dns/dns_config_service_posix_unittest.cc',
            ],
          },
        ],
        ['OS == "android"', {
          # TODO(mmenke):  This depends on test_support_base, which depends on
          #                icu.  Figure out a way to remove that dependency.
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ]
        }],
        [ 'use_icu_alternatives_on_android == 1', {
            'dependencies!': [
              '../base/base.gyp:base_i18n',
            ],
            'sources!': [
              'base/filename_util_unittest.cc',
              'base/net_util_icu_unittest.cc',
            ],
          },
        ],
      ],
      'target_conditions': [
        # These source files are excluded by default platform rules, but they
        # are needed in specific cases on other platforms. Re-including them can
        # only be done in target_conditions as it is evaluated after the
        # platform rules.
        ['OS == "android"', {
          'sources/': [
            ['include', '^base/address_tracker_linux_unittest\\.cc$'],
          ],
        }],
      ],
    },
    {
      'target_name': 'net_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_perf',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
        'net',
        'net_test_support',
      ],
      'sources': [
        'cookies/cookie_monster_perftest.cc',
        'disk_cache/blockfile/disk_cache_perftest.cc',
        'proxy/proxy_resolver_perftest.cc',
      ],
      'conditions': [
        [ 'use_v8_in_net==1', {
            'dependencies': [
              'net_with_v8',
            ],
          }, {  # else: !use_v8_in_net
            'sources!': [
              'proxy/proxy_resolver_perftest.cc',
            ],
          },
        ],
        [ 'OS == "win"', {
            'conditions': [
              [ 'icu_use_data_file_flag == 0', {
                # This is needed to trigger the dll copy step on windows.
                # TODO(mark): Specifying this here shouldn't be necessary.
                'dependencies': [
                  '../third_party/icu/icu.gyp:icudata',
                ],
              }],
            ],
            # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
            'msvs_disabled_warnings': [4267, ],
        }],
      ],
    },
    {
      'target_name': 'net_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../net/tools/tld_cleanup/tld_cleanup.gyp:tld_cleanup_util',
        '../testing/gtest.gyp:gtest',
        '../testing/gmock.gyp:gmock',
        '../url/url.gyp:url_lib',
        'net',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        # TODO(mmenke):  This depends on icu, figure out a way to build tests
        #                without icu.
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../testing/gmock.gyp:gmock',
      ],
      'sources': [
        'base/capturing_net_log.cc',
        'base/capturing_net_log.h',
        'base/load_timing_info_test_util.cc',
        'base/load_timing_info_test_util.h',
        'base/mock_file_stream.cc',
        'base/mock_file_stream.h',
        'base/test_completion_callback.cc',
        'base/test_completion_callback.h',
        'base/test_data_directory.cc',
        'base/test_data_directory.h',
        'cert/mock_cert_verifier.cc',
        'cert/mock_cert_verifier.h',
        'cookies/cookie_monster_store_test.cc',
        'cookies/cookie_monster_store_test.h',
        'cookies/cookie_store_test_callbacks.cc',
        'cookies/cookie_store_test_callbacks.h',
        'cookies/cookie_store_test_helpers.cc',
        'cookies/cookie_store_test_helpers.h',
        'disk_cache/disk_cache_test_base.cc',
        'disk_cache/disk_cache_test_base.h',
        'disk_cache/disk_cache_test_util.cc',
        'disk_cache/disk_cache_test_util.h',
        'dns/dns_test_util.cc',
        'dns/dns_test_util.h',
        'dns/mock_host_resolver.cc',
        'dns/mock_host_resolver.h',
        'dns/mock_mdns_socket_factory.cc',
        'dns/mock_mdns_socket_factory.h',
        'http/http_transaction_test_util.cc',
        'http/http_transaction_test_util.h',
        'proxy/mock_proxy_resolver.cc',
        'proxy/mock_proxy_resolver.h',
        'proxy/mock_proxy_script_fetcher.cc',
        'proxy/mock_proxy_script_fetcher.h',
        'proxy/proxy_config_service_common_unittest.cc',
        'proxy/proxy_config_service_common_unittest.h',
        'socket/socket_test_util.cc',
        'socket/socket_test_util.h',
        'test/cert_test_util.cc',
        'test/cert_test_util.h',
        'test/ct_test_util.cc',
        'test/ct_test_util.h',
        'test/embedded_test_server/embedded_test_server.cc',
        'test/embedded_test_server/embedded_test_server.h',
        'test/embedded_test_server/http_connection.cc',
        'test/embedded_test_server/http_connection.h',
        'test/embedded_test_server/http_request.cc',
        'test/embedded_test_server/http_request.h',
        'test/embedded_test_server/http_response.cc',
        'test/embedded_test_server/http_response.h',
        'test/net_test_suite.cc',
        'test/net_test_suite.h',
        'test/python_utils.cc',
        'test/python_utils.h',
        'test/spawned_test_server/base_test_server.cc',
        'test/spawned_test_server/base_test_server.h',
        'test/spawned_test_server/local_test_server_posix.cc',
        'test/spawned_test_server/local_test_server_win.cc',
        'test/spawned_test_server/local_test_server.cc',
        'test/spawned_test_server/local_test_server.h',
        'test/spawned_test_server/remote_test_server.cc',
        'test/spawned_test_server/remote_test_server.h',
        'test/spawned_test_server/spawned_test_server.h',
        'test/spawned_test_server/spawner_communicator.cc',
        'test/spawned_test_server/spawner_communicator.h',
        'url_request/test_url_fetcher_factory.cc',
        'url_request/test_url_fetcher_factory.h',
        'url_request/url_request_test_util.cc',
        'url_request/url_request_test_util.h',
      ],
      'conditions': [
        ['OS != "ios"', {
          'dependencies': [
            '../third_party/protobuf/protobuf.gyp:py_proto',
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "android" and OS != "ios"', {
          'conditions': [
            ['use_openssl==1', {
              'dependencies': [
                '../third_party/openssl/openssl.gyp:openssl',
              ],
            }, {
              'dependencies': [
                '../build/linux/system.gyp:ssl',
              ],
            }],
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "android" and OS != "ios"', {
          'conditions': [
            ['use_allocator!="none"', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        ['OS != "android"', {
          'sources!': [
            'test/spawned_test_server/remote_test_server.cc',
            'test/spawned_test_server/remote_test_server.h',
            'test/spawned_test_server/spawner_communicator.cc',
            'test/spawned_test_server/spawner_communicator.h',
          ],
        }],
        ['OS == "ios"', {
          'dependencies': [
            '../third_party/nss/nss.gyp:nss',
          ],
        }],
        [ 'use_v8_in_net==1', {
            'dependencies': [
              'net_with_v8',
            ],
          },
        ],
        [ 'enable_mdns != 1', {
            'sources!' : [
              'dns/mock_mdns_socket_factory.cc',
              'dns/mock_mdns_socket_factory.h'
            ]
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'net_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/net',
      },
      'actions': [
        {
          'action_name': 'net_resources',
          'variables': {
            'grit_grd_file': 'base/net_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'http_server',
      'type': 'static_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'dependencies': [
        '../base/base.gyp:base',
        'net',
      ],
      'sources': [
        'server/http_connection.cc',
        'server/http_connection.h',
        'server/http_server.cc',
        'server/http_server.h',
        'server/http_server_request_info.cc',
        'server/http_server_request_info.h',
        'server/http_server_response_info.cc',
        'server/http_server_response_info.h',
        'server/web_socket.cc',
        'server/web_socket.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'balsa',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        'net',
      ],
      'sources': [
        'tools/balsa/balsa_enums.h',
        'tools/balsa/balsa_frame.cc',
        'tools/balsa/balsa_frame.h',
        'tools/balsa/balsa_headers.cc',
        'tools/balsa/balsa_headers.h',
        'tools/balsa/balsa_headers_token_utils.cc',
        'tools/balsa/balsa_headers_token_utils.h',
        'tools/balsa/balsa_visitor_interface.h',
        'tools/balsa/http_message_constants.cc',
        'tools/balsa/http_message_constants.h',
        'tools/balsa/noop_balsa_visitor.h',
        'tools/balsa/simple_buffer.cc',
        'tools/balsa/simple_buffer.h',
        'tools/balsa/split.cc',
        'tools/balsa/split.h',
        'tools/balsa/string_piece_utils.h',
        'tools/quic/spdy_utils.cc',
        'tools/quic/spdy_utils.h',
      ],
    },
    {
      'target_name': 'dump_cache',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        'net',
        'net_test_support',
      ],
      'sources': [
        'tools/dump_cache/cache_dumper.cc',
        'tools/dump_cache/cache_dumper.h',
        'tools/dump_cache/dump_cache.cc',
        'tools/dump_cache/dump_files.cc',
        'tools/dump_cache/dump_files.h',
        'tools/dump_cache/simple_cache_dumper.cc',
        'tools/dump_cache/simple_cache_dumper.h',
        'tools/dump_cache/upgrade_win.cc',
        'tools/dump_cache/upgrade_win.h',
        'tools/dump_cache/url_to_filename_encoder.cc',
        'tools/dump_cache/url_to_filename_encoder.h',
        'tools/dump_cache/url_utilities.h',
        'tools/dump_cache/url_utilities.cc',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
  ],
  'conditions': [
    ['use_v8_in_net == 1', {
      'targets': [
        {
          'target_name': 'net_with_v8',
          'type': '<(component)',
          'variables': { 'enable_wexit_time_destructors': 1, },
          'dependencies': [
            '../base/base.gyp:base',
            '../gin/gin.gyp:gin',
            '../url/url.gyp:url_lib',
            '../v8/tools/gyp/v8.gyp:v8',
            'net'
          ],
          'defines': [
            'NET_IMPLEMENTATION',
          ],
          'sources': [
            'proxy/proxy_resolver_v8.cc',
            'proxy/proxy_resolver_v8.h',
            'proxy/proxy_resolver_v8_tracing.cc',
            'proxy/proxy_resolver_v8_tracing.h',
            'proxy/proxy_service_v8.cc',
            'proxy/proxy_service_v8.h',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
      ],
    }],
    ['OS != "ios" and OS != "android"', {
      'targets': [
        # iOS doesn't have the concept of simple executables, these targets
        # can't be compiled on the platform.
        {
          'target_name': 'crash_cache',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
            'net_test_support',
          ],
          'sources': [
            'tools/crash_cache/crash_cache.cc',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          'target_name': 'crl_set_dump',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
          ],
          'sources': [
            'tools/crl_set_dump/crl_set_dump.cc',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          'target_name': 'dns_fuzz_stub',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
          ],
          'sources': [
            'tools/dns_fuzz_stub/dns_fuzz_stub.cc',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          'target_name': 'gdig',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
          ],
          'sources': [
            'tools/gdig/file_net_log.cc',
            'tools/gdig/gdig.cc',
          ],
        },
        {
          'target_name': 'get_server_time',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../url/url.gyp:url_lib',
            'net',
          ],
          'sources': [
            'tools/get_server_time/get_server_time.cc',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          'target_name': 'hpack_example_generator',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
          ],
          'sources': [
            'spdy/fuzzing/hpack_example_generator.cc',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          'target_name': 'hpack_fuzz_mutator',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
          ],
          'sources': [
            'spdy/fuzzing/hpack_fuzz_mutator.cc',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          'target_name': 'hpack_fuzz_wrapper',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
          ],
          'sources': [
            'spdy/fuzzing/hpack_fuzz_wrapper.cc',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          'target_name': 'net_watcher',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
            'net_with_v8',
          ],
          'conditions': [
            [ 'use_glib == 1', {
                'dependencies': [
                  '../build/linux/system.gyp:gconf',
                  '../build/linux/system.gyp:gio',
                ],
              },
            ],
          ],
          'sources': [
            'tools/net_watcher/net_watcher.cc',
          ],
        },
        {
          'target_name': 'run_testserver',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../testing/gtest.gyp:gtest',
            'net_test_support',
          ],
          'sources': [
            'tools/testserver/run_testserver.cc',
          ],
        },
        {
          'target_name': 'stress_cache',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
            'net_test_support',
          ],
          'sources': [
            'disk_cache/blockfile/stress_cache.cc',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
        {
          'target_name': 'tld_cleanup',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_i18n',
            '../net/tools/tld_cleanup/tld_cleanup.gyp:tld_cleanup_util',
          ],
          'sources': [
            'tools/tld_cleanup/tld_cleanup.cc',
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
      ],
    }],
    ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android"', {
      'targets': [
        {
          'target_name': 'epoll_server',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
          ],
          'sources': [
            'tools/epoll_server/epoll_server.cc',
            'tools/epoll_server/epoll_server.h',
          ],
        },
        {
          'target_name': 'flip_in_mem_edsm_server_base',
          'type': 'static_library',
          'cflags': [
            '-Wno-deprecated',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../third_party/openssl/openssl.gyp:openssl',
            'balsa',
            'epoll_server',
            'net',
          ],
          'sources': [
            'tools/dump_cache/url_to_filename_encoder.cc',
            'tools/dump_cache/url_to_filename_encoder.h',
            'tools/dump_cache/url_utilities.h',
            'tools/dump_cache/url_utilities.cc',
            'tools/flip_server/acceptor_thread.h',
            'tools/flip_server/acceptor_thread.cc',
            'tools/flip_server/create_listener.cc',
            'tools/flip_server/create_listener.h',
            'tools/flip_server/constants.h',
            'tools/flip_server/flip_config.cc',
            'tools/flip_server/flip_config.h',
            'tools/flip_server/http_interface.cc',
            'tools/flip_server/http_interface.h',
            'tools/flip_server/loadtime_measurement.h',
            'tools/flip_server/mem_cache.h',
            'tools/flip_server/mem_cache.cc',
            'tools/flip_server/output_ordering.cc',
            'tools/flip_server/output_ordering.h',
            'tools/flip_server/ring_buffer.cc',
            'tools/flip_server/ring_buffer.h',
            'tools/flip_server/sm_connection.cc',
            'tools/flip_server/sm_connection.h',
            'tools/flip_server/sm_interface.h',
            'tools/flip_server/spdy_ssl.cc',
            'tools/flip_server/spdy_ssl.h',
            'tools/flip_server/spdy_interface.cc',
            'tools/flip_server/spdy_interface.h',
            'tools/flip_server/spdy_util.cc',
            'tools/flip_server/spdy_util.h',
            'tools/flip_server/streamer_interface.cc',
            'tools/flip_server/streamer_interface.h',
          ],
        },
        {
          'target_name': 'flip_in_mem_edsm_server_unittests',
          'type': 'executable',
          'dependencies': [
              '../testing/gtest.gyp:gtest',
              '../testing/gmock.gyp:gmock',
              '../third_party/openssl/openssl.gyp:openssl',
              'flip_in_mem_edsm_server_base',
              'net',
              'net_test_support',
          ],
          'sources': [
            'tools/flip_server/flip_test_utils.cc',
            'tools/flip_server/flip_test_utils.h',
            'tools/flip_server/http_interface_test.cc',
            'tools/flip_server/mem_cache_test.cc',
            'tools/flip_server/run_all_tests.cc',
            'tools/flip_server/spdy_interface_test.cc',
          ],
        },
        {
          'target_name': 'flip_in_mem_edsm_server',
          'type': 'executable',
          'cflags': [
            '-Wno-deprecated',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            'flip_in_mem_edsm_server_base',
            'net',
          ],
          'sources': [
            'tools/flip_server/flip_in_mem_edsm_server.cc',
          ],
        },
        {
          'target_name': 'quic_base',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../url/url.gyp:url_lib',
            'balsa',
            'epoll_server',
            'net',
          ],
          'sources': [
            'tools/quic/quic_client.cc',
            'tools/quic/quic_client.h',
            'tools/quic/quic_client_session.cc',
            'tools/quic/quic_client_session.h',
            'tools/quic/quic_default_packet_writer.cc',
            'tools/quic/quic_default_packet_writer.h',
            'tools/quic/quic_dispatcher.h',
            'tools/quic/quic_dispatcher.cc',
            'tools/quic/quic_epoll_clock.cc',
            'tools/quic/quic_epoll_clock.h',
            'tools/quic/quic_epoll_connection_helper.cc',
            'tools/quic/quic_epoll_connection_helper.h',
            'tools/quic/quic_in_memory_cache.cc',
            'tools/quic/quic_in_memory_cache.h',
            'tools/quic/quic_packet_writer_wrapper.cc',
            'tools/quic/quic_packet_writer_wrapper.h',
            'tools/quic/quic_server.cc',
            'tools/quic/quic_server.h',
            'tools/quic/quic_server_session.cc',
            'tools/quic/quic_server_session.h',
            'tools/quic/quic_socket_utils.cc',
            'tools/quic/quic_socket_utils.h',
            'tools/quic/quic_spdy_client_stream.cc',
            'tools/quic/quic_spdy_client_stream.h',
            'tools/quic/quic_spdy_server_stream.cc',
            'tools/quic/quic_spdy_server_stream.h',
            'tools/quic/quic_time_wait_list_manager.h',
            'tools/quic/quic_time_wait_list_manager.cc',
          ],
        },
        {
          'target_name': 'quic_client',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
            'quic_base',
          ],
          'sources': [
            'tools/quic/quic_client_bin.cc',
          ],
        },
        {
          'target_name': 'quic_server',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
            'quic_base',
          ],
          'sources': [
            'tools/quic/quic_server_bin.cc',
          ],
        },
      ]
    }],
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'net_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/net/AndroidCertVerifyResult.java',
            'android/java/src/org/chromium/net/AndroidKeyStore.java',
            'android/java/src/org/chromium/net/AndroidNetworkLibrary.java',
            'android/java/src/org/chromium/net/AndroidPrivateKey.java',
            'android/java/src/org/chromium/net/GURLUtils.java',
            'android/java/src/org/chromium/net/NetworkChangeNotifier.java',
            'android/java/src/org/chromium/net/ProxyChangeListener.java',
            'android/java/src/org/chromium/net/X509Util.java',
          ],
          'variables': {
            'jni_gen_package': 'net',
          },
          'includes': [ '../build/jni_generator.gypi' ],

          'conditions': [
            ['use_icu_alternatives_on_android==1', {
              'sources': [
                'android/java/src/org/chromium/net/NetStringUtil.java',
              ],
            }],
          ],
        },
        {
          'target_name': 'net_test_jni_headers',
          'type': 'none',
          'sources': [
            'android/javatests/src/org/chromium/net/AndroidKeyStoreTestUtil.java',
          ],
          'variables': {
            'jni_gen_package': 'net',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'net_java',
          'type': 'none',
          'variables': {
            'java_in_dir': '../net/android/java',
          },
          'dependencies': [
            '../base/base.gyp:base',
            'cert_verify_status_android_java',
            'certificate_mime_types_java',
            'net_errors_java',
            'private_key_types_java',
            'remote_android_keystore_aidl',
          ],
          'includes': [ '../build/java.gypi' ],
        },
        {
          # Processes the interface files for communication with an Android KeyStore
          # running in a separate process.
          'target_name': 'remote_android_keystore_aidl',
          'type': 'none',
          'variables': {
            'aidl_interface_file': '../net/android/java/src/org/chromium/net/IRemoteAndroidKeyStoreInterface.aidl',
          },
          'sources': [
            '../net/android/java/src/org/chromium/net/IRemoteAndroidKeyStore.aidl',
            '../net/android/java/src/org/chromium/net/IRemoteAndroidKeyStoreCallbacks.aidl',
          ],
          'includes': [ '../build/java_aidl.gypi' ],
        },
        {
          'target_name': 'net_java_test_support',
          'type': 'none',
          'variables': {
            'java_in_dir': '../net/test/android/javatests',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'net_javatests',
          'type': 'none',
          'variables': {
            'java_in_dir': '../net/android/javatests',
          },
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:base_java_test_support',
            'net_java',
          ],
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'net_errors_java',
          'type': 'none',
          'sources': [
            'android/java/NetError.template',
          ],
          'variables': {
            'package_name': 'org/chromium/net',
            'template_deps': ['base/net_error_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'certificate_mime_types_java',
          'type': 'none',
          'sources': [
            'android/java/CertificateMimeType.template',
          ],
          'variables': {
            'package_name': 'org/chromium/net',
            'template_deps': ['base/mime_util_certificate_type_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'cert_verify_status_android_java',
          'type': 'none',
          'sources': [
            'android/java/CertVerifyStatusAndroid.template',
          ],
          'variables': {
            'package_name': 'org/chromium/net',
            'template_deps': ['android/cert_verify_status_android_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'private_key_types_java',
          'type': 'none',
          'sources': [
            'android/java/PrivateKeyType.template',
          ],
          'variables': {
            'package_name': 'org/chromium/net',
            'template_deps': ['android/private_key_type_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
      ],
    }],
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'net_unittests_apk',
          'type': 'none',
          'dependencies': [
            'net_java',
            'net_javatests',
            'net_unittests',
          ],
          'variables': {
            'test_suite_name': 'net_unittests',
          },
          'includes': [ '../build/apk_test.gypi' ],
        },
      ],
    }],
    ['OS == "android" or OS == "linux"', {
      'targets': [
        {
          'target_name': 'disk_cache_memory_test',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            'net',
          ],
          'sources': [
            'tools/disk_cache_memory_test/disk_cache_memory_test.cc',
          ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'net_unittests_run',
          'type': 'none',
          'dependencies': [
            'net_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
            'net_unittests.isolate',
          ],
          'sources': [
            'net_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
