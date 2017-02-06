# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'cronet_jni_headers',
          'type': 'none',
          'sources': [
            'cronet/android/java/src/org/chromium/net/CronetBidirectionalStream.java',
            'cronet/android/java/src/org/chromium/net/CronetLibraryLoader.java',
            'cronet/android/java/src/org/chromium/net/CronetUploadDataStream.java',
            'cronet/android/java/src/org/chromium/net/CronetUrlRequest.java',
            'cronet/android/java/src/org/chromium/net/CronetUrlRequestContext.java',
            'cronet/android/java/src/org/chromium/net/ChromiumUrlRequest.java',
            'cronet/android/java/src/org/chromium/net/ChromiumUrlRequestContext.java',
          ],
          'variables': {
            'jni_gen_package': 'cronet',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'chromium_url_request_java',
          'type': 'none',
          'variables': {
            'source_file': 'cronet/android/chromium_url_request.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'net_request_priority_java',
          'type': 'none',
          'variables': {
            'source_file': '../net/base/request_priority.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'network_quality_observation_source_java',
          'type': 'none',
          'variables': {
            'source_file': '../net/nqe/network_quality_observation_source.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'url_request_error_java',
          'type': 'none',
          'variables': {
            'source_file': 'cronet/android/url_request_error.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          # This target is a jar file containing classes that Cronet's javadocs
          # may reference but are not included in the javadocs themselves.
          'target_name': 'cronet_javadoc_classpath',
          'type': 'none',
          'variables': {
            # Work around GYP requirement that java targets specify java_in_dir
            # variable that contains at least one java file.
            'java_in_dir': 'cronet/android/api',
            'java_in_dir_suffix': '/src_dummy',
            'never_lint': 1,
          },
          'dependencies': [
            'url_request_error_java',
          ],
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'http_cache_type_java',
          'type': 'none',
          'variables': {
            'source_file': 'cronet/url_request_context_config.h',
          },
          'includes': [ '../build/android/java_cpp_enum.gypi' ],
        },
        {
          'target_name': 'load_states_list',
          'type': 'none',
          'sources': [
            'cronet/android/java/src/org/chromium/net/LoadState.template',
          ],
          'variables': {
            'package_name': 'org/chromium/cronet',
            'template_deps': ['../net/base/load_states_list.h'],
          },
          'includes': [ '../build/android/java_cpp_template.gypi' ],
        },
        {
          'target_name': 'cronet_version',
          'type': 'none',
          'variables': {
            'lastchange_path': '<(DEPTH)/build/util/LASTCHANGE',
            'version_py_path': '<(DEPTH)/build/util/version.py',
            'version_path': '<(DEPTH)/chrome/VERSION',
            'template_input_path': 'cronet/android/java/src/org/chromium/net/Version.template',
            'output_path': '<(SHARED_INTERMEDIATE_DIR)/templates/<(_target_name)/org/chromium/cronet/Version.java',
          },
          'direct_dependent_settings': {
            'variables': {
              # Ensure that the output directory is used in the class path
              # when building targets that depend on this one.
              'generated_src_dirs': [
                '<(SHARED_INTERMEDIATE_DIR)/templates/<(_target_name)',
              ],
              # Ensure dependents are rebuilt when the generated Java file changes.
              'additional_input_paths': [
                '<(output_path)',
              ],
            },
          },
          'actions': [
            {
              'action_name': 'cronet_version',
              'inputs': [
                '<(template_input_path)',
                '<(version_path)',
                '<(lastchange_path)',
              ],
              'outputs': [
                '<(output_path)',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-f', '<(version_path)',
                '-f', '<(lastchange_path)',
                '<(template_input_path)',
                '<(output_path)',
              ],
              'message': 'Generating version information',
            },
          ],
        },
        {
          'target_name': 'cronet_version_header',
          'type': 'none',
          # Need to set hard_depency flag because cronet_version generates a
          # header.
          'hard_dependency': 1,
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/',
            ],
          },
          'actions': [
            {
              'action_name': 'version_header',
              'message': 'Generating version header file: <@(_outputs)',
              'inputs': [
                '<(version_path)',
                'cronet/version.h.in',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/components/cronet/version.h',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-e', 'VERSION_FULL="<(version_full)"',
                'cronet/version.h.in',
                '<@(_outputs)',
              ],
              'includes': [
                '../build/util/version.gypi',
              ],
            },
          ],
        },
        {
          # Protobuf compiler / generator for certificate verifcation protocol
          # buffer.
          # GN version: //cronet:cronet_android_cert_proto
          'target_name': 'cronet_android_cert_proto',
          'type': 'static_library',
          'sources': [
            'cronet/android/cert/proto/cert_verification.proto',
          ],
          'variables': {
            'enable_wexit_time_destructors': 1,
            'proto_in_dir': 'cronet/android/cert/proto',
            'proto_out_dir': 'cronet/android/cert/proto',
          },
          'includes': [
            '../build/protoc.gypi',
          ],
        },
        {
          'target_name': 'cronet_static',
          'type': 'static_library',
          'dependencies': [
            '../net/net.gyp:net',
            '../url/url.gyp:url_lib',
          ],
          'conditions': [
            ['enable_data_reduction_proxy_support==1',
              {
                'dependencies': [
                  '../components/components.gyp:data_reduction_proxy_core_browser_small',
                ],
              },
            ],
            ['use_platform_icu_alternatives!=1',
              {
                'dependencies': [
                  '../base/base.gyp:base_i18n',
                ],
              },
            ],
          ],
          'includes': [ 'cronet/cronet_static.gypi' ],
        },
        {
          'target_name': 'libcronet',
          'type': 'shared_library',
          'sources': [
            'cronet/android/cronet_jni.cc',
          ],
          'dependencies': [
            'cronet_static',
            '../base/base.gyp:base',
            '../net/net.gyp:net',
          ],
          'ldflags': [
            '-Wl,--version-script=<!(cd <(DEPTH) && pwd -P)/components/cronet/android/only_jni_exports.lst',
          ],
          'variables': {
            # libcronet doesn't really use native JNI exports, but it does use
            # its own linker version script. The ARM64 linker appears to not
            # work with multiple version scripts with anonymous version tags,
            # so enable use_native_jni_exports which avoids adding another
            # version sript (android_no_jni_exports.lst) so we don't run afoul
            # of this ARM64 linker limitation.
            'use_native_jni_exports': 1,
          },
        },
        { # cronet_api.jar defines Cronet API and provides implementation of
          # legacy api using HttpUrlConnection (not the Chromium stack).
          'target_name': 'cronet_api',
          'type': 'none',
          'dependencies': [
            'http_cache_type_java',
            'url_request_error_java',
            'cronet_version',
            'load_states_list',
            'network_quality_observation_source_java',
            '../third_party/android_tools/android_tools.gyp:android_support_annotations_javalib',
          ],
          'variables': {
            'java_in_dir': 'cronet/android/api',
            'run_findbugs': 1,
          },
          'includes': [ '../build/java.gypi' ],
        },
        { # cronet.jar implements HttpUrlRequest interface using Chromium stack
          # in native libcronet.so library.
          'target_name': 'cronet_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base',
            'cronet_api',
            'chromium_url_request_java',
            'libcronet',
            'net_request_priority_java',
            'network_quality_observation_source_java',
            '../third_party/android_tools/android_tools.gyp:android_support_annotations_javalib',
          ],
          'variables': {
            'java_in_dir': 'cronet/android/java',
            'javac_includes': [
              '**/ChromiumUrlRequest.java',
              '**/ChromiumUrlRequestContext.java',
              '**/ChromiumUrlRequestError.java',
              '**/ChromiumUrlRequestFactory.java',
              '**/ChromiumUrlRequestPriority.java',
              '**/CronetBidirectionalStream.java',
              '**/CronetLibraryLoader.java',
              '**/CronetUploadDataStream.java',
              '**/CronetUrlRequest.java',
              '**/CronetUrlRequestContext.java',
              '**/RequestPriority.java',
              '**/urlconnection/CronetBufferedOutputStream.java',
              '**/urlconnection/CronetChunkedOutputStream.java',
              '**/urlconnection/CronetFixedModeOutputStream.java',
              '**/urlconnection/CronetInputStream.java',
              '**/urlconnection/CronetHttpURLConnection.java',
              '**/urlconnection/CronetHttpURLStreamHandler.java',
              '**/urlconnection/CronetOutputStream.java',
              '**/urlconnection/CronetURLStreamHandlerFactory.java',
              '**/urlconnection/MessageLoop.java',
            ],
            'run_findbugs': 1,
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'cronet_sample_apk',
          'type': 'none',
          'dependencies': [
            'cronet_java',
            'cronet_api',
          ],
          'variables': {
            'apk_name': 'CronetSample',
            'java_in_dir': 'cronet/android/sample',
            'resource_dir': 'cronet/android/sample/res',
            'native_lib_target': 'libcronet',
            'proguard_enabled': 'true',
            'proguard_flags_paths': [
              'cronet/android/proguard.cfg',
              'cronet/android/sample/javatests/proguard.cfg',
            ],
            'run_findbugs': 1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
        {
          # cronet_sample_apk creates a .jar as a side effect. Any java targets
          # that need that .jar in their classpath should depend on this target,
          # cronet_sample_apk_java. Dependents of cronet_sample_apk receive its
          # jar path in the variable 'apk_output_jar_path'. This target should
          # only be used by targets which instrument cronet_sample_apk.
          'target_name': 'cronet_sample_apk_java',
          'type': 'none',
          'dependencies': [
            'cronet_sample_apk',
          ],
          'includes': [ '../build/apk_fake_jar.gypi' ],
        },
        {
          'target_name': 'cronet_sample_test_apk',
          'type': 'none',
          'dependencies': [
            'cronet_java',
            'cronet_sample_apk_java',
            'cronet_api',
            '../base/base.gyp:base_java_test_support',
            '../net/net.gyp:net_java_test_support',
            '../net/net.gyp:require_net_test_support_apk',
          ],
          'variables': {
            'apk_name': 'CronetSampleTest',
            'java_in_dir': 'cronet/android/sample/javatests',
            'is_test_apk': 1,
            'run_findbugs': 1,
            'test_type': 'instrumentation',
            'additional_apks': [
              '<(PRODUCT_DIR)/apks/ChromiumNetTestSupport.apk',
            ],
          },
          'includes': [
            '../build/java_apk.gypi',
            '../build/android/test_runner.gypi',
          ],
        },
        {
          'target_name': 'cronet_tests_jni_headers',
          'type': 'none',
          'sources': [
            'cronet/android/test/src/org/chromium/net/CronetTestUtil.java',
            'cronet/android/test/src/org/chromium/net/MockUrlRequestJobFactory.java',
            'cronet/android/test/src/org/chromium/net/MockCertVerifier.java',
            'cronet/android/test/src/org/chromium/net/NativeTestServer.java',
            'cronet/android/test/src/org/chromium/net/NetworkChangeNotifierUtil.java',
            'cronet/android/test/src/org/chromium/net/QuicTestServer.java',
            'cronet/android/test/src/org/chromium/net/SdchObserver.java',
            'cronet/android/test/src/org/chromium/net/TestUploadDataStreamHandler.java',
            'cronet/android/test/javatests/src/org/chromium/net/CronetUrlRequestContextTest.java',
          ],
          'variables': {
            'jni_gen_package': 'cronet_tests',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'libcronet_tests',
          'type': 'shared_library',
          'sources': [
            'cronet/android/test/cronet_test_jni.cc',
            'cronet/android/test/mock_cert_verifier.cc',
            'cronet/android/test/mock_cert_verifier.h',
            'cronet/android/test/mock_url_request_job_factory.cc',
            'cronet/android/test/mock_url_request_job_factory.h',
            'cronet/android/test/native_test_server.cc',
            'cronet/android/test/native_test_server.h',
            'cronet/android/test/quic_test_server.cc',
            'cronet/android/test/quic_test_server.h',
            'cronet/android/test/sdch_test_util.cc',
            'cronet/android/test/sdch_test_util.h',
            'cronet/android/test/test_upload_data_stream_handler.cc',
            'cronet/android/test/test_upload_data_stream_handler.h',
            'cronet/android/test/network_change_notifier_util.cc',
            'cronet/android/test/network_change_notifier_util.h',
            'cronet/android/test/cronet_url_request_context_config_test.cc',
            'cronet/android/test/cronet_url_request_context_config_test.h',
            'cronet/android/test/cronet_test_util.cc',
            'cronet/android/test/cronet_test_util.h',
          ],
          'dependencies': [
            'cronet_tests_jni_headers',
            '../base/base.gyp:base',
            '../net/net.gyp:net',
            '../net/net.gyp:net_quic_proto',
            '../net/net.gyp:net_test_support',
            '../net/net.gyp:simple_quic_tools',
            '../base/base.gyp:base_i18n',
            '../third_party/icu/icu.gyp:icui18n',
            '../third_party/icu/icu.gyp:icuuc',
          ],
          'ldflags': [
            '-Wl,--version-script=<!(cd <(DEPTH) && pwd -P)/components/cronet/android/only_jni_exports.lst',
          ],
          'variables': {
            # libcronet doesn't really use native JNI exports, but it does use
            # its own linker version script. The ARM64 linker appears to not
            # work with multiple version scripts with anonymous version tags,
            # so enable use_native_jni_exports which avoids adding another
            # version sript (android_no_jni_exports.lst) so we don't run afoul
            # of this ARM64 linker limitation.
            'use_native_jni_exports': 1,
          },
          'conditions': [
            ['enable_data_reduction_proxy_support==1',
              {
                'dependencies': [
                  '../components/components.gyp:data_reduction_proxy_core_browser_small',
                ],
              },
            ],
          ],
          'includes': [ 'cronet/cronet_static.gypi' ],
        },
        {
          'target_name': 'cronet_test_support',
          'type': 'none',
          'dependencies': [
            'cronet_java',
            '../net/net.gyp:net_java_test_support',
            '../third_party/netty-tcnative/netty-tcnative.gyp:netty-tcnative',
            '../third_party/netty4/netty.gyp:netty_all',
          ],
          'variables': {
            'java_in_dir': 'cronet/android/test',
            'additional_src_dirs': [ 'cronet/android/test/javatests/src' ],
            'run_findbugs': 1,
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'cronet_test_apk',
          'type': 'none',
          'dependencies': [
            'cronet_java',
            'cronet_test_support',
            '../net/net.gyp:net_java_test_support',
            '../third_party/netty-tcnative/netty-tcnative.gyp:netty-tcnative',
            '../third_party/netty4/netty.gyp:netty_all',
          ],
          'variables': {
            'apk_name': 'CronetTest',
            # There isn't an easy way to have a java_apk target without any Java
            # so we'll borrow the trick from the net_test_support_apk target of
            # pointing it at placeholder Java via java_in_dir_suffix.
            'java_in_dir': 'cronet/android/test',
            'java_in_dir_suffix': '/src_dummy',
            'resource_dir': 'cronet/android/test/res',
            'asset_location': 'cronet/android/test/assets',
            'native_lib_target': 'libcronet_tests',
            'never_lint': 1,
            'additional_bundled_libs': [
              '>(netty_tcnative_so_file_location)',
            ],
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
        {
          # cronet_test_apk creates a .jar as a side effect. Any java targets
          # that need that .jar in their classpath should depend on this target,
          # cronet_test_apk_java. Dependents of cronet_test_apk receive its
          # jar path in the variable 'apk_output_jar_path'. This target should
          # only be used by targets which instrument cronet_test_apk.
          'target_name': 'cronet_test_apk_java',
          'type': 'none',
          'dependencies': [
            'cronet_test_apk',
          ],
          'includes': [ '../build/apk_fake_jar.gypi' ],
        },
        {
          'target_name': 'cronet_test_instrumentation_apk',
          'type': 'none',
          'dependencies': [
            'cronet_test_apk_java',
            '../base/base.gyp:base_java_test_support',
            '../net/net.gyp:net_java_test_support',
            '../net/net.gyp:require_net_test_support_apk',
          ],
          'variables': {
            'apk_name': 'CronetTestInstrumentation',
            'java_in_dir': 'cronet/android/test/javatests',
            'resource_dir': 'cronet/android/test/res',
            'is_test_apk': 1,
            'run_findbugs': 1,
            'test_type': 'instrumentation',
            'isolate_file': 'cronet/android/cronet_test_instrumentation_apk.isolate',
            'additional_apks': [
              '<(PRODUCT_DIR)/apks/ChromiumNetTestSupport.apk',
            ],
          },
          'includes': [
            '../build/java_apk.gypi',
            '../build/android/test_runner.gypi',
          ],
        },
        {
          'target_name': 'cronet_perf_test_apk',
          'type': 'none',
          'dependencies': [
            'cronet_java',
            'cronet_api',
            'cronet_test_support',
          ],
          'variables': {
            'apk_name': 'CronetPerfTest',
            'java_in_dir': 'cronet/android/test/javaperftests',
            'native_lib_target': 'libcronet_tests',
            'proguard_enabled': 'true',
            'proguard_flags_paths': [
              'cronet/android/proguard.cfg',
              'cronet/android/test/javaperftests/proguard.cfg',
            ],
            'run_findbugs': 1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
        {
          'target_name': 'cronet_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'cronet_android_cert_proto',
            'cronet_static',
            'metrics',
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../net/net.gyp:net_test_support',
            '../testing/gtest.gyp:gtest',
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
          'sources': [
            'cronet/android/cert/cert_verifier_cache_serializer_unittest.cc',
            'cronet/run_all_unittests.cc',
            'cronet/url_request_context_config_unittest.cc',
            'cronet/histogram_manager_unittest.cc',
          ],
        },
        {
          'target_name': 'cronet_unittests_apk',
          'type': 'none',
          'dependencies': [
            'cronet_unittests',
          ],
          'variables': {
            'test_suite_name': 'cronet_unittests',
            'shard_timeout': 180,
          },
          'includes': [
            '../build/apk_test.gypi',
          ],
        },
        {
          'target_name': 'cronet_package',
          'type': 'none',
          'dependencies': [
            'libcronet',
            'cronet_java',
            'cronet_api',
            'cronet_javadoc_classpath',
            '../net/net.gyp:net_unittests_apk',
          ],
          'variables': {
            'native_lib': 'libcronet.>(android_product_extension)',
            'java_lib': 'cronet.jar',
            'java_api_lib': 'cronet_api.jar',
            'java_api_src_lib': 'cronet_api-src.jar',
            'java_src_lib': 'cronet-src.jar',
            'java_sample_src_lib': 'cronet-sample-src.jar',
            'lib_java_dir': '<(PRODUCT_DIR)/lib.java',
            'package_dir': '<(PRODUCT_DIR)/cronet',
            'intermediate_dir': '<(SHARED_INTERMEDIATE_DIR)/cronet',
            'jar_extract_dir': '<(intermediate_dir)/cronet_jar_extract',
            'jar_extract_stamp': '<(intermediate_dir)/jar_extract.stamp',
            'cronet_jar_stamp': '<(intermediate_dir)/cronet_jar.stamp',
          },
          'actions': [
            {
              'action_name': 'strip libcronet',
              'inputs': ['<(SHARED_LIB_DIR)/<(native_lib)'],
              'outputs': ['<(package_dir)/libs/<(android_app_abi)/<(native_lib)'],
              'action': [
                '<(android_strip)',
                '--strip-unneeded',
                '<@(_inputs)',
                '-o',
                '<@(_outputs)',
              ],
            },
            {
              'action_name': 'extracting from jars',
              'inputs':  [
                '<(lib_java_dir)/cronet_java.jar',
                '<(lib_java_dir)/base_java.jar',
                '<(lib_java_dir)/net_java.jar',
                '<(lib_java_dir)/url_java.jar',
              ],
              'outputs': ['<(jar_extract_stamp)', '<(jar_extract_dir)'],
              'action': [
                'python',
                'cronet/tools/extract_from_jars.py',
                '--classes-dir=<(jar_extract_dir)',
                '--jars=<@(_inputs)',
                '--stamp=<(jar_extract_stamp)',
              ],
            },
            {
              'action_name': 'jar_<(_target_name)',
              'message': 'Creating <(_target_name) jar',
              'inputs': [
                '<(DEPTH)/build/android/gyp/util/build_utils.py',
                '<(DEPTH)/build/android/gyp/util/md5_check.py',
                '<(DEPTH)/build/android/gyp/jar.py',
                '<(jar_extract_stamp)',
              ],
              'outputs': [
                '<(package_dir)/<(java_lib)',
                '<(cronet_jar_stamp)',
              ],
              'action': [
                'python', '<(DEPTH)/build/android/gyp/jar.py',
                '--classes-dir=<(jar_extract_dir)',
                '--jar-path=<(package_dir)/<(java_lib)',
                '--stamp=<(cronet_jar_stamp)',
              ]
            },
            {
              'action_name': 'jar_api_src_<(_target_name)',
              'inputs': ['cronet/tools/jar_src.py'] ,
              'outputs': ['<(package_dir)/<(java_api_src_lib)'],
              'action': [
                'python',
                '<@(_inputs)',
                '--src-dir=cronet/android/api/src',
                '--jar-path=<(package_dir)/<(java_api_src_lib)',
              ],
            },
            {
              'action_name': 'jar_src_<(_target_name)',
              'inputs': ['cronet/tools/jar_src.py'] ,
              'outputs': ['<(package_dir)/<(java_src_lib)'],
              'action': [
                'python',
                '<@(_inputs)',
                '--src-dir=../base/android/java/src',
                '--src-dir=../net/android/java/src',
                '--src-dir=../url/android/java/src',
                '--src-dir=cronet/android/java/src',
                '--jar-path=<(package_dir)/<(java_src_lib)',
              ],
            },
            {
              'action_name': 'jar_sample_src_<(_target_name)',
              'inputs': ['cronet/tools/jar_src.py'] ,
              'outputs': ['<(package_dir)/<(java_sample_src_lib)'],
              'action': [
                'python',
                '<@(_inputs)',
                '--src-dir=cronet/android/sample',
                '--jar-path=<(package_dir)/<(java_sample_src_lib)',
              ],
            },
            {
              'action_name': 'generate licenses',
              'inputs': ['cronet/tools/cronet_licenses.py'] ,
              'outputs': ['<(package_dir)/LICENSE'],
              'action': [
                'python',
                '<@(_inputs)',
                'license',
                '<@(_outputs)',
              ],
            },
            {
              'action_name': 'generate javadoc',
              'inputs': ['cronet/tools/generate_javadoc.py'] ,
              'outputs': ['<(package_dir)/javadoc'],
              'action': [
                'python',
                '<@(_inputs)',
                '--output-dir=<(package_dir)',
                '--input-dir=cronet/',
                '--overview-file=<(package_dir)/README.md.html',
                '--readme-file=cronet/README.md',
                '--lib-java-dir=<(lib_java_dir)',
              ],
              'message': 'Generating Javadoc',
            },
          ],
          'copies': [
            {
              'destination': '<(package_dir)',
              'files': [
                '../AUTHORS',
                '../chrome/VERSION',
                'cronet/android/proguard.cfg',
                '<(lib_java_dir)/<(java_api_lib)'
              ],
            },
            {
              'destination': '<(package_dir)/symbols/<(android_app_abi)',
              'files': [
                '<(SHARED_LIB_DIR)/<(native_lib)',
              ],
            },
          ],
        },
      ],
      'variables': {
        'enable_data_reduction_proxy_support%': 0,
      },
    }],  # OS=="android"
    ['OS=="ios"', {
      'targets': [
        { # TODO(mef): Dedup this with copy in OS=="android" section.
          'target_name': 'cronet_version_header',
          'type': 'none',
          # Need to set hard_depency flag because cronet_version generates a
          # header.
          'hard_dependency': 1,
          'direct_dependent_settings': {
            'include_dirs': [
              '<(SHARED_INTERMEDIATE_DIR)/',
            ],
          },
          'actions': [
            {
              'action_name': 'version_header',
              'message': 'Generating version header file: <@(_outputs)',
              'inputs': [
                '<(version_path)',
                'cronet/version.h.in',
              ],
              'outputs': [
                '<(SHARED_INTERMEDIATE_DIR)/components/cronet/ios/version.h',
              ],
              'action': [
                'python',
                '<(version_py_path)',
                '-e', 'VERSION_FULL="<(version_full)"',
                'cronet/version.h.in',
                '<@(_outputs)',
              ],
              'includes': [
                '../build/util/version.gypi',
              ],
            },
          ],
        },
        {
          'target_name': 'cronet_static',
          'type': 'static_library',
          'sources': [
            'cronet/ios/Cronet.h',
            'cronet/ios/Cronet.mm',
            'cronet/ios/cronet_bidirectional_stream.h',
            'cronet/ios/cronet_bidirectional_stream.cc',
            'cronet/ios/cronet_c_for_grpc.h',
            'cronet/ios/cronet_c_for_grpc.cc',
            'cronet/ios/cronet_environment.cc',
            'cronet/ios/cronet_environment.h',
            'cronet/url_request_context_config.cc',
            'cronet/url_request_context_config.h',
          ],
          'dependencies': [
            'cronet_version_header',
            '../base/base.gyp:base',
            '../net/net.gyp:net',
          ],
          'cflags': [
            '-fdata-sections',
            '-ffunction-sections',
            '-fno-rtti',
            '-fvisibility=hidden'
            '-fvisibility-inlines-hidden',
            '-Wno-sign-promo',
            '-Wno-missing-field-initializers',
          ],
          'ldflags': [
            '-llog',
            '-Wl,--gc-sections',
            '-Wl,--exclude-libs,ALL'
          ],
        },
        {
          'target_name': 'libcronet_shared',
          'type': 'shared_library',
          'sources': [
            'cronet/ios/Cronet.h',
            'cronet/ios/Cronet.mm',
          ],
          'dependencies': [
            'cronet_static',
            '../base/base.gyp:base',
          ],
        },
        {
          'target_name': 'cronet_framework',
          'product_name': 'Cronet',
          'type': 'shared_library',
          'mac_bundle': 1,
          'sources': [
            'cronet/ios/Cronet.h',
            'cronet/ios/cronet_c_for_grpc.h',
            'cronet/ios/empty.cc',
          ],
          'mac_framework_headers': [
            'cronet/ios/Cronet.h',
            'cronet/ios/cronet_c_for_grpc.h',
          ],
          'link_settings': {
            'libraries': [
              'Foundation.framework',
            ],
          },
          'xcode_settings': {
            'DEBUGGING_SYMBOLS': 'YES',
            'INFOPLIST_FILE': 'cronet/ios/Info.plist',
            'LD_DYLIB_INSTALL_NAME': '@loader_path/Frameworks/Cronet.framework/Cronet',
          },
          'dependencies': [
            'cronet_static',
            '../base/base.gyp:base',
          ],
          'configurations': {
            'Debug_Base': {
              'xcode_settings': {
                'DEPLOYMENT_POSTPROCESSING': 'NO',
                'DEBUG_INFORMATION_FORMAT': 'dwarf',
                'STRIP_INSTALLED_PRODUCT': 'NO',
              }
            },
            'Release_Base': {
              'xcode_settings': {
                'DEPLOYMENT_POSTPROCESSING': 'YES',
                'DEBUG_INFORMATION_FORMAT': 'dwarf-with-dsym',
                'STRIP_INSTALLED_PRODUCT': 'YES',
                'STRIP_STYLE': 'non-global',
              }
            },
          },
        },
        {
          'target_name': 'cronet_test',
          'type': 'executable',
          'dependencies': [
            'cronet_static',
            '../net/net.gyp:net_quic_proto',
            '../net/net.gyp:net_test_support',
            '../net/net.gyp:simple_quic_tools',
            '../testing/gtest.gyp:gtest',
          ],
          'sources': [
            'cronet/ios/test/cronet_bidirectional_stream_test.mm',
            'cronet/ios/test/cronet_test_runner.mm',
            'cronet/ios/test/quic_test_server.cc',
            'cronet/ios/test/quic_test_server.h',
          ],
          'mac_bundle_resources': [
            '../net/data/ssl/certificates/quic_test.example.com.crt',
            '../net/data/ssl/certificates/quic_test.example.com.key',
            '../net/data/ssl/certificates/quic_test.example.com.key.pkcs8',
            '../net/data/ssl/certificates/quic_test.example.com.key.sct',
          ],
          'include_dirs': [
            '..',
          ],
        },
        {
            # Build this target to package a standalone Cronet in a single
            # .a file.
            'target_name': 'cronet_package',
            'type': 'none',
            'variables' : {
              'package_dir': '<(PRODUCT_DIR)/cronet',
            },
            'dependencies': [
              # Depend on the dummy target so that all of CrNet's dependencies
              # are built before packaging.
              'libcronet_shared',
            ],
            'actions': [
              {
                'action_name': 'Package Cronet',
                'variables': {
                  'tool_path':
                      'cronet/tools/link_dependencies.py',
                },
                # Actions need an inputs list, even if it's empty.
                'inputs': [
                  '<(tool_path)',
                  '<(PRODUCT_DIR)/libcronet_shared.dylib',
                ],
                # Only specify one output, since this will be libtool's output.
                'outputs': [ '<(package_dir)/libcronet_standalone_with_symbols.a' ],
                'action': ['<(tool_path)',
                           '<(PRODUCT_DIR)',
                           'libcronet_shared.dylib',
                           '<@(_outputs)',
                ],
              },
              {
                'action_name': 'Stripping standalone library',
                # Actions need an inputs list, even if it's empty.
                'inputs': [
                  '<(package_dir)/libcronet_standalone_with_symbols.a',
                ],
                # Only specify one output, since this will be libtool's output.
                'outputs': [ '<(package_dir)/libcronet_standalone.a' ],
                'action': ['strip',
                           '-S',
                           '<@(_inputs)',
                           '-o',
                           '<@(_outputs)',
                ],
              },
            ],
            'copies': [
              {
                'destination': '<(package_dir)',
                'files': [
                  '../chrome/VERSION',
                  'cronet/ios/Cronet.h',
                  'cronet/ios/cronet_c_for_grpc.h',
                ],
              },
              {
                'destination': '<(package_dir)/test',
                'files': [
                  'cronet/ios/test/cronet_bidirectional_stream_test.mm',
                  'cronet/ios/test/cronet_test_runner.mm',
                ],
              },
            ],
          },
      ],
    }],  # OS=="ios"
  ],
}
