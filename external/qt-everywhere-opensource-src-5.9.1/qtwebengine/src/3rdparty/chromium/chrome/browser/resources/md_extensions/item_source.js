// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Closure compiler won't let this be declared inside cr.define().
/** @enum {string} */
var SourceType = {
  WEBSTORE: 'webstore',
  POLICY: 'policy',
  SIDELOADED: 'sideloaded',
  UNPACKED: 'unpacked',
};

cr.define('extensions', function() {
  /**
   * @param {chrome.developerPrivate.ExtensionInfo} item
   * @return {SourceType}
   */
  function getItemSource(item) {
    if (item.controlledInfo &&
        item.controlledInfo.type ==
            chrome.developerPrivate.ControllerType.POLICY) {
      return SourceType.POLICY;
    }
    if (item.location == chrome.developerPrivate.Location.THIRD_PARTY)
      return SourceType.SIDELOADED;
    if (item.location == chrome.developerPrivate.Location.UNPACKED)
      return SourceType.UNPACKED;
    return SourceType.WEBSTORE;
  }

  /**
   * @param {SourceType} source
   * @return {string}
   */
  function getItemSourceString(source) {
    switch (source) {
      case SourceType.POLICY:
        return loadTimeData.getString('itemSourcePolicy');
      case SourceType.SIDELOADED:
        return loadTimeData.getString('itemSourceSideloaded');
      case SourceType.UNPACKED:
        return loadTimeData.getString('itemSourceUnpacked');
      case SourceType.WEBSTORE:
        return loadTimeData.getString('itemSourceWebstore');
    }
    assertNotReached();
  }

  return {getItemSource: getItemSource,
          getItemSourceString: getItemSourceString};
});
