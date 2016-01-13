// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('kb-key-import', {
  /**
  * The id of the document fragment that will be imported.
   */
  importId: null,

  /**
   * Import content from a document fragment.
   * @param {!DocumentFragment} content Document fragment that contains
   *     the content to import.
   */
  importDoc: function(content) {
    var id = this.getAttribute('importId');
    var fragment = content.querySelector('#' + id);
    return fragment && fragment.content ? fragment.content : fragment;
  }
});

