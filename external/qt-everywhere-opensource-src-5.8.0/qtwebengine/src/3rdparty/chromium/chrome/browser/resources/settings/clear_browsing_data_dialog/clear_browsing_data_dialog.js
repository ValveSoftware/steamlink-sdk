// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-clear-browsing-data-dialog' allows the user to delete
 * browsing data that has been cached by Chromium.
 */
Polymer({
  is: 'settings-clear-browsing-data-dialog',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Results of browsing data counters, keyed by the suffix of
     * the corresponding data type deletion preference, as reported
     * by the C++ side.
     * @private {!Object<string>}
     */
    counters_: {
      type: Object,
      value: {
        // Will be filled as results are reported.
      }
    },

    /**
     * List of options for the dropdown menu.
     * @private {!DropdownMenuOptionList}
     */
    clearFromOptions_: {
      readOnly: true,
      type: Array,
      value: [
        {value: 0, name: loadTimeData.getString('clearDataHour')},
        {value: 1, name: loadTimeData.getString('clearDataDay')},
        {value: 2, name: loadTimeData.getString('clearDataWeek')},
        {value: 3, name: loadTimeData.getString('clearData4Weeks')},
        {value: 4, name: loadTimeData.getString('clearDataEverything')},
      ],
    },

    /** @private */
    clearingInProgress_: Boolean,

    /** @private */
    showHistoryDeletionDialog_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {settings.ClearBrowsingDataBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.$.clearFrom.menuOptions = this.clearFromOptions_;
    this.addWebUIListener(
        'browsing-history-pref-changed',
        this.setAllowDeletingHistory_.bind(this));
    this.addWebUIListener(
        'update-footer',
        this.updateFooter_.bind(this));
    this.addWebUIListener(
        'update-counter-text',
        this.updateCounterText_.bind(this));
    this.browserProxy_ =
        settings.ClearBrowsingDataBrowserProxyImpl.getInstance();
    this.browserProxy_.initialize();
    this.$.dialog.open();
  },

  /**
   * @param {boolean} allowed Whether the user is allowed to delete histories.
   * @private
   */
  setAllowDeletingHistory_: function(allowed) {
    this.$.browsingCheckbox.disabled = !allowed;
    this.$.downloadCheckbox.disabled = !allowed;
    if (!allowed) {
      this.set('prefs.browser.clear_data.browsing_history.value', false);
      this.set('prefs.browser.clear_data.download_history.value', false);
    }
  },

  /**
   * Updates the footer to show only those sentences that are relevant to this
   * user.
   * @param {boolean} syncing Whether the user is syncing data.
   * @param {boolean} otherFormsOfBrowsingHistory Whether the user has other
   *     forms of browsing history in their account.
   * @private
   */
  updateFooter_: function(syncing, otherFormsOfBrowsingHistory) {
    this.$.googleFooter.hidden = !otherFormsOfBrowsingHistory;
    this.$.syncedDataSentence.hidden = !syncing;
    this.$.dialog.notifyResize();
    this.$.dialog.classList.add('fully-rendered');
  },

  /**
   * Updates the text of a browsing data counter corresponding to the given
   * preference.
   * @param {string} prefName Browsing data type deletion preference.
   * @param {string} text The text with which to update the counter
   * @private
   */
  updateCounterText_: function(prefName, text) {
    // Data type deletion preferences are named "browser.clear_data.<datatype>".
    // Strip the common prefix, i.e. use only "<datatype>".
    var matches = prefName.match(/^browser\.clear_data\.(\w+)$/);
    this.set('counters_.' + assert(matches[1]), text);
  },

  open: function() {
    this.$.dialog.open();
  },

  /**
   * Handles the tap on the Clear Data button.
   * @private
   */
  onClearBrowsingDataTap_: function() {
    this.clearingInProgress_ = true;
    this.browserProxy_.clearBrowsingData().then(
      /**
       * @param {boolean} shouldShowNotice Whether we should show the notice
       *     about other forms of browsing history before closing the dialog.
       */
      function(shouldShowNotice) {
        this.clearingInProgress_ = false;
        this.showHistoryDeletionDialog_ = shouldShowNotice;

        if (!shouldShowNotice)
          this.$.dialog.close();
      }.bind(this));
  },

  /**
   * Handles the closing of the notice about other forms of browsing history.
   * @private
   */
  onHistoryDeletionDialogClose_: function() {
    this.showHistoryDeletionDialog_ = false;
    this.$.dialog.close();
  }
});
