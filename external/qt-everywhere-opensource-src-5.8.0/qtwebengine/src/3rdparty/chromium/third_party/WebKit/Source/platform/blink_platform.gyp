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
    '../build/features.gypi',
    '../build/scripts/scripts.gypi',
    '../build/win/precompile.gypi',
    'blink_platform.gypi',
    'heap/blink_heap.gypi',
  ],
  'targets': [{
    'target_name': 'blink_common',
    'type': '<(component)',
    'variables': { 'enable_wexit_time_destructors': 1 },
    'dependencies': [
      '<(DEPTH)/third_party/khronos/khronos.gyp:khronos_headers',
      '../config.gyp:config',
      '../wtf/wtf.gyp:wtf',
      '<(DEPTH)/base/base.gyp:base',
      # FIXME: Can we remove the dependency on Skia?
      '<(DEPTH)/skia/skia.gyp:skia',
      '<(DEPTH)/url/url.gyp:url_lib',
    ],
    'all_dependent_settings': {
      'include_dirs': [
        '..',
      ],
    },
    'export_dependent_settings': [
      '<(DEPTH)/skia/skia.gyp:skia',
    ],
    'defines': [
      'BLINK_COMMON_IMPLEMENTATION=1',
      'INSIDE_BLINK',
    ],
    'include_dirs': [
      '<(SHARED_INTERMEDIATE_DIR)/blink',
    ],
    'sources': [
      '../web/WebInputEvent.cpp',
      'exported/FilePathConversion.cpp',
      'exported/URLConversion.cpp',
      'exported/WebCString.cpp',
      'exported/WebString.cpp',
      'exported/linux/WebFontRenderStyle.cpp',
    ],
    'target_conditions': [
      ['OS=="android"', {
        'sources/': [
          ['include', 'exported/linux/WebFontRenderStyle\\.cpp$'],
        ],
      }],
    ],
  },
  {
    'target_name': 'blink_heap_asm_stubs',
    'type': 'static_library',
    # VS2010 does not correctly incrementally link obj files generated
    # from asm files. This flag disables UseLibraryDependencyInputs to
    # avoid this problem.
    'msvs_2010_disable_uldi_when_referenced': 1,
    'includes': [
      '../../../yasm/yasm_compile.gypi',
    ],
    'sources': [
      '<@(platform_heap_asm_files)',
    ],
    'variables': {
      'more_yasm_flags': [],
      'conditions': [
        ['OS == "mac"', {
          'more_yasm_flags': [
            # Necessary to ensure symbols end up with a _ prefix; added by
            # yasm_compile.gypi for Windows, but not Mac.
            '-DPREFIX',
          ],
        }],
        ['OS == "win" and target_arch == "x64"', {
          'more_yasm_flags': [
            '-DX64WIN=1',
          ],
        }],
        ['OS != "win" and target_arch == "x64"', {
          'more_yasm_flags': [
            '-DX64POSIX=1',
          ],
        }],
        ['target_arch == "ia32"', {
          'more_yasm_flags': [
            '-DIA32=1',
          ],
        }],
        ['target_arch == "arm"', {
          'more_yasm_flags': [
            '-DARM=1',
          ],
        }],
      ],
      'yasm_flags': [
        '>@(more_yasm_flags)',
      ],
      'yasm_output_path': '<(SHARED_INTERMEDIATE_DIR)/webcore/heap'
    },
  },
  {
    'target_name': 'blink_platform',
    'type': '<(component)',
    # Because of transitive dependency on make_platform_generated.
    'hard_dependency': 1,
    'dependencies': [
      '<(DEPTH)/third_party/khronos/khronos.gyp:khronos_headers',
      '../../public/blink.gyp:mojo_bindings',
      '../config.gyp:config',
      '../wtf/wtf.gyp:wtf',
      'blink_common',
      'blink_heap_asm_stubs',
      'platform_generated.gyp:make_platform_generated',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/cc/cc.gyp:cc',
      '<(DEPTH)/components/link_header_util/link_header_util.gyp:link_header_util',
      '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib',
      '<(DEPTH)/gpu/gpu.gyp:gles2_implementation',
      '<(DEPTH)/mojo/mojo_edk.gyp:mojo_system_impl',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings',
      '<(DEPTH)/mojo/mojo_public.gyp:mojo_cpp_bindings_wtf_support',
      '<(DEPTH)/net/net.gyp:net',
      '<(DEPTH)/skia/skia.gyp:skia',
      '<(DEPTH)/third_party/harfbuzz-ng/harfbuzz.gyp:harfbuzz-ng',
      '<(DEPTH)/third_party/iccjpeg/iccjpeg.gyp:iccjpeg',
      '<(DEPTH)/third_party/icu/icu.gyp:icui18n',
      '<(DEPTH)/third_party/icu/icu.gyp:icuuc',
      '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
      '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
      '<(DEPTH)/third_party/ots/ots.gyp:ots',
      '<(DEPTH)/third_party/qcms/qcms.gyp:qcms',
      '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
      '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
      '<(DEPTH)/url/url.gyp:url_lib',
      '<(DEPTH)/url/url.gyp:url_interfaces_mojom_for_blink',
      '<(DEPTH)/v8/src/v8.gyp:v8',
      '<(libjpeg_gyp_path):libjpeg',
    ],
    'export_dependent_settings': [
      'platform_generated.gyp:make_platform_generated',
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/cc/cc.gyp:cc',
      '<(DEPTH)/gpu/gpu.gyp:gles2_c_lib',
      '<(DEPTH)/skia/skia.gyp:skia',
      '<(DEPTH)/third_party/libpng/libpng.gyp:libpng',
      '<(DEPTH)/third_party/libwebp/libwebp.gyp:libwebp',
      '<(DEPTH)/third_party/ots/ots.gyp:ots',
      '<(DEPTH)/third_party/qcms/qcms.gyp:qcms',
      '<(DEPTH)/v8/src/v8.gyp:v8',
      '<(DEPTH)/url/url.gyp:url_lib',
      '<(DEPTH)/third_party/iccjpeg/iccjpeg.gyp:iccjpeg',
      '<(libjpeg_gyp_path):libjpeg',
    ],
    'defines': [
      'BLINK_PLATFORM_IMPLEMENTATION=1',
      'INSIDE_BLINK',
    ],
    'include_dirs': [
      '<(angle_path)/include',
      '<(SHARED_INTERMEDIATE_DIR)/blink',
    ],
    'xcode_settings': {
      # Some Mac-specific parts of WebKit won't compile without having this
      # prefix header injected.
      'GCC_PREFIX_HEADER': '<(DEPTH)/third_party/WebKit/Source/build/mac/Prefix.h',
    },
    'sources': [
      '<@(platform_files)',
      '<@(platform_heap_files)',

      # Additional .cpp files from platform_generated.gyp:make_platform_generated actions.
      '<(blink_platform_output_dir)/CharacterPropertyData.cpp',
      '<(blink_platform_output_dir)/ColorData.cpp',
      '<(blink_platform_output_dir)/FontFamilyNames.cpp',
      '<(blink_platform_output_dir)/HTTPNames.cpp',
      '<(blink_platform_output_dir)/RuntimeEnabledFeatures.cpp',
      '<(blink_platform_output_dir)/RuntimeEnabledFeatures.h',

      # Additional .cpp files from the protocol_sources list.
      '<(blink_platform_output_dir)/v8_inspector/protocol/Console.cpp',
      '<(blink_platform_output_dir)/v8_inspector/protocol/Debugger.cpp',
      '<(blink_platform_output_dir)/v8_inspector/protocol/HeapProfiler.cpp',
      '<(blink_platform_output_dir)/v8_inspector/protocol/Profiler.cpp',
      '<(blink_platform_output_dir)/v8_inspector/protocol/Runtime.cpp',

      # Additional .cpp files from the v8_inspector.
      '<(blink_platform_output_dir)/v8_inspector/DebuggerScript.h',
      '<(blink_platform_output_dir)/v8_inspector/InjectedScriptSource.h',
    ],
    'sources/': [
      # Exclude all platform specific things, reinclude them below on a per-platform basis
      # FIXME: Figure out how to store these patterns in a variable.
      ['exclude', '(cf|cg|mac|win)/'],
      ['exclude', '(?<!Chromium)(CF|CG|Mac|Win)\\.(cpp|mm?)$'],

      # *NEON.cpp files need special compile options.
      # They are moved to the webcore_0_neon target.
      ['exclude', 'graphics/cpu/arm/.*NEON\\.(cpp|h)'],
      ['exclude', 'graphics/cpu/arm/filters/.*NEON\\.(cpp|h)'],
    ],
    # Disable c4267 warnings until we fix size_t to int truncations.
    # Disable c4724 warnings which is generated in VS2012 due to improper
    # compiler optimizations, see crbug.com/237063
    'msvs_disabled_warnings': [ 4267, 4334, 4724 ],
    'conditions': [
      ['target_arch == "ia32" or target_arch == "x64"', {
        'sources/': [
          ['include', 'graphics/cpu/x86/WebGLImageConversionSSE\\.h$'],
        ],
      }],
      ['OS=="linux" or OS=="android"', {
        'sources/': [
          ['include', 'fonts/linux/FontPlatformDataLinux\\.cpp$'],
        ]
      }, { # OS!="linux" and OS!="android"
        'sources/': [
          ['exclude', 'fonts/linux/FontPlatformDataLinux\\.cpp$'],
        ]
      }],
      ['OS=="mac"', {
        'link_settings': {
          'libraries': [
            '$(SDKROOT)/System/Library/Frameworks/Accelerate.framework',
            '$(SDKROOT)/System/Library/Frameworks/Carbon.framework',
            '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
          ]
        },
        'sources/': [
          # We use LocaleMac.mm instead of LocaleICU.cpp
          ['exclude', 'text/LocaleICU\\.(cpp|h)$'],
          ['include', 'text/LocaleMac\\.mm$'],

          # The Mac uses mac/KillRingMac.mm instead of the dummy
          # implementation.
          ['exclude', 'KillRingNone\\.cpp$'],

          # The Mac build uses Core Foundation.
          ['include', 'CF\\.cpp$'],

          # Use native Mac font code from core.
          ['include', '(fonts/)?mac/[^/]*Font[^/]*\\.(cpp|mm?)$'],

          ['include', 'text/mac/HyphenationMac\\.cpp$'],

          # Cherry-pick some files that can't be included by broader regexps.
          # Some of these are used instead of Chromium platform files, see
          # the specific exclusions in the "exclude" list below.
          ['include', 'audio/mac/FFTFrameMac\\.cpp$'],
          ['include', 'fonts/mac/GlyphPageTreeNodeMac\\.cpp$'],
          ['include', 'mac/ColorMac\\.mm$'],
          ['include', 'mac/BlockExceptions\\.mm$'],
          ['include', 'mac/KillRingMac\\.mm$'],
          ['include', 'mac/LocalCurrentGraphicsContext\\.mm$'],
          ['include', 'mac/NSScrollerImpDetails\\.mm$'],
          ['include', 'mac/ScrollAnimatorMac\\.mm$'],
          ['include', 'mac/ThemeMac\\.h$'],
          ['include', 'mac/ThemeMac\\.mm$'],
          ['include', 'mac/VersionUtilMac\\.h$'],
          ['include', 'mac/VersionUtilMac\\.mm$'],
          ['include', 'mac/WebCoreNSCellExtras\\.h$'],
          ['include', 'mac/WebCoreNSCellExtras\\.mm$'],
          ['include', 'scroll/ScrollbarThemeMac\\.h$'],
          ['include', 'scroll/ScrollbarThemeMac\\.mm$'],

          # Mac uses only ScrollAnimatorMac.
          ['exclude', 'scroll/ScrollAnimator\\.cpp$'],
          ['exclude', 'scroll/ScrollAnimator\\.h$'],

          ['exclude', 'fonts/skia/FontCacheSkia\\.cpp$'],

          ['include', 'geometry/mac/FloatPointMac\\.mm$'],
          ['include', 'geometry/mac/FloatRectMac\\.mm$'],
          ['include', 'geometry/mac/FloatSizeMac\\.mm$'],
          ['include', 'geometry/mac/IntPointMac\\.mm$'],
          ['include', 'geometry/mac/IntRectMac\\.mm$'],

          ['include', 'geometry/cg/FloatPointCG\\.cpp$'],
          ['include', 'geometry/cg/FloatRectCG\\.cpp$'],
          ['include', 'geometry/cg/FloatSizeCG\\.cpp$'],
          ['include', 'geometry/cg/IntPointCG\\.cpp$'],
          ['include', 'geometry/cg/IntRectCG\\.cpp$'],
          ['include', 'geometry/cg/IntSizeCG\\.cpp$'],
        ],
        'conditions': [
          ['appstore_compliant_code==1', {
            'sources/': [
              ['include', 'KillRingNone\\.cpp$'],
              ['exclude', 'mac/KillRingMac\\.mm$']
            ]
          }]
        ],
      }, { # OS!="mac"
        'sources/': [
          ['exclude', 'mac/'],
          ['exclude', 'geometry/mac/'],
          ['exclude', 'geometry/cg/'],
          ['exclude', 'scroll/ScrollbarThemeMac'],
        ],
      }],
      ['OS != "linux" and OS != "mac" and OS != "win"', {
        'sources/': [
          ['exclude', 'VDMX[^/]+\\.(cpp|h)$'],
        ],
      }],
      ['OS=="win"', {
        'sources/': [
          # We use LocaleWin.cpp instead of LocaleICU.cpp
          ['exclude', 'text/LocaleICU\\.(cpp|h)$'],
          ['include', 'text/LocaleWin\\.(cpp|h)$'],

          ['include', 'clipboard/ClipboardUtilitiesWin\\.(cpp|h)$'],

          ['include', 'fonts/win/FontCacheSkiaWin\\.cpp$'],
          ['include', 'fonts/win/FontFallbackWin\\.(cpp|h)$'],
          ['include', 'fonts/win/FontPlatformDataWin\\.cpp$'],

          ['include', 'text/win/HyphenationWin\\.cpp$'],

          # SystemInfo.cpp is useful and we don't want to copy it.
          ['include', 'win/SystemInfo\\.cpp$'],
        ],
      }, { # OS!="win"
        'sources/': [
          ['exclude', 'win/'],
          ['exclude', 'Win\\.cpp$'],
          ['exclude', '/(Windows)[^/]*\\.cpp$'],
        ],
      }],
      ['OS=="win" and chromium_win_pch==1', {
        'sources/': [
          ['include', '<(DEPTH)/third_party/WebKit/Source/build/win/Precompile.cpp'],
        ],
      }],
      ['OS=="android"', {
        'sources/': [
          ['include', '^fonts/VDMXParser\\.cpp$'],
        ],
      }, { # OS!="android"
        'sources/': [
          ['exclude', 'Android\\.cpp$'],
        ],
      }],
      ['use_default_render_theme==0', {
        'sources/': [
          ['exclude', 'scroll/ScrollbarThemeAura\\.(cpp|h)'],
        ],
      }],
      ['"WTF_USE_WEBAUDIO_FFMPEG=1" in feature_defines', {
        'include_dirs': [
          '<(DEPTH)/third_party/ffmpeg',
        ],
        'dependencies': [
          '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        ],
      }],
      ['"WTF_USE_WEBAUDIO_OPENMAX_DL_FFT=1" in feature_defines', {
         'include_dirs': [
           '<(DEPTH)/third_party/openmax_dl',
         ],
        'dependencies': [
          '<(DEPTH)/third_party/openmax_dl/dl/dl.gyp:openmax_dl',
        ],
      }],
      ['target_arch=="arm"', {
        'dependencies': [
          'blink_arm_neon',
        ],
      }],
    ],
    'target_conditions': [
      ['OS=="android"', {
        'sources/': [
          ['include', 'fonts/linux/FontPlatformDataLinux\\.cpp$'],
        ],
      }],
    ],
  },
  # The *NEON.cpp files fail to compile when -mthumb is passed. Force
  # them to build in ARM mode.
  # See https://bugs.webkit.org/show_bug.cgi?id=62916.
  {
    'target_name': 'blink_arm_neon',
    'conditions': [
      ['target_arch=="arm"', {
        'type': 'static_library',
        'dependencies': [
          '<(DEPTH)/third_party/khronos/khronos.gyp:khronos_headers',
          'blink_common',
        ],
        'hard_dependency': 1,
        'sources': [
          '<@(platform_files)',
        ],
        'sources/': [
          ['exclude', '.*'],
          ['include', 'graphics/cpu/arm/filters/.*NEON\\.(cpp|h)'],
        ],
        'cflags': ['-marm'],
        'conditions': [
          ['OS=="android"', {
            'cflags!': ['-mthumb'],
          }],
        ],
      },{  # target_arch!="arm"
        'type': 'none',
      }],
    ],
  }],
}
