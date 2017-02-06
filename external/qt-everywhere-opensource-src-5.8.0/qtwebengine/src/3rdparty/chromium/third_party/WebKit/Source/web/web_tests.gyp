#
# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#         * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#         * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#         * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

{
    'includes': [
        '../bindings/bindings.gypi',
        '../build/features.gypi',
        '../build/scripts/scripts.gypi',
        '../core/core.gypi',
        '../modules/modules.gypi',
        '../platform/blink_platform.gypi',
        '../web/web.gypi',
        '../wtf/wtf.gypi',
    ],
    'targets': [
        {
            # GN version: //third_party/WebKit/Source/web:webkit_unit_tests
            'target_name': 'webkit_unit_tests',
            'type': 'executable',
            'variables': { 'enable_wexit_time_destructors': 1, },
            'dependencies': [
                '../../public/blink.gyp:blink',
                '../../public/blink.gyp:mojo_bindings',
                '../config.gyp:unittest_config',
                '../modules/modules.gyp:modules',
                '../platform/blink_platform_tests.gyp:blink_platform_test_support',
                '../wtf/wtf.gyp:wtf',
                'web.gyp:blink_web_test_support',
                '<(DEPTH)/base/base.gyp:base',
                '<(DEPTH)/base/base.gyp:base_i18n',
                '<(DEPTH)/base/base.gyp:test_support_base',
                '<(DEPTH)/gpu/gpu.gyp:gpu_unittest_utils',
                '<(DEPTH)/testing/gmock.gyp:gmock',
                '<(DEPTH)/testing/gtest.gyp:gtest',
                '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
                '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
                '<(DEPTH)/url/url.gyp:url_lib',
                '<(DEPTH)/v8/src/v8.gyp:v8',
                '<(DEPTH)/content/content_shell_and_tests.gyp:test_support_content',
            ],
            'sources': [
                '../web/tests/RunAllTests.cpp',
                '<@(bindings_unittest_files)',
                '<@(core_unittest_files)',
                '<@(modules_unittest_files)',
                '<@(platform_web_unittest_files)',
                '<@(web_unittest_files)',
            ],
            'include_dirs': [
                '../../public/web',
                '../web',
                'src',
            ],
            'defines': [
                'BLINK_IMPLEMENTATION=1',
                'INSIDE_BLINK',
            ],
            'conditions': [
                ['component!="shared_library"', {
                    'dependencies': [
                        '../core/core.gyp:webcore',
                    ],
                }, {
                    'dependencies': [
                        '../core/core.gyp:webcore_shared',
                    ],
                }],
                ['OS=="win" and component!="shared_library"', {
                    'configurations': {
                        'Debug_Base': {
                            'msvs_settings': {
                                'VCLinkerTool': {
                                    'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                                },
                            },
                        },
                    },
                }],
                ['OS=="android"', {
                    'type': 'shared_library',
                    'dependencies': [
                        '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
                        '<(DEPTH)/tools/android/forwarder2/forwarder.gyp:forwarder2',
                    ],
                }],
                ['OS=="mac"', {
                    'include_dirs': [
                        '../../public/web/mac',
                    ],
                }],
            ],
        }
    ], # targets
    'conditions': [
        ['OS=="android" and gtest_target_type == "shared_library"', {
            # Wrap libwebkit_unit_tests.so into an android apk for execution.
            'targets': [{
                'target_name': 'webkit_unit_tests_apk',
                'type': 'none',
                'dependencies': [
                    '<(DEPTH)/base/base.gyp:base_java',
                    '<(DEPTH)/content/content.gyp:content_shell_assets_copy',
                    '<(DEPTH)/net/net.gyp:net_java',
                    'webkit_unit_tests',
                ],
                'variables': {
                    'test_suite_name': 'webkit_unit_tests',
                    'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)webkit_unit_tests<(SHARED_LIB_SUFFIX)',
                    'additional_input_paths': ['<(asset_location)/content_shell.pak'],
                    'asset_location': '<(PRODUCT_DIR)/content_shell/assets',
                    'conditions': [
                      ['v8_use_external_startup_data==1', {
                        'additional_input_paths': [
                          '<(asset_location)/natives_blob_<(arch_suffix).bin',
                          '<(asset_location)/snapshot_blob_<(arch_suffix).bin',
                        ],
                      }],
                    ],
                },
                'includes': [
                  '../../../../build/apk_test.gypi',
                  '../../../../build/android/v8_external_startup_data_arch_suffix.gypi',
                ],
            }],
        }],
        ['test_isolation_mode != "noop"', {
            'targets': [
                {
                    'target_name': 'webkit_unit_tests_run',
                    'type': 'none',
                    'dependencies': [
                        'webkit_unit_tests',
                    ],
                    'includes': [
                        '../../../../build/isolate.gypi',
                    ],
                    'sources': [
                        'webkit_unit_tests.isolate',
                    ],
                },
            ],
        }],
    ],
}

