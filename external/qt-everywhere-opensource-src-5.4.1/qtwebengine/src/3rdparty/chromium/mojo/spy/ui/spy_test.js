// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

tvcm.require('ui.spy');
tvcm.require('tvcm.event_target');

tvcm.unittest.testSuite('ui.spy_test', function() {
  /**
   * @constructor
   */
  function FakeChannel() {
    tvcm.EventTarget.call(this);
  }

  FakeChannel.prototype = {
    __proto__: tvcm.EventTarget.prototype,

    send: function(msg) {
    },

    dispatchMessage: function(msg) {
      var event = new Event('message', false, false);
      event.data = msg;
      this.dispatchEvent(event);
    }
  };

  test('basic', function() {
    var channel = new FakeChannel();

    var spy = new ui.Spy();
    spy.style.width = '600px';
    spy.style.height = '400px';
    spy.style.border = '1px solid black';
    this.addHTMLOutput(spy);
    spy.channel = channel;

    channel.dispatchMessage({data: 'alo there'});

    // Fake out echo reply
    channel.send = function(msg) {
      setTimeout(function() {
        channel.dispatchMessage({data: {type: 'reply', msg: msg}});
      }, 10);
    }
  });

});
