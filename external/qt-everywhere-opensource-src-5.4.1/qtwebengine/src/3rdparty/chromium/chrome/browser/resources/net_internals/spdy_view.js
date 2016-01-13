// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays a summary of the state of each SPDY sessions, and
 * has links to display them in the events tab.
 */
var SpdyView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function SpdyView() {
    assertFirstConstructorCall(SpdyView);

    // Call superclass's constructor.
    superClass.call(this, SpdyView.MAIN_BOX_ID);

    g_browser.addSpdySessionInfoObserver(this, true);
    g_browser.addSpdyStatusObserver(this, true);
    g_browser.addSpdyAlternateProtocolMappingsObserver(this, true);
  }

  SpdyView.TAB_ID = 'tab-handle-spdy';
  SpdyView.TAB_NAME = 'SPDY';
  SpdyView.TAB_HASH = '#spdy';

  // IDs for special HTML elements in spdy_view.html
  SpdyView.MAIN_BOX_ID = 'spdy-view-tab-content';
  SpdyView.STATUS_ID = 'spdy-view-status';
  SpdyView.SESSION_INFO_ID = 'spdy-view-session-info';
  SpdyView.ALTERNATE_PROTOCOL_MAPPINGS_ID =
      'spdy-view-alternate-protocol-mappings';

  cr.addSingletonGetter(SpdyView);

  SpdyView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onLoadLogFinish: function(data) {
      return this.onSpdySessionInfoChanged(data.spdySessionInfo) &&
             this.onSpdyStatusChanged(data.spdyStatus) &&
             this.onSpdyAlternateProtocolMappingsChanged(
                 data.spdyAlternateProtocolMappings);
    },

    /**
     * If |spdySessionInfo| contains any sessions, displays a single table with
     * information on each SPDY session.  Otherwise, displays "None".
     */
    onSpdySessionInfoChanged: function(spdySessionInfo) {
      if (!spdySessionInfo)
        return false;
      var input = new JsEvalContext({ spdySessionInfo: spdySessionInfo });
      jstProcess(input, $(SpdyView.SESSION_INFO_ID));
      return true;
    },

    /**
     * Displays information on the global SPDY status.
     */
    onSpdyStatusChanged: function(spdyStatus) {
      if (!spdyStatus)
        return false;
      var input = new JsEvalContext(spdyStatus);
      jstProcess(input, $(SpdyView.STATUS_ID));
      return true;
    },

    /**
     * Displays information on the SPDY alternate protocol mappings.
     */
    onSpdyAlternateProtocolMappingsChanged: function(
        spdyAlternateProtocolMappings) {
      if (!spdyAlternateProtocolMappings)
        return false;
      var input = new JsEvalContext(
          {spdyAlternateProtocolMappings: spdyAlternateProtocolMappings});
      jstProcess(input, $(SpdyView.ALTERNATE_PROTOCOL_MAPPINGS_ID));
      return true;
    }
  };

  return SpdyView;
})();
