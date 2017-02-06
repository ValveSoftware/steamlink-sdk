// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-engine-dialog' is a component for adding
 * or editing a search engine entry.
 */
Polymer({
  is: 'settings-search-engine-dialog',

  properties: {
    /**
     * The search engine to be edited. If not populated a new search engine
     * should be added.
     * @type {?SearchEngine}
     */
    model: Object,

    /** @private {string} */
    searchEngine_: String,

    /** @private {string} */
    keyword_: String,

    /** @private {string} */
    queryUrl_: String,

    /** @private {string} */
    dialogTitle_: String,

    /** @private {string} */
    actionButtonText_: String,
  },

  /** @private {settings.SearchEnginesBrowserProxy} */
  browserProxy_: null,

  /**
   * The |modelIndex| to use when a new search engine is added. Must match with
   * kNewSearchEngineIndex constant specified at
   * chrome/browser/ui/webui/settings/search_engines_handler.cc
   * @const {number}
   */
  DEFAULT_MODEL_INDEX: -1,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.SearchEnginesBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    if (this.model) {
      this.dialogTitle_ =
          loadTimeData.getString('searchEnginesEditSearchEngine');
      this.actionButtonText_ = loadTimeData.getString('save');

      // If editing an existing search engine, pre-populate the input fields.
      this.searchEngine_ = this.model.displayName;
      this.keyword_ = this.model.keyword;
      this.queryUrl_ = this.model.url;
    } else {
      this.dialogTitle_ =
          loadTimeData.getString('searchEnginesAddSearchEngine');
      this.actionButtonText_ = loadTimeData.getString('add');
    }

    this.addEventListener('iron-overlay-canceled', function() {
      this.browserProxy_.searchEngineEditCancelled();
    }.bind(this));
  },

  /** @override */
  attached: function() {
    this.updateActionButtonState_();
    this.browserProxy_.searchEngineEditStarted(
        this.model ? this.model.modelIndex : this.DEFAULT_MODEL_INDEX);
    this.$.dialog.open();
  },

  /** @private */
  cancel_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onActionButtonTap_: function() {
    this.browserProxy_.searchEngineEditCompleted(
        this.searchEngine_, this.keyword_, this.queryUrl_);
    this.$.dialog.close();
  },

  /**
   * @param {!Event} event
   * @private
   */
  validate_: function(event) {
    var inputElement = Polymer.dom(event).localTarget;

    this.browserProxy_.validateSearchEngineInput(
        inputElement.id, inputElement.value).then(function(isValid) {
      inputElement.invalid = !isValid;
      this.updateActionButtonState_();
    }.bind(this));
  },

  /** @private */
  updateActionButtonState_: function() {
    var allValid = [
      this.$.searchEngine, this.$.keyword, this.$.queryUrl
    ].every(function(inputElement) {
      return !inputElement.invalid && inputElement.value.length != 0;
    });
    this.$.actionButton.disabled = !allValid;
  },
});
