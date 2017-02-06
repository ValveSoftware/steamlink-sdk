# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # A hook that can be overridden in other repositories to add additional
    # compilation targets to 'All'.
    'app_targets%': [],
    # For Android-specific targets.
    'android_app_targets%': [],
  },
  'includes': [
    '../third_party/openh264/openh264_args.gypi',
  ],
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'xcode_create_dependents_test_runner': 1,
      'dependencies': [
        '<@(app_targets)',
        'some.gyp:*',
        '../base/base.gyp:*',
        '../components/components.gyp:*',
        '../components/components_tests.gyp:*',
        '../crypto/crypto.gyp:*',
        '../net/net.gyp:*',
        '../sdch/sdch.gyp:*',
        '../sql/sql.gyp:*',
        '../testing/gmock.gyp:*',
        '../testing/gtest.gyp:*',
        '../third_party/boringssl/boringssl.gyp:*',
        '../third_party/icu/icu.gyp:*',
        '../third_party/libxml/libxml.gyp:*',
        '../third_party/sqlite/sqlite.gyp:*',
        '../third_party/zlib/zlib.gyp:*',
        '../ui/accessibility/accessibility.gyp:*',
        '../ui/base/ui_base.gyp:*',
        '../ui/display/display.gyp:display_unittests',
        '../ui/native_theme/native_theme.gyp:native_theme_unittests',
        '../ui/snapshot/snapshot.gyp:*',
        '../url/url.gyp:*',
      ],
      'conditions': [
        ['OS!="ios" and OS!="mac"', {
          'dependencies': [
            '../ui/touch_selection/ui_touch_selection.gyp:*',
          ],
        }],
        ['OS=="ios"', {
          'dependencies': [
            '../ios/ios.gyp:*',
            # NOTE: This list of targets is present because
            # mojo_base.gyp:mojo_base cannot be built on iOS, as
            # javascript-related targets cause v8 to be built.
            # TODO(crbug.com/605508): http://crrev.com/1832703002 introduced
            # a dependency on //third_party/WebKit that cause build failures
            # when using Xcode version of clang (loading clang plugin fails).
            # '../mojo/mojo_base.gyp:mojo_common_lib',
            # '../mojo/mojo_base.gyp:mojo_common_unittests',
            # '../mojo/mojo_edk.gyp:mojo_system_impl',
            # '../mojo/mojo_edk_tests.gyp:mojo_public_bindings_unittests',
            # '../mojo/mojo_edk_tests.gyp:mojo_public_system_unittests',
            # '../mojo/mojo_edk_tests.gyp:mojo_system_unittests',
            # '../mojo/mojo_public.gyp:mojo_cpp_bindings',
            # '../mojo/mojo_public.gyp:mojo_public_test_utils',
            # '../mojo/mojo_public.gyp:mojo_system',
            '../google_apis/google_apis.gyp:google_apis_unittests',
            '../skia/skia_tests.gyp:skia_unittests',
            '../ui/base/ui_base_tests.gyp:ui_base_unittests',
            '../ui/gfx/gfx_tests.gyp:gfx_unittests',
          ],
        }, { # 'OS!="ios"
          'dependencies': [
            '../content/content.gyp:*',
            '../device/bluetooth/bluetooth.gyp:*',
            '../device/device_tests.gyp:*',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '../content/content_shell_and_tests.gyp:content_shell_apk',
            '<@(android_app_targets)',
            'android_builder_tests',
            '../third_party/catapult/telemetry/telemetry.gyp:*#host',
            # TODO(nyquist) This should instead by a target for sync when all of
            # the sync-related code for Android has been upstreamed.
            # See http://crbug.com/159203
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_javalib',
          ],
          'conditions': [
            ['chromecast==0', {
              'dependencies': [
                '../android_webview/android_webview.gyp:android_webview_apk',
                '../android_webview/android_webview_shell.gyp:system_webview_shell_apk',
                '../chrome/android/chrome_apk.gyp:chrome_public_apk',
                '../chrome/android/chrome_apk.gyp:chrome_sync_shell_apk',
              ],
            }],
            ['chromecast==0 and use_webview_internal_framework==0', {
              'dependencies': [
                '../android_webview/android_webview.gyp:system_webview_apk',
              ],
            }],
            # TODO: Enable packed relocations for x64. See: b/20532404
            ['target_arch != "x64"', {
              'dependencies': [
                '../third_party/android_platform/relocation_packer.gyp:android_relocation_packer_unittests#host',
              ],
            }],
          ],
        }, {
          'dependencies': [
            # TODO: This should build on Android and the target should move to the list above.
            '../sync/sync.gyp:*',
          ],
          'conditions': [
            ['OS!="ios"', {
              'dependencies': [
                '../content/content_shell_and_tests.gyp:*',
              ],
            }],
          ],
        }],
        ['OS!="ios" and OS!="android" and chromecast==0', {
          'dependencies': [
            '../third_party/re2/re2.gyp:re2',
            '../chrome/chrome.gyp:*',
            '../cc/blink/cc_blink_tests.gyp:*',
            '../cc/cc_tests.gyp:*',
            '../device/usb/usb.gyp:*',
            '../extensions/extensions.gyp:*',
            '../extensions/extensions_tests.gyp:*',
            '../gin/gin.gyp:*',
            '../gpu/gpu.gyp:*',
            '../gpu/tools/tools.gyp:*',
            '../ipc/ipc.gyp:*',
            '../jingle/jingle.gyp:*',
            '../media/capture/capture.gyp:*',
            '../media/cast/cast.gyp:*',
            '../media/media.gyp:*',
            '../media/midi/midi.gyp:*',
            '../mojo/mojo.gyp:*',
            '../mojo/mojo_base.gyp:*',
            '../ppapi/ppapi.gyp:*',
            '../ppapi/ppapi_internal.gyp:*',
            '../ppapi/tools/ppapi_tools.gyp:*',
            '../services/shell/shell.gyp:*',
            '../skia/skia.gyp:*',
            '../sync/tools/sync_tools.gyp:*',
            '../third_party/catapult/telemetry/telemetry.gyp:*',
            '../third_party/WebKit/public/all.gyp:*',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:*',
            '../third_party/codesighs/codesighs.gyp:*',
            '../third_party/ffmpeg/ffmpeg.gyp:*',
            '../third_party/iccjpeg/iccjpeg.gyp:*',
            '../third_party/libpng/libpng.gyp:*',
            '../third_party/libusb/libusb.gyp:*',
            '../third_party/libwebp/libwebp.gyp:*',
            '../third_party/libxslt/libxslt.gyp:*',
            '../third_party/lzma_sdk/lzma_sdk.gyp:*',
            '../third_party/mesa/mesa.gyp:*',
            '../third_party/modp_b64/modp_b64.gyp:*',
            '../third_party/ots/ots.gyp:*',
            '../third_party/pdfium/samples/samples.gyp:*',
            '../third_party/qcms/qcms.gyp:*',
            '../tools/battor_agent/battor_agent.gyp:*',
            '../tools/gn/gn.gyp:*',
            '../tools/perf/clear_system_cache/clear_system_cache.gyp:*',
            '../v8/src/v8.gyp:*',
            '<(libjpeg_gyp_path):*',
          ],
        }],
        ['OS=="win" or OS=="ios" or OS=="linux"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:*',
           ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            '../sandbox/sandbox.gyp:*',
            '../third_party/crashpad/crashpad/crashpad.gyp:*',
            '../third_party/ocmock/ocmock.gyp:*',
          ],
          'conditions': [
            ['enable_ipc_fuzzer==1', {
              'dependencies': [
                '../tools/ipc_fuzzer/ipc_fuzzer.gyp:*',
              ],
            }],
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../courgette/courgette.gyp:*',
            '../sandbox/sandbox.gyp:*',
          ],
          'conditions': [
            ['branding=="Chrome"', {
              'dependencies': [
                '../chrome/chrome.gyp:linux_packages_<(channel)',
              ],
            }],
            ['enable_ipc_fuzzer==1', {
              'dependencies': [
                '../tools/ipc_fuzzer/ipc_fuzzer.gyp:*',
              ],
            }],
            ['use_dbus==1', {
              'dependencies': [
                '../dbus/dbus.gyp:*',
              ],
            }],
          ],
        }],
        ['chromecast==1', {
          'dependencies': [
            '../chromecast/chromecast.gyp:*',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:*',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../chrome_elf/chrome_elf.gyp:*',
            '../courgette/courgette.gyp:*',
            '../rlz/rlz.gyp:*',
            '../sandbox/sandbox.gyp:*',
            '<(angle_path)/src/angle.gyp:*',
            '../third_party/bspatch/bspatch.gyp:*',
            '../tools/win/static_initializers/static_initializers.gyp:*',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/controls/webview/webview.gyp:*',
            '../ui/views/views.gyp:*',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../ash/ash.gyp:*',
            '../ui/app_list/app_list.gyp:*',
            '../ui/aura/aura.gyp:*',
            '../ui/aura_extra/aura_extra.gyp:*',
          ],
        }],
        ['remoting==1', {
          'dependencies': [
            '../remoting/remoting_all.gyp:remoting_all',
          ],
        }],
        ['OS!="ios"', {
          'dependencies': [
            '../third_party/boringssl/boringssl_tests.gyp:*',
          ],
        }],
        ['OS!="android" and OS!="ios"', {
          'dependencies': [
            '../google_apis/gcm/gcm.gyp:*',
          ],
        }],
        ['(chromeos==1 or OS=="linux" or OS=="win" or OS=="mac") and chromecast==0', {
          'dependencies': [
            '../extensions/shell/app_shell.gyp:*',
          ],
        }],
        ['envoy==1', {
          'dependencies': [
            '../envoy/envoy.gyp:*',
          ],
        }],
        ['use_openh264==1', {
          'dependencies': [
            '../third_party/openh264/openh264.gyp:*',
          ],
        }],
        ['enable_basic_printing==1 or enable_print_preview==1', {
          'dependencies': [
            '../printing/printing.gyp:*',
          ],
        }],
      ],
    }, # target_name: All
    {
      'target_name': 'All_syzygy',
      'type': 'none',
      'conditions': [
        ['OS=="win" and fastbuild==0 and target_arch=="ia32" and '
            '(syzyasan==1 or syzygy_optimize==1)', {
          'dependencies': [
            '../chrome/installer/mini_installer_syzygy.gyp:*',
          ],
        }],
      ],
    }, # target_name: All_syzygy
    {
      # Note: Android uses android_builder_tests below.
      # TODO: Consider merging that with this target.
      'target_name': 'chromium_builder_tests',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_unittests',
        '../components/components_tests.gyp:components_unittests',
        '../crypto/crypto.gyp:crypto_unittests',
        '../net/net.gyp:net_unittests',
        '../skia/skia_tests.gyp:skia_unittests',
        '../sql/sql.gyp:sql_unittests',
        '../sync/sync.gyp:sync_unit_tests',
        '../ui/base/ui_base_tests.gyp:ui_base_unittests',
        '../ui/display/display.gyp:display_unittests',
        '../ui/gfx/gfx_tests.gyp:gfx_unittests',
        '../url/url.gyp:url_unittests',
      ],
      'conditions': [
        ['OS!="ios"', {
          'dependencies': [
            '../ui/gl/gl_tests.gyp:gl_unittests',
            '../url/ipc/url_ipc.gyp:url_ipc_unittests',
          ],
        }],
        ['OS!="ios" and OS!="mac"', {
          'dependencies': [
            '../ui/touch_selection/ui_touch_selection.gyp:ui_touch_selection_unittests',
          ],
        }],
        ['OS!="ios" and OS!="android"', {
          'dependencies': [
            '../cc/blink/cc_blink_tests.gyp:cc_blink_unittests',
            '../cc/cc_tests.gyp:cc_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_shell',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../gin/gin.gyp:gin_unittests',
            '../google_apis/google_apis.gyp:google_apis_unittests',
            '../gpu/gles2_conform_support/gles2_conform_support.gyp:gles2_conform_support',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/capture/capture.gyp:capture_unittests',
            '../media/cast/cast.gyp:cast_unittests',
            '../media/media.gyp:media_unittests',
            '../media/midi/midi.gyp:midi_unittests',
            '../mojo/mojo.gyp:mojo',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../third_party/catapult/telemetry/telemetry.gyp:*',
            '../third_party/WebKit/public/all.gyp:all_blink',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/leveldatabase/leveldatabase.gyp:env_chromium_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
          ],
        }],
        ['OS!="ios" and OS!="android" and chromecast==0', {
          'dependencies': [
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:chromedriver_tests',
            '../chrome/chrome.gyp:chromedriver_unittests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../extensions/extensions_tests.gyp:extensions_browsertests',
            '../extensions/extensions_tests.gyp:extensions_unittests',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../chrome/chrome.gyp:installer_util_unittests',
            '../chrome/chrome.gyp:install_static_unittests',
            '../chrome/chrome.gyp:setup_unittests',
            # ../chrome/test/mini_installer requires mini_installer.
            '../chrome/installer/mini_installer.gyp:mini_installer',
            '../chrome_elf/chrome_elf.gyp:chrome_elf_unittests',
            '../courgette/courgette.gyp:courgette_unittests',
            '../sandbox/sandbox.gyp:sbox_integration_tests',
            '../sandbox/sandbox.gyp:sbox_unittests',
            '../sandbox/sandbox.gyp:sbox_validation_tests',
          ],
          'conditions': [
            # remoting_host_installation uses lots of non-trivial GYP that tend
            # to break because of differences between ninja and msbuild. Make
            # sure this target is built by the builders on the main waterfall.
            # See http://crbug.com/180600.
            ['wix_exists == "True"', {
              'dependencies': [
                '../remoting/remoting.gyp:remoting_host_installation',
              ],
            }],
            ['syzyasan==1', {
              'variables': {
                # Disable incremental linking for all modules.
                # 0: inherit, 1: disabled, 2: enabled.
                'msvs_debug_link_incremental': '1',
                'msvs_large_module_debug_link_mode': '1',
                # Disable RTC. Syzygy explicitly doesn't support RTC
                # instrumented binaries for now.
                'win_debug_RuntimeChecks': '0',
              },
              'defines': [
                # Disable iterator debugging (huge speed boost).
                '_HAS_ITERATOR_DEBUGGING=0',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                  # Enable profile information (necessary for SyzyAsan
                  # instrumentation). This is incompatible with incremental
                  # linking.
                  'Profile': 'true',
                },
              }
            }],
            ['component!="shared_library" or target_arch!="ia32"', {
              'dependencies': [
                '../chrome/installer/mini_installer.gyp:next_version_mini_installer',
              ],
            }],
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../ui/chromeos/ui_chromeos.gyp:ui_chromeos_unittests',
            '../ui/arc/arc.gyp:ui_arc_unittests',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../sandbox/sandbox.gyp:sandbox_linux_unittests',
          ],
        }],
        ['OS=="linux" and use_dbus==1', {
          'dependencies': [
            '../dbus/dbus.gyp:dbus_unittests',
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            '../ui/message_center/message_center.gyp:*',
          ],
        }],
        ['test_isolation_mode != "noop"', {
          'dependencies': [
            'chromium_swarm_tests',
          ],
        }],
        ['OS!="android"', {
          'dependencies': [
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
          ],
        }],
        ['enable_basic_printing==1 or enable_print_preview==1', {
          'dependencies': [
            '../printing/printing.gyp:printing_unittests',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../ash/ash.gyp:ash_unittests',
            '../ui/app_list/app_list.gyp:app_list_unittests',
            '../ui/app_list/presenter/app_list_presenter.gyp:app_list_presenter_unittests',
            '../ui/aura/aura.gyp:aura_unittests',
            '../ui/compositor/compositor.gyp:compositor_unittests',
          ],
        }],
        ['use_aura==1 and chromecast==0', {
          'dependencies': [
            '../ui/keyboard/keyboard.gyp:keyboard_unittests',
            '../ui/views/views.gyp:views_unittests',
          ],
        }],
        ['use_aura==1 or toolkit_views==1', {
          'dependencies': [
            '../ui/events/events_unittests.gyp:events_unittests',
          ],
        }],
        ['disable_nacl==0', {
          'dependencies': [
            '../components/nacl.gyp:nacl_loader_unittests',
          ],
        }],
        ['disable_nacl==0 and disable_nacl_untrusted==0 and enable_nacl_nonsfi_test==1', {
          'dependencies': [
            '../components/nacl.gyp:nacl_helper_nonsfi_unittests',
          ],
        }],
      ],
    }, # target_name: chromium_builder_tests
  ],
  'conditions': [
    # TODO(GYP): make gn_migration.gypi work unconditionally.
    ['OS=="mac" or OS=="win" or (OS=="android" and chromecast==0) or (OS=="linux" and target_arch=="x64" and chromecast==0)', {
      'includes': [
        'gn_migration.gypi',
      ],
    }],
    ['OS!="ios"', {
      'targets': [
        {
          'target_name': 'blink_tests',
          'type': 'none',
          'dependencies': [
            '../third_party/WebKit/public/all.gyp:all_blink',
          ],
          'conditions': [
            ['OS=="android"', {
              'dependencies': [
                '../content/content_shell_and_tests.gyp:content_shell_apk',
                '../breakpad/breakpad.gyp:dump_syms#host',
                '../breakpad/breakpad.gyp:minidump_stackwalk#host',
                '../tools/imagediff/image_diff.gyp:image_diff#host',
              ],
            }, {  # OS!="android"
              'dependencies': [
                '../content/content_shell_and_tests.gyp:content_shell',
                '../tools/imagediff/image_diff.gyp:image_diff',
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                '../components/test_runner/test_runner.gyp:layout_test_helper',
                '../content/content_shell_and_tests.gyp:content_shell_crash_service',
              ],
            }],
            ['OS!="win" and OS!="android"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:minidump_stackwalk',
              ],
            }],
            ['OS=="mac"', {
              'dependencies': [
                '../components/test_runner/test_runner.gyp:layout_test_helper',
                '../breakpad/breakpad.gyp:dump_syms#host',
              ],
            }],
            ['OS=="linux"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:dump_syms#host',
              ],
            }],
          ],
        }, # target_name: blink_tests
      ],
    }], # OS!=ios
    ['OS!="ios" and OS!="android" and chromecast==0', {
      'targets': [
        {
          'target_name': 'chromium_builder_nacl_win_integration',
          'type': 'none',
          'dependencies': [
            'chromium_builder_tests',
          ],
        }, # target_name: chromium_builder_nacl_win_integration
        {
          'target_name': 'chromium_builder_perf',
          'type': 'none',
          'dependencies': [
            '../cc/cc_tests.gyp:cc_perftests',
            '../chrome/chrome.gyp:chrome',
            '../chrome/chrome.gyp:load_library_perf_tests',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../gpu/gpu.gyp:gpu_perftests',
            '../media/media.gyp:media_perftests',
            '../tools/perf/clear_system_cache/clear_system_cache.gyp:*',
            '../third_party/catapult/telemetry/telemetry.gyp:*',
          ],
          'conditions': [
            ['OS!="ios" and OS!="win"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:minidump_stackwalk',
              ],
            }],
            ['OS=="linux"', {
              'dependencies': [
                '../chrome/chrome.gyp:linux_symbols'
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                '../chrome/installer/mini_installer.gyp:mini_installer',
                '../gpu/gpu.gyp:angle_perftests',
              ],
            }],
          ],
        }, # target_name: chromium_builder_perf
        {
          'target_name': 'chromium_gpu_builder',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chrome',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../gpu/gles2_conform_support/gles2_conform_test.gyp:gles2_conform_test',
            '../gpu/khronos_glcts_support/khronos_glcts_test.gyp:khronos_glcts_test',
            '../gpu/gpu.gyp:gl_tests',
            '../gpu/gpu.gyp:angle_unittests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../gpu/gpu.gyp:command_buffer_gles2_tests',
            '../third_party/catapult/telemetry/telemetry.gyp:*',
          ],
          'conditions': [
            ['OS!="ios" and OS!="win"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:minidump_stackwalk',
              ],
            }],
            ['OS=="linux"', {
              'dependencies': [
                '../chrome/chrome.gyp:linux_symbols'
              ],
            }],
          ],
        }, # target_name: chromium_gpu_builder
        {
          'target_name': 'chromium_gpu_debug_builder',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chrome',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../gpu/gles2_conform_support/gles2_conform_test.gyp:gles2_conform_test',
            '../gpu/khronos_glcts_support/khronos_glcts_test.gyp:khronos_glcts_test',
            '../gpu/gpu.gyp:gl_tests',
            '../gpu/gpu.gyp:angle_unittests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../gpu/gpu.gyp:command_buffer_gles2_tests',
            '../third_party/catapult/telemetry/telemetry.gyp:*',
          ],
          'conditions': [
            ['OS!="ios" and OS!="win"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:minidump_stackwalk',
              ],
            }],
            ['OS=="linux"', {
              'dependencies': [
                '../chrome/chrome.gyp:linux_symbols'
              ],
            }],
          ],
        }, # target_name: chromium_gpu_debug_builder
        {
          # This target contains everything we need to run tests on the special
          # device-equipped WebRTC bots. We have device-requiring tests in
          # browser_tests and content_browsertests.
          'target_name': 'chromium_builder_webrtc',
          'type': 'none',
          'dependencies': [
            'chromium_builder_perf',
            '../chrome/chrome.gyp:browser_tests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../media/capture/capture.gyp:capture_unittests',
            '../media/media.gyp:media_unittests',
            '../media/midi/midi.gyp:midi_unittests',
            '../third_party/webrtc/tools/tools.gyp:frame_analyzer',
            '../third_party/webrtc/tools/tools.gyp:rgba_to_i420_converter',
          ],
          'conditions': [
            ['remoting==1', {
              'dependencies': [
                '../remoting/remoting.gyp:*',
              ],
            }],
          ],
        },  # target_name: chromium_builder_webrtc
        {
          'target_name': 'chromium_builder_chromedriver',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chromedriver',
            '../chrome/chrome.gyp:chromedriver_tests',
            '../chrome/chrome.gyp:chromedriver_unittests',
          ],
        },  # target_name: chromium_builder_chromedriver
        {
          'target_name': 'chromium_builder_asan',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chrome',

            # We refer to content_shell directly rather than blink_tests
            # because we don't want the _unittests binaries.
            '../content/content_shell_and_tests.gyp:content_shell',

            '../v8/src/d8.gyp:d8',
          ],
          'conditions': [
            ['OS!="win"', {
              'dependencies': [
                '../net/net.gyp:hpack_fuzz_wrapper',
                '../net/net.gyp:dns_fuzz_stub',
                '../skia/skia.gyp:filter_fuzz_stub',
              ],
            }],
            ['enable_ipc_fuzzer==1 and component!="shared_library" and '
                 '(OS=="linux" or OS=="win" or OS=="mac")', {
              'dependencies': [
                '../tools/ipc_fuzzer/ipc_fuzzer.gyp:*',
              ],
            }],
            ['chromeos==0', {
              'dependencies': [
                '../v8/samples/samples.gyp:v8_shell#host',
                '../third_party/pdfium/samples/samples.gyp:pdfium_test',
              ],
            }],
            # TODO(thakis): Remove this block, nothing ever sets this.
            ['internal_filter_fuzzer==1', {
              'dependencies': [
                '../skia/tools/clusterfuzz-data/fuzzers/filter_fuzzer/filter_fuzzer.gyp:filter_fuzzer',
              ],
            }], # internal_filter_fuzzer
            ['clang==1', {
              'dependencies': [
                'sanitizers/sanitizers.gyp:llvm-symbolizer',
              ],
            }],
            ['OS=="win" and fastbuild==0 and target_arch=="ia32" and syzyasan==1', {
              'dependencies': [
                '../chrome/chrome_syzygy.gyp:chrome_dll_syzygy',
                '../content/content_shell_and_tests.gyp:content_shell_syzyasan',
              ],
              'conditions': [
                ['chrome_multiple_dll==1', {
                  'dependencies': [
                    '../chrome/chrome_syzygy.gyp:chrome_child_dll_syzygy',
                  ],
                }],
              ],
            }],
          ],
        },
        {
          'target_name': 'chromium_builder_nacl_sdk',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chrome',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '../chrome/chrome.gyp:chrome_nacl_win64',
              ]
            }],
          ],
        },  #target_name: chromium_builder_nacl_sdk
      ],  # targets
    }], #OS!=ios and OS!=android
    ['OS=="android"', {
      'targets': [
        {
          # The current list of tests for android.  This is temporary
          # until the full set supported.
          #
          # WARNING:
          # Do not add targets here without communicating the implications
          # on tryserver triggers and load.  Discuss with
          # chrome-infrastructure-team please.
          'target_name': 'android_builder_tests',
          'type': 'none',
          'dependencies': [
            '../base/android/jni_generator/jni_generator.gyp:jni_generator_tests',
            '../base/base.gyp:base_unittests',
            '../breakpad/breakpad.gyp:breakpad_unittests_deps',
            # Also compile the tools needed to deal with minidumps, they are
            # needed to run minidump tests upstream.
            '../breakpad/breakpad.gyp:dump_syms#host',
            '../breakpad/breakpad.gyp:symupload#host',
            '../breakpad/breakpad.gyp:minidump_dump#host',
            '../breakpad/breakpad.gyp:minidump_stackwalk#host',
            '../build/android/pylib/device/commands/commands.gyp:chromium_commands',
            '../cc/blink/cc_blink_tests.gyp:cc_blink_unittests',
            '../cc/cc_tests.gyp:cc_perftests_apk',
            '../cc/cc_tests.gyp:cc_unittests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_junit_tests',
            '../content/content_shell_and_tests.gyp:chromium_linker_test_apk',
            '../content/content_shell_and_tests.gyp:content_shell_test_apk',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../gpu/gpu.gyp:gl_tests',
            '../gpu/gpu.gyp:gpu_perftests_apk',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/capture/capture.gyp:capture_unittests',
            '../media/media.gyp:media_perftests_apk',
            '../media/media.gyp:media_unittests',
            '../media/midi/midi.gyp:midi_unittests_apk',
            '../media/midi/midi.gyp:midi_unittests',
            '../net/net.gyp:net_unittests',
            '../sandbox/sandbox.gyp:sandbox_linux_unittests_deps',
            '../skia/skia_tests.gyp:skia_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../testing/android/junit/junit_test.gyp:junit_unit_tests',
            '../third_party/leveldatabase/leveldatabase.gyp:env_chromium_unittests',
            '../third_party/WebKit/public/all.gyp:*',
            '../tools/android/android_tools.gyp:android_tools',
            '../tools/android/android_tools.gyp:memconsumer',
            '../tools/android/android_tools.gyp:push_apps_to_background',
            '../tools/android/findbugs_plugin/findbugs_plugin.gyp:findbugs_plugin_test',
            '../tools/cygprofile/cygprofile.gyp:cygprofile_unittests',
            '../ui/android/ui_android.gyp:ui_android_unittests',
            '../ui/base/ui_base_tests.gyp:ui_base_unittests',
            '../ui/events/events_unittests.gyp:events_unittests',
            '../ui/touch_selection/ui_touch_selection.gyp:ui_touch_selection_unittests',
            # Unit test bundles packaged as an apk.
            '../base/base.gyp:base_unittests_apk',
            '../cc/blink/cc_blink_tests.gyp:cc_blink_unittests_apk',
            '../cc/cc_tests.gyp:cc_unittests_apk',
            '../components/components_tests.gyp:components_browsertests_apk',
            '../components/components_tests.gyp:components_unittests_apk',
            '../content/content_shell_and_tests.gyp:content_browsertests_apk',
            '../content/content_shell_and_tests.gyp:content_unittests_apk',
            '../gpu/gpu.gyp:command_buffer_gles2_tests_apk',
            '../gpu/gpu.gyp:gl_tests_apk',
            '../gpu/gpu.gyp:gpu_unittests_apk',
            '../ipc/ipc.gyp:ipc_tests_apk',
            '../media/media.gyp:media_unittests_apk',
            '../media/media.gyp:video_decode_accelerator_unittest_apk',
            '../media/midi/midi.gyp:midi_unittests_apk',
            '../net/net.gyp:net_unittests_apk',
            '../skia/skia_tests.gyp:skia_unittests_apk',
            '../sql/sql.gyp:sql_unittests_apk',
            '../sync/sync.gyp:sync_unit_tests_apk',
            '../ui/android/ui_android.gyp:ui_android_unittests_apk',
            '../ui/android/ui_android.gyp:ui_junit_tests',
            '../ui/base/ui_base_tests.gyp:ui_base_unittests_apk',
            '../ui/events/events_unittests.gyp:events_unittests_apk',
            '../ui/gfx/gfx_tests.gyp:gfx_unittests_apk',
            '../ui/gl/gl_tests.gyp:gl_unittests_apk',
            '../ui/touch_selection/ui_touch_selection.gyp:ui_touch_selection_unittests_apk',
          ],
          'conditions': [
            ['chromecast==0', {
              'dependencies': [
                '../android_webview/android_webview.gyp:android_webview_unittests',
                '../chrome/chrome.gyp:unit_tests',
                # Unit test bundles packaged as an apk.
                '../android_webview/android_webview.gyp:android_webview_test_apk',
                '../android_webview/android_webview.gyp:android_webview_unittests_apk',
                '../android_webview/android_webview_shell.gyp:system_webview_shell_layout_test_apk',
                '../android_webview/android_webview_shell.gyp:system_webview_shell_page_cycler_apk',
                '../chrome/android/chrome_apk.gyp:chrome_public_test_apk',
                '../chrome/android/chrome_apk.gyp:chrome_sync_shell_test_apk',
                '../chrome/chrome.gyp:chrome_junit_tests',
                '../chrome/chrome.gyp:chromedriver_webview_shell_apk',
                '../chrome/chrome.gyp:unit_tests_apk',
                '../third_party/custom_tabs_client/custom_tabs_client.gyp:custom_tabs_client_example_apk',
              ],
            }],
          ],
        },
        {
          # WebRTC Chromium tests to run on Android.
          'target_name': 'android_builder_chromium_webrtc',
          'type': 'none',
          'dependencies': [
            '../build/android/pylib/device/commands/commands.gyp:chromium_commands',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../tools/android/android_tools.gyp:android_tools',
            '../tools/android/android_tools.gyp:memconsumer',
            '../content/content_shell_and_tests.gyp:content_browsertests_apk',
          ],
        },  # target_name: android_builder_chromium_webrtc
      ], # targets
    }], # OS="android"
    ['OS=="mac"', {
      'targets': [
        {
          # Target to build everything plus the dmg.  We don't put the dmg
          # in the All target because developers really don't need it.
          'target_name': 'all_and_dmg',
          'type': 'none',
          'dependencies': [
            'All',
            '../chrome/chrome.gyp:build_app_dmg',
          ],
        },
        # These targets are here so the build bots can use them to build
        # subsets of a full tree for faster cycle times.
        {
          'target_name': 'chromium_builder_dbg',
          'type': 'none',
          'dependencies': [
            '../cc/blink/cc_blink_tests.gyp:cc_blink_unittests',
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/capture/capture.gyp:capture_unittests',
            '../media/media.gyp:media_unittests',
            '../media/midi/midi.gyp:midi_unittests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../rlz/rlz.gyp:*',
            '../skia/skia_tests.gyp:skia_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/leveldatabase/leveldatabase.gyp:env_chromium_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../tools/perf/clear_system_cache/clear_system_cache.gyp:*',
            '../third_party/catapult/telemetry/telemetry.gyp:*',
            '../ui/base/ui_base_tests.gyp:ui_base_unittests',
            '../ui/gfx/gfx_tests.gyp:gfx_unittests',
            '../ui/gl/gl_tests.gyp:gl_unittests',
            '../url/url.gyp:url_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_rel',
          'type': 'none',
          'dependencies': [
            '../cc/blink/cc_blink_tests.gyp:cc_blink_unittests',
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/capture/capture.gyp:capture_unittests',
            '../media/media.gyp:media_unittests',
            '../media/midi/midi.gyp:midi_unittests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../skia/skia_tests.gyp:skia_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/leveldatabase/leveldatabase.gyp:env_chromium_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../tools/perf/clear_system_cache/clear_system_cache.gyp:*',
            '../third_party/catapult/telemetry/telemetry.gyp:*',
            '../ui/base/ui_base_tests.gyp:ui_base_unittests',
            '../ui/gfx/gfx_tests.gyp:gfx_unittests',
            '../ui/gl/gl_tests.gyp:gl_unittests',
            '../url/url.gyp:url_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_tsan_mac',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../crypto/crypto.gyp:crypto_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/capture/capture.gyp:capture_unittests',
            '../media/media.gyp:media_unittests',
            '../media/midi/midi.gyp:midi_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../url/url.gyp:url_unittests',
          ],
        },
     ],  # targets
    }], # OS="mac"
    ['OS=="win"', {
      'targets': [
        # These targets are here so the build bots can use them to build
        # subsets of a full tree for faster cycle times.
        {
          'target_name': 'chromium_builder',
          'type': 'none',
          'dependencies': [
            '../cc/blink/cc_blink_tests.gyp:cc_blink_unittests',
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:gcapi_test',
            '../chrome/chrome.gyp:installer_util_unittests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../chrome/chrome.gyp:setup_unittests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            # ../chrome/test/mini_installer requires mini_installer.
            '../chrome/installer/mini_installer.gyp:mini_installer',
            '../courgette/courgette.gyp:courgette_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/capture/capture.gyp:capture_unittests',
            '../media/media.gyp:media_unittests',
            '../media/midi/midi.gyp:midi_unittests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../skia/skia_tests.gyp:skia_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/leveldatabase/leveldatabase.gyp:env_chromium_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../tools/perf/clear_system_cache/clear_system_cache.gyp:*',
            '../third_party/catapult/telemetry/telemetry.gyp:*',
            '../ui/base/ui_base_tests.gyp:ui_base_unittests',
            '../ui/events/events_unittests.gyp:events_unittests',
            '../ui/gfx/gfx_tests.gyp:gfx_unittests',
            '../ui/gl/gl_tests.gyp:gl_unittests',
            '../ui/touch_selection/ui_touch_selection.gyp:ui_touch_selection_unittests',
            '../ui/views/views.gyp:views_unittests',
            '../url/url.gyp:url_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_tsan_win',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../crypto/crypto.gyp:crypto_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/capture/capture.gyp:capture_unittests',
            '../media/media.gyp:media_unittests',
            '../media/midi/midi.gyp:midi_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/leveldatabase/leveldatabase.gyp:env_chromium_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../url/url.gyp:url_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_lkgr_drmemory_win',
          'type': 'none',
          'dependencies': [
            '../components/test_runner/test_runner.gyp:layout_test_helper',
            '../content/content_shell_and_tests.gyp:content_shell',
            '../content/content_shell_and_tests.gyp:content_shell_crash_service',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_drmemory_win',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../cc/blink/cc_blink_tests.gyp:cc_blink_unittests',
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:chrome_app_unittests',
            '../chrome/chrome.gyp:chromedriver_unittests',
            '../chrome/chrome.gyp:installer_util_unittests',
            '../chrome/chrome.gyp:setup_unittests',
            '../chrome/chrome.gyp:unit_tests',
            '../chrome_elf/chrome_elf.gyp:chrome_elf_unittests',
            '../components/components_tests.gyp:components_unittests',
            '../components/test_runner/test_runner.gyp:layout_test_helper',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_shell',
            '../content/content_shell_and_tests.gyp:content_shell_crash_service',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../courgette/courgette.gyp:courgette_unittests',
            '../crypto/crypto.gyp:crypto_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../extensions/extensions_tests.gyp:extensions_browsertests',
            '../extensions/extensions_tests.gyp:extensions_unittests',
            '../gin/gin.gyp:gin_shell',
            '../gin/gin.gyp:gin_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../google_apis/google_apis.gyp:google_apis_unittests',
            '../gpu/gpu.gyp:angle_unittests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/capture/capture.gyp:capture_unittests',
            '../media/cast/cast.gyp:cast_unittests',
            '../media/media.gyp:media_unittests',
            '../media/midi/midi.gyp:midi_unittests',
            '../mojo/mojo.gyp:mojo',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../skia/skia_tests.gyp:skia_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/leveldatabase/leveldatabase.gyp:env_chromium_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../third_party/WebKit/Source/platform/blink_platform_tests.gyp:blink_heap_unittests',
            '../third_party/WebKit/Source/platform/blink_platform_tests.gyp:blink_platform_unittests',
            '../ui/accessibility/accessibility.gyp:accessibility_unittests',
            '../ui/aura/aura.gyp:aura_unittests',
            '../ui/compositor/compositor.gyp:compositor_unittests',
            '../ui/display/display.gyp:display_unittests',
            '../ui/events/events_unittests.gyp:events_unittests',
            '../ui/gfx/gfx_tests.gyp:gfx_unittests',
            '../ui/gl/gl_tests.gyp:gl_unittests',
            '../ui/keyboard/keyboard.gyp:keyboard_unittests',
            '../ui/touch_selection/ui_touch_selection.gyp:ui_touch_selection_unittests',
            '../url/url.gyp:url_unittests',
          ],
        },
        {
          'target_name': 'chrome_official_builder_no_unittests',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:gcapi_dll',
            '../chrome/chrome.gyp:pack_policy_templates',
            '../chrome/installer/mini_installer.gyp:mini_installer',
            '../courgette/courgette.gyp:courgette',
            '../courgette/courgette.gyp:courgette64',
            '../remoting/remoting.gyp:remoting_webapp',
            '../third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmadapter',
          ],
          'conditions': [
            ['component != "shared_library" and wix_exists == "True"', {
              # GN uses target_cpu==x86 && is_chrome_branded instead, and
              # so doesn't need the wix_exists check.
              'dependencies': [
                '../remoting/remoting.gyp:remoting_host_installation',
              ],
            }], # component != "shared_library"
          ]
        },
        {
          'target_name': 'chrome_official_builder',
          'type': 'none',
          'dependencies': [
            'chrome_official_builder_no_unittests',
            '../base/base.gyp:base_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/capture/capture.gyp:capture_unittests',
            '../media/media.gyp:media_unittests',
            '../media/midi/midi.gyp:midi_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../ui/base/ui_base_tests.gyp:ui_base_unittests',
            '../ui/gfx/gfx_tests.gyp:gfx_unittests',
            '../ui/gl/gl_tests.gyp:gl_unittests',
            '../ui/touch_selection/ui_touch_selection.gyp:ui_touch_selection_unittests',
            '../ui/views/views.gyp:views_unittests',
            '../url/url.gyp:url_unittests',
          ],
        },
      ], # targets
    }], # OS="win"
    ['chromeos==1', {
      'targets': [
        {
          'target_name': 'chromiumos_preflight',
          'type': 'none',
          'dependencies': [
            '../breakpad/breakpad.gyp:minidump_stackwalk',
            '../chrome/chrome.gyp:chrome',
            '../chrome/chrome.gyp:chromedriver',
            '../media/media.gyp:media_unittests',
            '../media/media.gyp:video_decode_accelerator_unittest',
            '../media/media.gyp:video_encode_accelerator_unittest',
            '../ppapi/ppapi_internal.gyp:ppapi_example_video_decode',
            '../sandbox/sandbox.gyp:chrome_sandbox',
            '../sandbox/sandbox.gyp:sandbox_linux_unittests',
            '../third_party/catapult/telemetry/telemetry.gyp:bitmaptools#host',
            '../third_party/mesa/mesa.gyp:osmesa',
            '../tools/perf/clear_system_cache/clear_system_cache.gyp:clear_system_cache',
          ],
          'conditions': [
            ['disable_nacl==0', {
              'dependencies': [
                '../components/nacl.gyp:nacl_helper',
                '../native_client/src/trusted/service_runtime/linux/nacl_bootstrap.gyp:nacl_helper_bootstrap',
              ],
            }],
          ],
        },
      ],  # targets
    }], # "chromeos==1"
    ['use_aura==1', {
      'targets': [
        {
          'target_name': 'aura_builder',
          'type': 'none',
          'dependencies': [
            '../ash/ash.gyp:ash_shell_with_content',
            '../ash/ash.gyp:ash_unittests',
            '../cc/blink/cc_blink_tests.gyp:cc_blink_unittests',
            '../cc/cc_tests.gyp:cc_unittests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../skia/skia_tests.gyp:skia_unittests',
            '../ui/app_list/app_list.gyp:*',
            '../ui/aura/aura.gyp:*',
            '../ui/aura_extra/aura_extra.gyp:*',
            '../ui/base/ui_base_tests.gyp:ui_base_unittests',
            '../ui/compositor/compositor.gyp:*',
            '../ui/display/display.gyp:display_unittests',
            '../ui/events/events.gyp:*',
            '../ui/gfx/gfx_tests.gyp:gfx_unittests',
            '../ui/gl/gl_tests.gyp:gl_unittests',
            '../ui/keyboard/keyboard.gyp:*',
            '../ui/snapshot/snapshot.gyp:snapshot_unittests',
            '../ui/touch_selection/ui_touch_selection.gyp:ui_touch_selection_unittests',
            '../ui/wm/wm.gyp:*',
            'blink_tests',
          ],
          'conditions': [
            ['OS=="linux"', {
              # Tests that currently only work on Linux.
              'dependencies': [
                '../base/base.gyp:base_unittests',
                '../ipc/ipc.gyp:ipc_tests',
                '../sql/sql.gyp:sql_unittests',
                '../sync/sync.gyp:sync_unit_tests',
              ],
            }],
            ['chromeos==1', {
              'dependencies': [
                '../chromeos/chromeos.gyp:chromeos_unittests',
                '../ui/chromeos/ui_chromeos.gyp:ui_chromeos_unittests',
              ],
            }],
            ['use_ozone==1', {
              'dependencies': [
                '../ui/ozone/ozone.gyp:*',
                '../ui/ozone/demo/ozone_demos.gyp:*',
              ],
            }],
            ['chromecast==0', {
              'dependencies': [
                '../chrome/chrome.gyp:browser_tests',
                '../chrome/chrome.gyp:chrome',
                '../chrome/chrome.gyp:interactive_ui_tests',
                '../chrome/chrome.gyp:unit_tests',
                '../ui/message_center/message_center.gyp:*',
                '../ui/views/examples/examples.gyp:views_examples_with_content_exe',
                '../ui/views/views.gyp:views',
                '../ui/views/views.gyp:views_unittests',
              ],
            }],
          ],
        },
      ],  # targets
    }], # "use_aura==1"
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'chromium_swarm_tests',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests_run',
            '../content/content_shell_and_tests.gyp:content_browsertests_run',
            '../content/content_shell_and_tests.gyp:content_unittests_run',
            '../net/net.gyp:net_unittests_run',
          ],
          'conditions': [
            ['chromecast==0', {
              'dependencies': [
                '../chrome/chrome.gyp:browser_tests_run',
                '../chrome/chrome.gyp:interactive_ui_tests_run',
                '../chrome/chrome.gyp:sync_integration_tests_run',
                '../chrome/chrome.gyp:unit_tests_run',
              ],
            }],
          ],
        }, # target_name: chromium_swarm_tests
      ],
    }],
    ['archive_chromoting_tests==1', {
      'targets': [
        {
          'target_name': 'chromoting_swarm_tests',
          'type': 'none',
          'dependencies': [
            '../testing/chromoting/integration_tests.gyp:*',
          ],
        }, # target_name: chromoting_swarm_tests
      ]
    }],
    ['archive_media_router_tests==1', {
      'targets': [
        {
          'target_name': 'media_router_swarming_tests',
          'type': 'none',
          'dependencies': [
            '../chrome/test/media_router/e2e_tests.gyp:media_router_e2e_tests_run',
          ],
        }, # target_name: media_router_swarming_tests
        {
          'target_name': 'media_router_swarming_perf_tests',
          'type': 'none',
          'dependencies': [
            '../chrome/test/media_router/e2e_tests.gyp:media_router_perf_tests_run',
          ],
        }, # target_name: media_router_swarming_perf_tests
      ]
    }],
    ['OS=="mac" and toolkit_views==1', {
      'targets': [
        {
          'target_name': 'macviews_builder',
          'type': 'none',
          'dependencies': [
            '../ui/views/examples/examples.gyp:views_examples_with_content_exe',
            '../ui/views/views.gyp:views',
            '../ui/views/views.gyp:views_unittests',
          ],
        },  # target_name: macviews_builder
      ],  # targets
    }],  # os=='mac' and toolkit_views==1
  ],  # conditions
}
