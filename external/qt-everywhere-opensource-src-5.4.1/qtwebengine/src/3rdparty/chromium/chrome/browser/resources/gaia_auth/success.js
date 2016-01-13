// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function load() {
  var params = getUrlSearchParams(location.search);
  var msg = {
    'method': 'confirmLogin',
    'attemptToken': params['attemptToken']
  };
  window.parent.postMessage(msg,
          'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/main.html');
}

document.addEventListener('DOMContentLoaded', load);

