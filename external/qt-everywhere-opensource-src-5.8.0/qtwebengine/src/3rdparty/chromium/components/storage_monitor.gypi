# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/storage_monitor
      'target_name': 'storage_monitor',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'storage_monitor/image_capture_device.h',
        'storage_monitor/image_capture_device.mm',
        'storage_monitor/image_capture_device_manager.h',
        'storage_monitor/image_capture_device_manager.mm',
        'storage_monitor/media_storage_util.cc',
        'storage_monitor/media_storage_util.h',
        'storage_monitor/media_transfer_protocol_device_observer_linux.cc',
        'storage_monitor/media_transfer_protocol_device_observer_linux.h',
        'storage_monitor/mtab_watcher_linux.cc',
        'storage_monitor/mtab_watcher_linux.h',
        'storage_monitor/portable_device_watcher_win.cc',
        'storage_monitor/portable_device_watcher_win.h',
        'storage_monitor/removable_device_constants.cc',
        'storage_monitor/removable_device_constants.h',
        'storage_monitor/removable_storage_observer.h',
        'storage_monitor/storage_info.cc',
        'storage_monitor/storage_info.h',
        'storage_monitor/storage_monitor.cc',
        'storage_monitor/storage_monitor.h',
        'storage_monitor/storage_monitor_chromeos.cc',
        'storage_monitor/storage_monitor_chromeos.h',
        'storage_monitor/storage_monitor_linux.cc',
        'storage_monitor/storage_monitor_linux.h',
        'storage_monitor/storage_monitor_mac.h',
        'storage_monitor/storage_monitor_mac.mm',
        'storage_monitor/storage_monitor_win.cc',
        'storage_monitor/storage_monitor_win.h',
        'storage_monitor/transient_device_ids.cc',
        'storage_monitor/transient_device_ids.h',
        'storage_monitor/udev_util_linux.cc',
        'storage_monitor/udev_util_linux.h',
        'storage_monitor/volume_mount_watcher_win.cc',
        'storage_monitor/volume_mount_watcher_win.h',
      ],
      'conditions': [
        ['OS == "mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/DiskArbitration.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/ImageCaptureCore.framework',
            ],
          },
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../device/media_transfer_protocol/media_transfer_protocol.gyp:device_media_transfer_protocol',
            '../device/media_transfer_protocol/media_transfer_protocol.gyp:mtp_file_entry_proto',
            '../device/media_transfer_protocol/media_transfer_protocol.gyp:mtp_storage_info_proto',
          ],
        }],
        ['use_udev==1', {
          'dependencies': [
            '../device/udev_linux/udev.gyp:udev_linux',
          ],
        }, {  # use_udev==0
          'sources!': [
            'storage_monitor/storage_monitor_linux.cc',
            'storage_monitor/storage_monitor_linux.h',
            'storage_monitor/udev_util_linux.cc',
            'storage_monitor/udev_util_linux.h',
          ],
        }],
        ['chromeos==1', {
          'sources!': [
            'storage_monitor/mtab_watcher_linux.cc',
            'storage_monitor/mtab_watcher_linux.h',
            'storage_monitor/storage_monitor_linux.cc',
            'storage_monitor/storage_monitor_linux.h',
          ],
        }],
      ],
    },
    {
      # GN version: //components/storage_monitor:test_support
      'target_name': 'storage_monitor_test_support',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'storage_monitor',
      ],
      'sources': [
        'storage_monitor/mock_removable_storage_observer.cc',
        'storage_monitor/mock_removable_storage_observer.h',
        'storage_monitor/test_media_transfer_protocol_manager_linux.cc',
        'storage_monitor/test_media_transfer_protocol_manager_linux.h',
        'storage_monitor/test_portable_device_watcher_win.cc',
        'storage_monitor/test_portable_device_watcher_win.h',
        'storage_monitor/test_storage_monitor.cc',
        'storage_monitor/test_storage_monitor.h',
        'storage_monitor/test_storage_monitor_win.cc',
        'storage_monitor/test_storage_monitor_win.h',
        'storage_monitor/test_volume_mount_watcher_win.cc',
        'storage_monitor/test_volume_mount_watcher_win.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'dependencies': [
            '../device/media_transfer_protocol/media_transfer_protocol.gyp:device_media_transfer_protocol',
            '../device/media_transfer_protocol/media_transfer_protocol.gyp:mtp_file_entry_proto',
            '../device/media_transfer_protocol/media_transfer_protocol.gyp:mtp_storage_info_proto',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../testing/gtest.gyp:gtest',
          ],
        }],
      ],
    },
  ],
}
