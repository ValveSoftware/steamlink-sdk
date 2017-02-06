#
# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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
    '../build/win/precompile.gypi',
    'blink_platform.gypi',
    'heap/blink_heap.gypi',
  ],
  'targets': [
    {
      'target_name': 'blink_heap_unittests',
      'type': 'executable',
      'dependencies': [
        '../config.gyp:unittest_config',
        '../wtf/wtf.gyp:wtf',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/content/content_shell_and_tests.gyp:test_support_content',
        'blink_platform.gyp:blink_platform',
        'blink_platform_test_support',
      ],
      'defines': [
        'INSIDE_BLINK',
      ],
      'sources': [
        'heap/RunAllTests.cpp',
        '<@(platform_heap_test_files)',
      ],
      'conditions': [
        ['OS=="android"', {
          'type': 'shared_library',
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
            '<(DEPTH)/tools/android/forwarder2/forwarder.gyp:forwarder2',
          ],
        }],
      ],
    },
    {
      'target_name': 'blink_platform_unittests',
      'type': 'executable',
      'dependencies': [
        'blink_platform_test_support',
        '../config.gyp:unittest_config',
        '../wtf/wtf.gyp:wtf',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/cc/cc.gyp:cc',
        '<(DEPTH)/cc/cc_tests.gyp:cc_test_support',
        '<(DEPTH)/cc/blink/cc_blink.gyp:cc_blink',
        '<(DEPTH)/mojo/mojo_edk.gyp:mojo_common_test_support',
        '<(DEPTH)/mojo/mojo_edk_tests.gyp:mojo_public_bindings_for_blink_tests',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/third_party/harfbuzz-ng/harfbuzz.gyp:harfbuzz-ng',
        '<(DEPTH)/third_party/qcms/qcms.gyp:qcms',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
        '<(DEPTH)/url/url.gyp:url_lib',
        '<(DEPTH)/url/url.gyp:url_interfaces_mojom_for_blink',
        '<(DEPTH)/url/url.gyp:url_test_interfaces_mojom_for_blink',
        'blink_platform.gyp:blink_platform',
      ],
      'defines': [
        'INSIDE_BLINK',
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)/blink',
      ],
      'sources': [
        'testing/RunAllTests.cpp',
        '<@(platform_test_files)',
      ],
      'conditions': [
        ['OS=="android" and gtest_target_type == "shared_library"', {
          'type': 'shared_library',
          'dependencies': [
            '<(DEPTH)/testing/android/native_test.gyp:native_test_native_code',
            '<(DEPTH)/tools/android/forwarder2/forwarder.gyp:forwarder2',
          ],
        }],
      ],
    },
    {
      'target_name': 'image_decode_bench',
      'type': 'executable',
      'dependencies': [
        '../config.gyp:config',
        '../wtf/wtf.gyp:wtf',
        'blink_platform.gyp:blink_platform',
      ],
      'defines': [
        'INSIDE_BLINK',
      ],
      'sources': [
        'testing/ImageDecodeBench.cpp',
      ],
    },
    {
      'target_name': 'blink_platform_test_support',
      'type': 'static_library',
      'dependencies': [
        '../config.gyp:config',
        '../wtf/wtf.gyp:wtf',
        'blink_platform.gyp:blink_common',
        'blink_platform.gyp:blink_platform',
        '<(DEPTH)/cc/cc.gyp:cc',
        '<(DEPTH)/cc/cc_tests.gyp:cc_test_support',
        '<(DEPTH)/cc/blink/cc_blink.gyp:cc_blink',
        '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
        '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
        '<(DEPTH)/testing/gmock.gyp:gmock',
      ],
      'defines': [
        'INSIDE_BLINK',
      ],
      'include_dirs': [
        '<(SHARED_INTERMEDIATE_DIR)/blink',
      ],
      'sources': [
        '<@(platform_test_support_files)',
      ],
      # Disable c4267 warnings until we fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267 ],
    },
  ],
  'conditions': [
    ['OS=="android" and gtest_target_type == "shared_library"', {
      'targets': [{
        'target_name': 'blink_heap_unittests_apk',
        'type': 'none',
        'dependencies': [
          '<(DEPTH)/base/base.gyp:base_java',
          '<(DEPTH)/net/net.gyp:net_java',
          'blink_heap_unittests',
        ],
        'conditions': [
          ['v8_use_external_startup_data==1', {
            'dependencies': [
              '<(DEPTH)/v8/src/v8.gyp:v8_external_snapshot',
            ],
            'variables': {
              'dest_path': '<(asset_location)',
              'renaming_sources': [
                '<(PRODUCT_DIR)/natives_blob.bin',
                '<(PRODUCT_DIR)/snapshot_blob.bin',
              ],
              'renaming_destinations': [
                'natives_blob_<(arch_suffix).bin',
                'snapshot_blob_<(arch_suffix).bin',
              ],
              'clear': 1,
            },
            'includes': ['../../../../build/android/copy_ex.gypi'],
          }],
        ],
        'variables': {
          'test_suite_name': 'blink_heap_unittests',
          'conditions': [
            ['v8_use_external_startup_data==1', {
              'asset_location': '<(PRODUCT_DIR)/blink_heap_unittests_apk/assets',
              'additional_input_paths': [
                '<(PRODUCT_DIR)/blink_heap_unittests_apk/assets/natives_blob_<(arch_suffix).bin',
                '<(PRODUCT_DIR)/blink_heap_unittests_apk/assets/snapshot_blob_<(arch_suffix).bin',
              ],
            }],
          ],
        },
        'includes': [
          '../../../../build/apk_test.gypi',
          '../../../../build/android/v8_external_startup_data_arch_suffix.gypi',
        ],
      },
      {
        'target_name': 'blink_platform_unittests_apk',
        'type': 'none',
        'dependencies': [
          '<(DEPTH)/base/base.gyp:base_java',
          '<(DEPTH)/net/net.gyp:net_java',
          'blink_platform_unittests',
        ],
        'variables': {
          'test_suite_name': 'blink_platform_unittests',
        },
        'includes': [ '../../../../build/apk_test.gypi' ],
      }],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'blink_heap_unittests_run',
          'type': 'none',
          'dependencies': [
            'blink_heap_unittests',
          ],
          'includes': [
            '../../../../build/isolate.gypi',
          ],
          'sources': [
            'blink_heap_unittests.isolate',
          ],
        },
        {
          'target_name': 'blink_platform_unittests_run',
          'type': 'none',
          'dependencies': [
            'blink_platform_unittests',
          ],
          'includes': [
            '../../../../build/isolate.gypi',
          ],
          'sources': [
            'blink_platform_unittests.isolate',
          ],
        }
      ],
    }],
  ],
}
