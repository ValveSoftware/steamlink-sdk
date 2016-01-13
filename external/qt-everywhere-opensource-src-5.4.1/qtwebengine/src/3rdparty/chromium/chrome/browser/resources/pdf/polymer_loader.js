// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
// This is necessary because polymer currently doesn't handle the configuration
// where native Custom Elements are available but native Shadow DOM is not.
// TODO(bshe): remove this line when polymer supports the configuration.
document.registerElement = undefined;

<include src="../../../../third_party/polymer/platform/platform.js"></include>
})();
