// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

tvcm.require('ui.spy');
tvcm.require('tvcm.ui');
tvcm.require('tvcm.ui.dom_helpers');
tvcm.requireTemplate('ui.spy_shell');

tvcm.exportTo('ui', function() {
  /**
   * @constructor
   */
  var SpyShell = tvcm.ui.define('x-spy-shell');

  SpyShell.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function(socketURL) {
      var node = tvcm.instantiateTemplate('#x-spy-shell-template');
      this.appendChild(node);

      this.socketURL_ = socketURL;
      this.conn_ = undefined;

      this.statusEl_ = this.querySelector('#status');
      this.statusEl_.textContent = 'Not connected';

      this.spy_ = this.querySelector('x-spy');
      tvcm.ui.decorate(this.spy_, ui.Spy);

      this.openConnection_();
    },

    get socketURL() {
      return this.socketURL_;
    },

    openConnection_: function() {
      if (!(this.conn_ == undefined ||
            this.conn_.readyState === undefined ||
            conn.readyState > 1)) {
        return;
      }

      this.conn_ = new WebSocket(this.socketURL_);
      this.conn_.onopen = function() {
        this.statusEl_.textContent = 'connected at ' + this.socketURL_;
        this.spy_.connection = this.conn_;
      }.bind(this);

      this.conn_.onclose = function(event) {
        this.statusEl_.textContent = 'connection closed';
        this.spy_.connection = undefined;
      }.bind(this);
      this.conn_.onerror = function(event) {
        this.statusEl_.innerHTML = 'got error';
      }.bind(this);
    }

  };

  return {
    SpyShell: SpyShell
  };
});
