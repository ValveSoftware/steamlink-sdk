# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [{
    'target_name': 'events_ozone',
    'type': '<(component)',
    'dependencies': [
      '../../../base/base.gyp:base',
    ],
    'defines': [
      'EVENTS_OZONE_IMPLEMENTATION',
    ],
    'sources': [
      'device/device_event.cc',
      'device/device_event.h',
      'device/device_event_observer.h',
      'device/device_manager.cc',
      'device/device_manager.h',
      'device/device_manager_manual.cc',
      'device/device_manager_manual.h',
      'device/udev/device_manager_udev.cc',
      'device/udev/device_manager_udev.h',
      'events_ozone_export.h',
    ],
    'conditions': [
      ['use_udev==0', {
        'sources/': [
          ['exclude', '_udev\\.(h|cc)$'],
        ],
      }],
      ['use_ozone_evdev==1 and use_udev==1', {
        'dependencies': [
          '<(DEPTH)/device/udev_linux/udev.gyp:udev_linux',
        ],
      }],
    ],
  }, {
    'target_name': 'events_ozone_evdev',
    'type': '<(component)',
    'dependencies': [
      '../../../base/base.gyp:base',
      '../../gfx/gfx.gyp:gfx',
      '../../ozone/ozone.gyp:ozone_base',
      '../platform/events_platform.gyp:events_platform',
      'events_ozone',
    ],
    'defines': [
      'EVENTS_OZONE_EVDEV_IMPLEMENTATION',
    ],
    'sources': [
      'evdev/libgestures_glue/event_reader_libevdev_cros.cc',
      'evdev/libgestures_glue/event_reader_libevdev_cros.h',
      'evdev/libgestures_glue/gesture_interpreter_libevdev_cros.cc',
      'evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h',
      'evdev/libgestures_glue/gesture_logging.cc',
      'evdev/libgestures_glue/gesture_logging.h',
      'evdev/libgestures_glue/gesture_timer_provider.cc',
      'evdev/libgestures_glue/gesture_timer_provider.h',
      'evdev/event_converter_evdev.cc',
      'evdev/event_converter_evdev.h',
      'evdev/event_device_info.cc',
      'evdev/event_device_info.h',
      'evdev/event_factory_evdev.cc',
      'evdev/event_factory_evdev.h',
      'evdev/event_modifiers_evdev.cc',
      'evdev/event_modifiers_evdev.h',
      'evdev/events_ozone_evdev_export.h',
      'evdev/key_event_converter_evdev.cc',
      'evdev/key_event_converter_evdev.h',
      'evdev/touch_event_converter_evdev.cc',
      'evdev/touch_event_converter_evdev.h',
    ],
    'conditions': [
      ['use_ozone_evdev==1 and use_evdev_gestures==1', {
        'dependencies': [
          '<(DEPTH)/build/linux/system.gyp:libgestures',
          '<(DEPTH)/build/linux/system.gyp:libevdev-cros',
        ],
        'defines': [
          'USE_EVDEV_GESTURES',
        ],
      }, {
        'sources/': [
          ['exclude', '^evdev/libgestures_glue/'],
        ],
      }],
      ['use_ozone_evdev==1', {
        'defines': ['USE_OZONE_EVDEV=1'],
      }],
    ],
  }]
}
