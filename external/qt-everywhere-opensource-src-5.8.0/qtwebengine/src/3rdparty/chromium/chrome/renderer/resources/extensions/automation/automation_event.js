// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var utils = require('utils');

function AutomationEventImpl(type, target) {
  this.propagationStopped = false;

  // TODO(aboxhall): make these read-only properties
  this.type = type;
  this.target = target;
  this.eventPhase = Event.NONE;
}

AutomationEventImpl.prototype = {
  __proto__: null,
  stopPropagation: function() {
    this.propagationStopped = true;
  },
};

function AutomationEvent() {
  privates(AutomationEvent).constructPrivate(this, arguments);
}
utils.expose(AutomationEvent, AutomationEventImpl, {
  functions: [
    'stopPropagation',
  ],
  readonly: [
    'type',
    'target',
    'eventPhase',
  ],
});

exports.$set('AutomationEvent', AutomationEvent);
