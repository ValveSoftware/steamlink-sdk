// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Modal dialog for print destination's advanced settings.
   * @param {!print_preview.PrintTicketStore} printTicketStore Contains the
   *     print ticket to print.
   * @constructor
   * @extends {print_preview.Overlay}
   */
  function AdvancedSettings(printTicketStore) {
    print_preview.Overlay.call(this);

    /**
     * Contains the print ticket to print.
     * @private {!print_preview.PrintTicketStore}
     */
    this.printTicketStore_ = printTicketStore;

    /**
     * Used to record usage statistics.
     * @private {!print_preview.PrintSettingsUiMetricsContext}
     */
    this.metrics_ = new print_preview.PrintSettingsUiMetricsContext();

    /** @private {!print_preview.SearchBox} */
    this.searchBox_ = new print_preview.SearchBox(
        loadTimeData.getString('advancedSettingsSearchBoxPlaceholder'));
    this.addChild(this.searchBox_);

    /** @private {print_preview.Destination} */
    this.destination_ = null;

    /** @private {!Array<!print_preview.AdvancedSettingsItem>} */
    this.items_ = [];
  };

  /**
   * CSS classes used by the component.
   * @enum {string}
   * @private
   */
  AdvancedSettings.Classes_ = {
    EXTRA_PADDING: 'advanced-settings-item-extra-padding'
  };

  AdvancedSettings.prototype = {
    __proto__: print_preview.Overlay.prototype,

    /**
     * @param {!print_preview.Destination} destination Destination to show
     *     advanced settings for.
     */
    showForDestination: function(destination) {
      assert(!this.destination_);
      this.destination_ = destination;
      this.getChildElement('.advanced-settings-title').textContent =
          loadTimeData.getStringF('advancedSettingsDialogTitle',
                                  this.destination_.displayName);
      this.setIsVisible(true);
      this.renderSettings_();
    },

    /** @override */
    enterDocument: function() {
      print_preview.Overlay.prototype.enterDocument.call(this);

      this.tracker.add(
          this.getChildElement('.button-strip .cancel-button'),
          'click',
          this.cancel.bind(this));

      this.tracker.add(
          this.getChildElement('.button-strip .done-button'),
          'click',
          this.onApplySettings_.bind(this));

      this.tracker.add(
          this.searchBox_,
          print_preview.SearchBox.EventType.SEARCH,
          this.onSearch_.bind(this));
    },

    /** @override */
    decorateInternal: function() {
      this.searchBox_.render(this.getChildElement('.search-box-area'));
    },

    /** @override */
    onSetVisibleInternal: function(isVisible) {
      if (isVisible) {
        this.searchBox_.focus();
        this.metrics_.record(print_preview.Metrics.PrintSettingsUiBucket.
            ADVANCED_SETTINGS_DIALOG_SHOWN);
      } else {
        this.resetSearch_();
        this.destination_ = null;
      }
    },

    /** @override */
    onCancelInternal: function() {
      this.metrics_.record(print_preview.Metrics.PrintSettingsUiBucket.
          ADVANCED_SETTINGS_DIALOG_CANCELED);
    },

    /** @override */
    onEnterPressedInternal: function() {
      var doneButton = this.getChildElement('.button-strip .done-button');
      if (!doneButton.disabled)
        doneButton.click();
      return !doneButton.disabled;
    },

    /**
     * @return {number} Height available for settings, in pixels.
     * @private
     */
    getAvailableContentHeight_: function() {
      var elStyle = window.getComputedStyle(this.getElement());
      return this.getElement().offsetHeight -
          parseInt(elStyle.getPropertyValue('padding-top'), 10) -
          parseInt(elStyle.getPropertyValue('padding-bottom'), 10) -
          this.getChildElement('.settings-area').offsetTop -
          this.getChildElement('.action-area').offsetHeight;
    },

    /**
     * Filters displayed settings with the given query.
     * @param {?string} query Query to filter settings by.
     * @private
     */
    filterLists_: function(query) {
      var atLeastOneMatch = false;
      var lastVisibleItemWithBubble = null;
      this.items_.forEach(function(item) {
        item.updateSearchQuery(query);
        if (getIsVisible(item.getElement()))
          atLeastOneMatch = true;
        if (item.searchBubbleShown)
          lastVisibleItemWithBubble = item;
      });
      setIsVisible(
          this.getChildElement('.no-settings-match-hint'), !atLeastOneMatch);
      setIsVisible(
          this.getChildElement('.' + AdvancedSettings.Classes_.EXTRA_PADDING),
          !!lastVisibleItemWithBubble);
    },

    /**
     * Resets the filter query.
     * @private
     */
    resetSearch_: function() {
      this.searchBox_.setQuery(null);
      this.filterLists_(null);
    },

    /**
     * Renders all of the available settings.
     * @private
     */
    renderSettings_: function() {
      // Remove all children settings elements.
      this.items_.forEach(function(item) {
        this.removeChild(item);
      }.bind(this));
      this.items_ = [];

      var extraPadding =
          this.getChildElement('.' + AdvancedSettings.Classes_.EXTRA_PADDING);
      if (extraPadding)
        extraPadding.parentNode.removeChild(extraPadding);

      var vendorCapabilities = this.printTicketStore_.vendorItems.capability;
      if (!vendorCapabilities)
        return;

      var availableHeight = this.getAvailableContentHeight_();
      var containerEl = this.getChildElement('.settings-area');
      containerEl.style.maxHeight = availableHeight + 'px';
      var settingsEl = this.getChildElement('.settings');

      vendorCapabilities.forEach(function(capability) {
        var item = new print_preview.AdvancedSettingsItem(
            this.eventTarget_, this.printTicketStore_, capability);
        this.addChild(item);
        item.render(settingsEl);
        this.items_.push(item);
      }.bind(this));

      if (this.items_.length <= 1) {
        setIsVisible(this.getChildElement('.search-box-area'), false);
      } else {
        setIsVisible(this.getChildElement('.search-box-area'), true);
        this.searchBox_.focus();
      }

      extraPadding = document.createElement('div');
      extraPadding.classList.add(AdvancedSettings.Classes_.EXTRA_PADDING);
      extraPadding.hidden = true;
      settingsEl.appendChild(extraPadding);
    },

    /**
     * Called when settings search query changes. Filters displayed settings
     * with the given query.
     * @param {Event} evt Contains search query.
     * @private
     */
    onSearch_: function(evt) {
      this.filterLists_(evt.queryRegExp);
    },

    /**
     * Called when current settings selection need to be stored in the ticket.
     * @private
     */
    onApplySettings_: function(evt) {
      this.setIsVisible(false);

      var values = {};
      this.items_.forEach(function(item) {
        if (item.isModified())
          values[item.id] = item.selectedValue;
      }.bind(this));

      this.printTicketStore_.vendorItems.updateValue(values);
    }
  };

  // Export
  return {
    AdvancedSettings: AdvancedSettings
  };
});
