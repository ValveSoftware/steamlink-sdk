# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [{
    'target_name': 'x11_events_platform',
    'type': '<(component)',
    'defines': [
      'EVENTS_IMPLEMENTATION',
    ],
    'dependencies': [
      '../../../../build/linux/system.gyp:x11',
      '../../../gfx/x/gfx_x11.gyp:gfx_x11',
      '../../events.gyp:events',
      '../events_platform.gyp:events_platform',
    ],
    'sources': [
      'x11_event_source.cc',
      'x11_event_source.h',
      'x11_event_source_glib.cc',
      'x11_event_source_libevent.cc',
    ],
    'conditions': [
      ['use_glib==1', {
        'dependencies': [
          '../../../../build/linux/system.gyp:glib',
        ],
        'sources!': [
          'x11_event_source_libevent.cc',
        ],
      }, {
        # use_glib == 0
        'sources!': [
          'x11_event_source_glib.cc',
        ],
      }],
    ],
  }],
}
