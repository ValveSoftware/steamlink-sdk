// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var elems = document.querySelectorAll(
      'meta[property="og:type"],meta[name="og:type"]');
  for (var i in elems) {
    if (elems[i].content && elems[i].content.toUpperCase() == 'ARTICLE') {
      return true;
    }
  }
  var elems = document.querySelectorAll(
      '*[itemtype="http://schema.org/Article"]');
  for (var i in elems) {
    if (elems[i].itemscope) {
      return true;
    }
  }
  return false;
})()
