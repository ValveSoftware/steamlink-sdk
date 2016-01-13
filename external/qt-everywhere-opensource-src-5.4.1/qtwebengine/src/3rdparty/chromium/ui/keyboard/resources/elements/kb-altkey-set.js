// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('kb-altkey-set', {
  offset: 0,
  out: function(e) {
    e.stopPropagation();
  }
});
