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
    'variables': {
      'blink_web_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/web',
    },
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
    'target_defaults': {
        'variables': {
            'clang_warning_flags': ['-Wglobal-constructors'],
        },
    },
    'targets': [
        {
            'target_name': 'blink_web',
            'type': '<(component)',
            'variables': { 'enable_wexit_time_destructors': 1, },
            'dependencies': [
                '<(DEPTH)/third_party/khronos/khronos.gyp:khronos_headers',
                '../config.gyp:config',
                '../platform/blink_platform.gyp:blink_common',
                '../modules/modules.gyp:modules',
                '<(DEPTH)/base/base.gyp:base',
                '<(DEPTH)/cc/cc.gyp:cc',
                '<(DEPTH)/skia/skia.gyp:skia',
                '<(angle_path)/src/angle.gyp:translator',
                '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
                '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
                '<(DEPTH)/v8/src/v8.gyp:v8',
            ],
            'export_dependent_settings': [
                '<(DEPTH)/skia/skia.gyp:skia',
                '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
                '<(DEPTH)/v8/src/v8.gyp:v8',
            ],
            'include_dirs': [
                '<(angle_path)/include',
            ],
            'defines': [
                'BLINK_WEB_IMPLEMENTATION=1',
                'BLINK_IMPLEMENTATION=1',
                'INSIDE_BLINK',
            ],
            'sources': [
                '<@(web_files)',
            ],
            'conditions': [
                ['component=="shared_library"', {
                    'dependencies': [
                        '../core/core.gyp:webcore_shared',
                        '../platform/blink_platform.gyp:blink_common',
                        '../platform/blink_platform.gyp:blink_platform',
                        '../wtf/wtf.gyp:wtf',
                        '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
                        '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
                        '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
                        '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
                        '<(DEPTH)/third_party/ots/ots.gyp:ots',
                        '<(DEPTH)/third_party/zlib/zlib.gyp:zlib',
                        '<(DEPTH)/url/url.gyp:url_lib',
                        '<(DEPTH)/v8/src/v8.gyp:v8',
                        '<(libjpeg_gyp_path):libjpeg',
                        # We must not add webkit_support here because of cyclic dependency.
                    ],
                    'export_dependent_settings': [
                        '<(DEPTH)/url/url.gyp:url_lib',
                        '<(DEPTH)/v8/src/v8.gyp:v8',
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
                }, {
                     # component=="static_library"
                     'dependencies': [
                        '../core/core.gyp:webcore',
                     ],
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
                ['OS=="mac"', {
                    'link_settings': {
                        'libraries': [
                            '$(SDKROOT)/System/Library/Frameworks/Accelerate.framework',
                            '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
                        ],
                    },
                }, { # else: OS!="mac"
                    'sources/': [
                        ['exclude', 'mac/WebScrollbarTheme.cpp$'],
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
        },
        {
            # GN version: //third_party/WebKit/Source/web:test_support
            'target_name': 'blink_web_test_support',
            # Because of transitive dependency on make_platform_generated.
            'hard_dependency': 1,
            'type': 'static_library',
            'dependencies': [
                '../config.gyp:config',
                '../core/core.gyp:webcore_testing',
                '../modules/modules.gyp:modules_testing',
                '../wtf/wtf.gyp:wtf',
                '<(DEPTH)/skia/skia.gyp:skia',
                '<(DEPTH)/v8/src/v8.gyp:v8',
            ],
            'include_dirs': [
                '../../',
                '<(SHARED_INTERMEDIATE_DIR)/blink',  # gen/blink
            ],
            'sources': [
                'WebTestingSupport.cpp',
            ],
            'conditions': [
                ['component!="shared_library"', {
                    'dependencies': [
                        '../core/core.gyp:webcore',
                        '../core/core.gyp:webcore_generated',
                    ],
                }, {
                    'dependencies': [
                        '../core/core.gyp:webcore_shared',
                    ],
                }],
            ],
        },
    ], # targets
    'conditions': [
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
    ], # conditions
}
