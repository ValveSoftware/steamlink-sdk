# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {

    # This is a place for defines which will hold out Skia changes.
    #
    # When Skia needs to make a change which will affect Chromium, a define may
    # be added here, the Skia change made behind this define, then Skia rolled
    # into Chromium. At this point the Chromium side change can be made while
    # also removing the define. After this Skia can remove the code behind the
    # now no longer needed define.
    #
    # Examples of these sorts of changes are Skia API changes and Skia changes
    # which will affect blink baselines.
    #
    # The intent is that the defines here are transient. Every effort should be
    # made to remove these defines as soon as practical. This is in contrast to
    # defines in SkUserConfig.h which are normally more permanent.
    'chromium_skia_defines': [
      'SK_IGNORE_DW_GRAY_FIX'
    ],
  },
}
