// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/** @interface */
settings.DirectionDelegate = function() {};

settings.DirectionDelegate.prototype = {
  /**
   * @return {boolean} Whether the direction of the settings UI is
   *     right-to-left.
   */
  isRtl: assertNotReached,
};

/**
 * @implements {settings.DirectionDelegate}
 * @constructor
 */
settings.DirectionDelegateImpl = function() {};

settings.DirectionDelegateImpl.prototype = {
  /** @override */
  isRtl: function() {
    return loadTimeData.getString('textdirection') == 'rtl';
  },
};
