# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/arc
      'target_name': 'arc',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'arc_base',
        'arc_mojo_bindings',
        'components.gyp:exo',
        'components.gyp:onc_component',
        '../base/base.gyp:base',
        '../chromeos/chromeos.gyp:chromeos',
        '../chromeos/chromeos.gyp:power_manager_proto',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth',
        '../ipc/ipc.gyp:ipc',
        '../third_party/re2/re2.gyp:re2',
        '../skia/skia.gyp:skia',
        '../ui/arc/arc.gyp:arc',
        '../ui/aura/aura.gyp:aura',
        '../ui/base/ime/ui_base_ime.gyp:ui_base_ime',
        '../ui/base/ui_base.gyp:ui_base',
        '../ui/base/ui_base.gyp:ui_base_test_support',
        '../ui/events/events.gyp:events_base',
        '../ui/keyboard/keyboard.gyp:keyboard',
        '../url/url.gyp:url_lib',
      ],
      'sources': [
        'arc/arc_bridge_bootstrap.cc',
        'arc/arc_bridge_bootstrap.h',
        'arc/arc_bridge_service_impl.cc',
        'arc/arc_bridge_service_impl.h',
        'arc/arc_service_manager.cc',
        'arc/arc_service_manager.h',
        'arc/audio/arc_audio_bridge.cc',
        'arc/audio/arc_audio_bridge.h',
        'arc/bluetooth/arc_bluetooth_bridge.cc',
        'arc/bluetooth/arc_bluetooth_bridge.h',
        'arc/bluetooth/bluetooth_type_converters.cc',
        'arc/bluetooth/bluetooth_type_converters.h',
        'arc/clipboard/arc_clipboard_bridge.cc',
        'arc/clipboard/arc_clipboard_bridge.h',
        'arc/crash_collector/arc_crash_collector_bridge.cc',
        'arc/crash_collector/arc_crash_collector_bridge.h',
        'arc/ime/arc_ime_bridge.h',
        'arc/ime/arc_ime_bridge_impl.cc',
        'arc/ime/arc_ime_bridge_impl.h',
        'arc/ime/arc_ime_service.cc',
        'arc/ime/arc_ime_service.h',
        'arc/instance_holder.h',
        'arc/intent_helper/activity_icon_loader.cc',
        'arc/intent_helper/activity_icon_loader.h',
        'arc/intent_helper/arc_intent_helper_bridge.cc',
        'arc/intent_helper/arc_intent_helper_bridge.h',
        'arc/intent_helper/font_size_util.cc',
        'arc/intent_helper/font_size_util.h',
        'arc/intent_helper/intent_filter.cc',
        'arc/intent_helper/intent_filter.h',
        'arc/intent_helper/link_handler_model_impl.cc',
        'arc/intent_helper/link_handler_model_impl.h',
        'arc/intent_helper/local_activity_resolver.cc',
        'arc/intent_helper/local_activity_resolver.h',
        'arc/metrics/arc_metrics_service.cc',
        'arc/metrics/arc_metrics_service.h',
        'arc/metrics/oom_kills_histogram.h',
        'arc/metrics/oom_kills_monitor.cc',
        'arc/metrics/oom_kills_monitor.h',
        'arc/net/arc_net_host_impl.cc',
        'arc/net/arc_net_host_impl.h',
        'arc/obb_mounter/arc_obb_mounter_bridge.cc',
        'arc/obb_mounter/arc_obb_mounter_bridge.h',
        'arc/power/arc_power_bridge.cc',
        'arc/power/arc_power_bridge.h',
        'arc/set_wallpaper_delegate.h',
        'arc/storage_manager/arc_storage_manager.cc',
        'arc/storage_manager/arc_storage_manager.h',
        'arc/user_data/arc_user_data_service.cc',
        'arc/user_data/arc_user_data_service.h',
        'arc/window_manager/arc_window_manager_bridge.cc',
        'arc/window_manager/arc_window_manager_bridge.h',
      ],
    },
    {
      # GN version: //components/arc:arc_base
      'target_name': 'arc_base',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../chromeos/chromeos.gyp:chromeos',
      ],
      'sources': [
        'arc/arc_bridge_service.cc',
        'arc/arc_bridge_service.h',
        'arc/arc_service.cc',
        'arc/arc_service.h',
      ],
    },
    {
      # GN version: //components/arc:arc_bitmap
      'target_name': 'arc_bitmap',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'arc_mojo_bindings',
        '../skia/skia.gyp:skia',
      ],
      'sources': [
        'arc/bitmap/bitmap_type_converters.cc',
        'arc/bitmap/bitmap_type_converters.h',
      ],
    },
    {
      # GN version: //components/arc_test_support
      'target_name': 'arc_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'arc',
        'arc_mojo_bindings',
      ],
      'sources': [
        'arc/test/fake_app_instance.cc',
        'arc/test/fake_app_instance.h',
        'arc/test/fake_arc_bridge_bootstrap.cc',
        'arc/test/fake_arc_bridge_bootstrap.h',
        'arc/test/fake_arc_bridge_instance.cc',
        'arc/test/fake_arc_bridge_instance.h',
        'arc/test/fake_arc_bridge_service.cc',
        'arc/test/fake_arc_bridge_service.h',
        'arc/test/fake_notifications_instance.cc',
        'arc/test/fake_notifications_instance.h',
        'arc/test/fake_policy_instance.cc',
        'arc/test/fake_policy_instance.h',
      ],
    },
    {
      # GN version: //components/arc:mojo_bindings
      'target_name': 'arc_mojo_bindings',
      'type': 'static_library',
      'variables': {
        'mojom_typemaps': [
          'arc/common/app.typemap',
          'arc/common/bluetooth.typemap',
          '../mojo/common/common_custom_types.typemap',
        ],
      },
      'sources': [
        'arc/common/app.mojom',
        'arc/common/arc_bridge.mojom',
        'arc/common/audio.mojom',
        'arc/common/auth.mojom',
        'arc/common/bitmap.mojom',
        'arc/common/bluetooth.mojom',
        'arc/common/clipboard.mojom',
        'arc/common/crash_collector.mojom',
        'arc/common/enterprise_reporting.mojom',
        'arc/common/file_system.mojom',
        'arc/common/ime.mojom',
        'arc/common/intent_helper.mojom',
        'arc/common/metrics.mojom',
        'arc/common/net.mojom',
        'arc/common/notifications.mojom',
        'arc/common/obb_mounter.mojom',
        'arc/common/policy.mojom',
        'arc/common/power.mojom',
        'arc/common/process.mojom',
        'arc/common/scale_factor.mojom',
        'arc/common/screen_rect.mojom',
        'arc/common/storage_manager.mojom',
        'arc/common/video.mojom',
        'arc/common/video_accelerator.mojom',
        'arc/common/window_manager.mojom',
        'arc/common/app_struct_traits.cc',
      ],
      'includes': [ '../mojo/mojom_bindings_generator.gypi' ],
      'dependencies': [
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../mojo/mojo_base.gyp:mojo_common_custom_types_mojom',
      ],
    },
    {
      # GN version: //components/arc:arc_standalone_service
      'target_name': 'arc_standalone_service',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
        '../mojo/mojo_edk.gyp:mojo_system_impl',
      ],
      'sources': [
        'arc/standalone/service_helper.cc',
        'arc/standalone/service_helper.h',
      ],
    },
    {
      # GN version: //components/arc:arc_standalone
      'target_name': 'arc_standalone',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'arc',
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
        '../mojo/mojo_edk.gyp:mojo_system_impl',
      ],
      'sources': [
        'arc/standalone/arc_standalone_bridge_runner.cc',
        'arc/standalone/arc_standalone_bridge_runner.h',
      ]
    },
    {
      # GN version: //components/arc:arc_standalone_bridge
      'target_name': 'arc_standalone_bridge',
      'type': 'executable',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'arc_standalone',
        'arc_standalone_service',
        '../base/base.gyp:base',
        '../ipc/ipc.gyp:ipc',
        '../mojo/mojo_edk.gyp:mojo_system_impl',
      ],
      'sources': [
        'arc/standalone/arc_standalone_bridge_main.cc',
      ]
    }
  ],
}
