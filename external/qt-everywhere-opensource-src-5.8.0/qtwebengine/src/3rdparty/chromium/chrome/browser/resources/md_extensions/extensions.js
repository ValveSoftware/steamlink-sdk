// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';
  var manager = /** @type {extensions.Manager} */(
                    document.querySelector('extensions-manager'));
  manager.readyPromiseResolver.promise.then(function() {
    extensions.Service.getInstance().managerReady(manager);
  });
})();
