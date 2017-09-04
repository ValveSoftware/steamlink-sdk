// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'history-router',

  properties: {
    selectedPage: {
      type: String,
      observer: 'serializePath_',
      notify: true,
    },

    queryState: {
      type: Object,
      notify: true
    },

    path_: {
      type: String,
      observer: 'pathChanged_'
    },

    queryParams_: Object,
  },

  observers: [
    'queryParamsChanged_(queryParams_.*)',
    'searchTermChanged_(queryState.searchTerm)',
  ],

  /** @override */
  attached: function() {
    // Redirect legacy search URLs to URLs compatible with material history.
    if (window.location.hash) {
      window.location.href = window.location.href.split('#')[0] + '?' +
          window.location.hash.substr(1);
    }
  },

  /** @private */
  serializePath_: function() {
    var page = this.selectedPage == 'history' ? '' : this.selectedPage;
    this.path_ = '/' + page;
  },

  /** @private */
  pathChanged_: function() {
    var sections = this.path_.substr(1).split('/');
    this.selectedPage = sections[0] || 'history';
  },

  /** @private */
  queryParamsChanged_: function() {
    this.set('queryState.searchTerm', this.queryParams_.q || '');
  },

  /** @private */
  searchTermChanged_: function() {
    this.set('queryParams_.q', this.queryState.searchTerm || null);
  },
});
