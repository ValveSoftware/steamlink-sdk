// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mocks for globals needed for loading utility.js.

var localStorage = {};

chrome['alarms'] = {
  get: emptyMock
};
chrome['identity'] = {
  getAuthToken: emptyMock,
  removeCachedAuthToken: emptyMock
};
chrome['storage'] = {
  local: {
    get: emptyMock
  }
};
chrome['webstorePrivate'] = {
  getBrowserLogin: emptyMock
};

mockChromeEvent(chrome, 'alarms.onAlarm');
mockChromeEvent(chrome, 'identity.onSignInChanged');
mockChromeEvent(chrome, 'runtime.onSuspend');
