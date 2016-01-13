# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(dnicoara) Move this into its own executable. (Currently it is built as
# part of ui_unittests.)
{
  'dependencies': [
    '../base/base.gyp:test_support_base',
    '../testing/gtest.gyp:gtest',
    '../ui/display/display.gyp:display_util',
    '../ui/gfx/gfx.gyp:gfx_geometry',
  ],
  'sources': [
    'chromeos/display_configurator_unittest.cc',
    'chromeos/touchscreen_delegate_impl_unittest.cc',
    'chromeos/x11/display_util_x11_unittest.cc',
    'chromeos/x11/native_display_event_dispatcher_x11_unittest.cc',
    'util/display_util_unittest.cc',
    'util/edid_parser_unittest.cc',
  ],
  'conditions': [
    # TODO(dnicoara) When we add non-chromeos display code this dependency can
    # be added to the above list. For now, keep it here since Win & Android do
    # not like empty libraries.
    ['chromeos == 1', {
      'dependencies': [
        '../ui/display/display.gyp:display',
        '../ui/display/display.gyp:display_test_util',
        '../ui/display/display.gyp:display_types',
      ],
    }],
  ],
}
