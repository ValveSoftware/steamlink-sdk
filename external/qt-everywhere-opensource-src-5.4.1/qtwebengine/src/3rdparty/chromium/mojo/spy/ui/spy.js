// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

tvcm.require('tvcm.utils');
tvcm.require('tvcm.ui');
tvcm.requireTemplate('ui.spy');

tvcm.exportTo('ui', function() {
  /**
   * @constructor
   */
  var LogMessage = tvcm.ui.define('x-spy-log-message');

  LogMessage.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function() {
    },

    get message() {
      return message_;
    },

    set message(message) {
      this.message_ = message;
      this.textContent = JSON.stringify(message);
    }
  };


  /**
   * @constructor
   */
  var Spy = tvcm.ui.define('x-spy');

  Spy.prototype = {
    __proto__: HTMLUnknownElement.prototype,

    decorate: function() {
      var node = tvcm.instantiateTemplate('#x-spy-template');
      this.appendChild(node);

      this.channel_ = undefined;
      this.onMessage_ = this.onMessage_.bind(this);

      var commandEl = this.querySelector('#command');
      commandEl.addEventListener('keydown', function(e) {
        if (e.keyCode == 13) {
          e.stopPropagation();
          this.onCommandEntered_();
        }
      }.bind(this));

      this.updateDisabledStates_();
    },

    get channel() {
      return channel_;
    },

    set channel(channel) {
      if (this.channel_)
        this.channel_.removeEventListener('message', this.onMessage_);
      this.channel_ = channel;
      if (this.channel_)
        this.channel_.addEventListener('message', this.onMessage_);
      this.updateDisabledStates_();
    },

    updateDisabledStates_: function() {
      var connected = this.channel_ !== undefined;

      this.querySelector('#command').disabled = !connected;
    },

    onCommandEntered_: function(cmd) {
      var commandEl = this.querySelector('#command');
      this.channel_.send(JSON.stringify(commandEl.value));
      commandEl.value = '';
    },

    onMessage_: function(message) {
      var messageEl = new LogMessage();
      messageEl.message = message.data;
      this.querySelector('messages').appendChild(messageEl);
    }

  };

  return {
    Spy: Spy
  };
});
