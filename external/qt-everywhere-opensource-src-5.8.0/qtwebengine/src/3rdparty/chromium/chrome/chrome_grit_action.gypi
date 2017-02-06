# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # Wrapper around build/grit_action.gypi that applies Chrome-specific grit
    # settings. Actions that include this gypi file at their local scopes can
    # define variables with the same names as the ones below and they will be
    # merged.
    'grit_additional_defines': [ '<@(chrome_grit_defines)' ],
  },
  'includes': [ '../build/grit_action.gypi', 'chrome_features.gypi' ],
}
