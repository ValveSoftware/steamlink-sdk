// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *     default_handler: number,
 *     handlers: Array,
 *     has_policy_recommendations: boolean,
 *     is_default_handler_set_by_user: boolean,
 *     protocol: string
 * }}
 * @see chrome/browser/ui/webui/options/handler_options_handler.cc
 */
var Handlers;

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /////////////////////////////////////////////////////////////////////////////
  // HandlerOptions class:

  /**
   * Encapsulated handling of handler options page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function HandlerOptions() {
    this.activeNavTab = null;
    Page.call(this,
              'handlers',
              loadTimeData.getString('handlersPageTabTitle'),
              'handler-options');
  }

  cr.addSingletonGetter(HandlerOptions);

  HandlerOptions.prototype = {
    __proto__: Page.prototype,

    /**
     * The handlers list.
     * @type {options.HandlersList}
     * @private
     */
    handlersList_: null,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      this.createHandlersList_();

      $('handler-options-overlay-confirm').onclick =
          PageManager.closeOverlay.bind(PageManager);
    },

    /**
     * Creates, decorates and initializes the handlers list.
     * @private
     */
    createHandlersList_: function() {
      var handlersList = $('handlers-list');
      options.HandlersList.decorate(handlersList);
      this.handlersList_ = assertInstanceof(handlersList, options.HandlersList);
      this.handlersList_.autoExpands = true;

      var ignoredHandlersList = $('ignored-handlers-list');
      options.IgnoredHandlersList.decorate(ignoredHandlersList);
      this.ignoredHandlersList_ = assertInstanceof(ignoredHandlersList,
          options.IgnoredHandlersList);
      this.ignoredHandlersList_.autoExpands = true;
    },
  };

  /**
   * Sets the list of handlers shown by the view.
   * @param {Array<Handlers>} handlers Handlers to be shown in the view.
   */
  HandlerOptions.setHandlers = function(handlers) {
    $('handlers-list').setHandlers(handlers);
  };

  /**
   * Sets the list of ignored handlers shown by the view.
   * @param {Array} handlers Handlers to be shown in the view.
   */
  HandlerOptions.setIgnoredHandlers = function(handlers) {
    $('ignored-handlers-section').hidden = handlers.length == 0;
    $('ignored-handlers-list').setHandlers(handlers);
  };

  return {
    HandlerOptions: HandlerOptions
  };
});
