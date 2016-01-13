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
        '../core/core.gypi',
        '../build/features.gypi',
        '../build/scripts/scripts.gypi',
        '../build/win/precompile.gypi',
        '../modules/modules.gypi',
        '../platform/blink_platform.gypi',
        '../wtf/wtf.gypi',
        'web.gypi',
    ],
    'targets': [
        {
            'target_name': 'blink_web',
            'type': '<(component)',
            'variables': { 'enable_wexit_time_destructors': 1, },
            'dependencies': [
                '../config.gyp:config',
                '../platform/blink_platform.gyp:blink_common',
                '../core/core.gyp:webcore',
                '../modules/modules.gyp:modules',
                '<(DEPTH)/skia/skia.gyp:skia',
                '<(angle_path)/src/build_angle.gyp:translator',
                '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
                '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
                '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
            ],
            'export_dependent_settings': [
                '<(DEPTH)/skia/skia.gyp:skia',
                '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
                '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
                '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
            ],
            'include_dirs': [
                '<(angle_path)/include',
                '<(DEPTH)/third_party/skia/include/utils',
            ],
            'defines': [
                'BLINK_IMPLEMENTATION=1',
                'INSIDE_BLINK',
            ],
            'sources': [
                '<@(web_files)',
            ],
            'conditions': [
                ['component=="shared_library"', {
                    'dependencies': [
                        '../core/core.gyp:webcore_generated',
                        '../core/core.gyp:webcore_testing',
                        '../modules/modules.gyp:modules_testing',
                        '../wtf/wtf_tests.gyp:wtf_unittest_helpers',
                        '<(DEPTH)/base/base.gyp:test_support_base',
                        '<(DEPTH)/testing/gmock.gyp:gmock',
                        '<(DEPTH)/testing/gtest.gyp:gtest',
                        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
                        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
                        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
                        '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
                        '<(DEPTH)/third_party/libxml/libxml.gyp:libxml',
                        '<(DEPTH)/third_party/libxslt/libxslt.gyp:libxslt',
                        '<(DEPTH)/third_party/modp_b64/modp_b64.gyp:modp_b64',
                        '<(DEPTH)/third_party/ots/ots.gyp:ots',
                        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
                        '<(DEPTH)/url/url.gyp:url_lib',
                        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                        '<(libjpeg_gyp_path):libjpeg',
                        # We must not add webkit_support here because of cyclic dependency.
                    ],
                    'export_dependent_settings': [
                        '<(DEPTH)/url/url.gyp:url_lib',
                        '<(DEPTH)/v8/tools/gyp/v8.gyp:v8',
                    ],
                    'sources': [
                        # Compile Blink unittest files into blink_web.dll in component build mode
                        # since there're methods that are tested but not exported.
                        # WebUnitTests.* exports an API that runs all the unittests inside
                        # blink_web.dll.
                        '<@(bindings_unittest_files)',
                        '<@(core_unittest_files)',
                        '<@(modules_unittest_files)',
                        # FIXME: the next line should not be needed. We prefer to run these unit tests outside blink_web.dll.
                        '<@(platform_web_unittest_files)',
                        '<@(web_unittest_files)',
                        'WebTestingSupport.cpp',
                        'tests/WebUnitTests.cpp',   # Components test runner support.
                    ],
                    'conditions': [
                        ['OS=="win" or OS=="mac"', {
                            'dependencies': [
                                '<(DEPTH)/third_party/nss/nss.gyp:*',
                            ],
                        }],
                        ['clang==1', {
                            # FIXME: It would be nice to enable this in shared builds too,
                            # but the test files have global constructors from the GTEST macro
                            # and we pull in the test files into the blink_web target in the
                            # shared build.
                            'cflags!': ['-Wglobal-constructors'],
                            'xcode_settings': {
                              'WARNING_CFLAGS!': ['-Wglobal-constructors'],
                            },
                        }],
                    ],
                    'msvs_settings': {
                      'VCLinkerTool': {
                        'conditions': [
                          ['incremental_chrome_dll==1', {
                            'UseLibraryDependencyInputs': "true",
                          }],
                        ],
                      },
                    },
                }],
                ['OS == "linux"', {
                    'dependencies': [
                        '<(DEPTH)/build/linux/system.gyp:fontconfig',
                    ],
                }, {
                    'sources/': [
                        ['exclude', 'linux/'],
                    ],
                }],
                ['use_x11 == 1', {
                    'dependencies': [
                        '<(DEPTH)/build/linux/system.gyp:x11',
                    ],
                }, {
                    'sources/': [
                        ['exclude', 'x11/'],
                    ]
                }],
                ['OS!="android"', {
                    'sources/': [
                        ['exclude', 'WebInputEventFactoryAndroid.cpp$'],
                    ],
                }],
                ['OS=="mac"', {
                    'link_settings': {
                        'libraries': [
                            '$(SDKROOT)/System/Library/Frameworks/Accelerate.framework',
                            '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
                        ],
                    },
                }, { # else: OS!="mac"
                    'sources/': [
                        ['exclude', 'WebInputEventFactoryMac.mm$'],
                        ['exclude', 'mac/WebScrollbarTheme.cpp$'],
                    ],
                }],
                ['OS!="win"', {
                    'sources/': [
                        ['exclude', 'WebInputEventFactoryWin.cpp$'],
                    ],
                }],
                ['use_default_render_theme==0', {
                    'sources/': [
                        ['exclude', 'default/WebRenderTheme.cpp'],
                    ],
                }],
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                    '../../',
                ],
            },
            'target_conditions': [
                ['OS=="android"', {
                    'sources/': [
                        ['include', '^linux/WebFontRendering\\.cpp$'],
                    ],
                }],
            ],
            'actions': [
                {
                    'action_name': 'PickerCommon',
                    'process_outputs_as_sources': 1,
                    'variables': {
                        'resources': [
                            'resources/pickerCommon.css',
                            'resources/pickerCommon.js',
                        ],
                    },
                    'inputs': [
                        '../build/scripts/make-file-arrays.py',
                        '<@(resources)',
                    ],
                    'outputs': [
                        '<(SHARED_INTERMEDIATE_DIR)/blink/PickerCommon.h',
                        '<(SHARED_INTERMEDIATE_DIR)/blink/PickerCommon.cpp',
                        ],
                    'action': [
                        'python',
                        '../build/scripts/make-file-arrays.py',
                        '--out-h=<(SHARED_INTERMEDIATE_DIR)/blink/PickerCommon.h',
                        '--out-cpp=<(SHARED_INTERMEDIATE_DIR)/blink/PickerCommon.cpp',
                        '<@(resources)',
                    ],
                },
                {
                    'action_name': 'CalendarPicker',
                    'process_outputs_as_sources': 1,
                    'variables': {
                        'resources': [
                            'resources/calendarPicker.css',
                            'resources/calendarPicker.js',
                            'resources/pickerButton.css',
                            'resources/suggestionPicker.css',
                            'resources/suggestionPicker.js',
                        ],
                    },
                    'inputs': [
                        '../build/scripts/make-file-arrays.py',
                        '<@(resources)'
                    ],
                    'outputs': [
                        '<(SHARED_INTERMEDIATE_DIR)/blink/CalendarPicker.h',
                        '<(SHARED_INTERMEDIATE_DIR)/blink/CalendarPicker.cpp',
                    ],
                    'action': [
                        'python',
                        '../build/scripts/make-file-arrays.py',
                        '--out-h=<(SHARED_INTERMEDIATE_DIR)/blink/CalendarPicker.h',
                        '--out-cpp=<(SHARED_INTERMEDIATE_DIR)/blink/CalendarPicker.cpp',
                        '<@(resources)',
                    ],
               },
                {
                    'action_name': 'ColorSuggestionPicker',
                    'process_outputs_as_sources': 1,
                    'variables': {
                        'resources': [
                            'resources/colorSuggestionPicker.css',
                            'resources/colorSuggestionPicker.js',
                        ],
                    },
                    'inputs': [
                        '../build/scripts/make-file-arrays.py',
                        '<@(resources)',
                    ],
                    'outputs': [
                        '<(SHARED_INTERMEDIATE_DIR)/blink/ColorSuggestionPicker.h',
                        '<(SHARED_INTERMEDIATE_DIR)/blink/ColorSuggestionPicker.cpp',
                    ],
                    'action': [
                        'python',
                        '../build/scripts/make-file-arrays.py',
                        '--out-h=<(SHARED_INTERMEDIATE_DIR)/blink/ColorSuggestionPicker.h',
                        '--out-cpp=<(SHARED_INTERMEDIATE_DIR)/blink/ColorSuggestionPicker.cpp',
                        '<@(resources)',
                    ],
                },
            ],
        },
        {
            'target_name': 'blink_web_test_support',
            'conditions': [
                ['component=="shared_library"', {
                    'type': 'none',
                }, {
                    'type': 'static_library',
                    'dependencies': [
                        '../config.gyp:config',
                        '../core/core.gyp:webcore_testing',
                        '../modules/modules.gyp:modules_testing',
                        '../wtf/wtf.gyp:wtf',
                        '<(DEPTH)/skia/skia.gyp:skia',
                    ],
                    'include_dirs': [
                        '../../',
                    ],
                    'sources': [
                        'WebTestingSupport.cpp',
                    ],
                }],
            ],
        },
    ], # targets
    'conditions': [
        ['gcc_version>=46', {
            'target_defaults': {
                # Disable warnings about c++0x compatibility, as some names (such
                # as nullptr) conflict with upcoming c++0x types.
                'cflags_cc': ['-Wno-c++0x-compat'],
            },
        }],
        ['OS=="mac"', {
            'targets': [
                {
                    'target_name': 'copy_mesa',
                    'type': 'none',
                    'dependencies': ['<(DEPTH)/third_party/mesa/mesa.gyp:osmesa'],
                    'copies': [{
                        'destination': '<(PRODUCT_DIR)/DumpRenderTree.app/Contents/MacOS/',
                        'files': ['<(PRODUCT_DIR)/osmesa.so'],
                    }],
                },
            ],
        }],
        ['clang==1', {
            'target_defaults': {
                'cflags': ['-Wglobal-constructors'],
                'xcode_settings': {
                    'WARNING_CFLAGS': ['-Wglobal-constructors'],
                },
            },
        }],
    ], # conditions
}
