// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // HandlerOptions class:

  /**
   * Encapsulated handling of handler options page.
   * @constructor
   */
  function HandlerOptions() {
    this.activeNavTab = null;
    OptionsPage.call(this,
                     'handlers',
                     loadTimeData.getString('handlersPageTabTitle'),
                     'handler-options');
  }

  cr.addSingletonGetter(HandlerOptions);

  HandlerOptions.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * The handlers list.
     * @type {options.HandlersList}
     * @private
     */
    handlersList_: null,

    /** @override */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.createHandlersList_();

      $('handler-options-overlay-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);
    },

    /**
     * Creates, decorates and initializes the handlers list.
     * @private
     */
    createHandlersList_: function() {
      this.handlersList_ = $('handlers-list');
      options.HandlersList.decorate(this.handlersList_);
      this.handlersList_.autoExpands = true;

      this.ignoredHandlersList_ = $('ignored-handlers-list');
      options.IgnoredHandlersList.decorate(this.ignoredHandlersList_);
      this.ignoredHandlersList_.autoExpands = true;
    },
  };

  /**
   * Sets the list of handlers shown by the view.
   * @param {Array} Handlers to be shown in the view.
   */
  HandlerOptions.setHandlers = function(handlers) {
    $('handlers-list').setHandlers(handlers);
  };

  /**
   * Sets the list of ignored handlers shown by the view.
   * @param {Array} Handlers to be shown in the view.
   */
  HandlerOptions.setIgnoredHandlers = function(handlers) {
    $('ignored-handlers-section').hidden = handlers.length == 0;
    $('ignored-handlers-list').setHandlers(handlers);
  };

  return {
    HandlerOptions: HandlerOptions
  };
});
