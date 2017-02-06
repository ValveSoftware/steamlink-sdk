// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  // TODO(rltoscano): This class needs a throbber while loading the destination
  // or another solution is persist the settings of the printer so that next
  // load is fast.

  /**
   * Component used to render the print destination.
   * @param {!print_preview.DestinationStore} destinationStore Used to determine
   *     the selected destination.
   * @constructor
   * @extends {print_preview.SettingsSection}
   */
  function DestinationSettings(destinationStore) {
    print_preview.SettingsSection.call(this);

    /**
     * Used to determine the selected destination.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * Current CSS class of the destination icon.
     * @type {?DestinationSettings.Classes_}
     * @private
     */
    this.iconClass_ = null;
  };

  /**
   * Event types dispatched by the component.
   * @enum {string}
   */
  DestinationSettings.EventType = {
    CHANGE_BUTTON_ACTIVATE:
        'print_preview.DestinationSettings.CHANGE_BUTTON_ACTIVATE'
  };

  /**
   * CSS classes used by the component.
   * @enum {string}
   * @private
   */
  DestinationSettings.Classes_ = {
    CHANGE_BUTTON: 'destination-settings-change-button',
    ICON: 'destination-settings-icon',
    ICON_CLOUD: 'destination-settings-icon-cloud',
    ICON_CLOUD_SHARED: 'destination-settings-icon-cloud-shared',
    ICON_GOOGLE_PROMOTED: 'destination-settings-icon-google-promoted',
    ICON_LOCAL: 'destination-settings-icon-local',
    ICON_MOBILE: 'destination-settings-icon-mobile',
    ICON_MOBILE_SHARED: 'destination-settings-icon-mobile-shared',
    LOCATION: 'destination-settings-location',
    NAME: 'destination-settings-name',
    STALE: 'stale',
    THOBBER_NAME: 'destination-throbber-name'
  };

  DestinationSettings.prototype = {
    __proto__: print_preview.SettingsSection.prototype,

    /** @override */
    isAvailable: function() {
      return true;
    },

    /** @override */
    hasCollapsibleContent: function() {
      return false;
    },

    /** @override */
    set isEnabled(isEnabled) {
      var changeButton = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.CHANGE_BUTTON)[0];
      changeButton.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.SettingsSection.prototype.enterDocument.call(this);
      var changeButton = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.CHANGE_BUTTON)[0];
      this.tracker.add(
          changeButton, 'click', this.onChangeButtonClick_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationSelect_.bind(this));
      this.tracker_.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.
              CACHED_SELECTED_DESTINATION_INFO_READY,
          this.onSelectedDestinationNameSet_.bind(this));
    },

    /**
     * Called when the "Change" button is clicked. Dispatches the
     * CHANGE_BUTTON_ACTIVATE event.
     * @private
     */
    onChangeButtonClick_: function() {
      cr.dispatchSimpleEvent(
          this, DestinationSettings.EventType.CHANGE_BUTTON_ACTIVATE);
    },

    /**
     * Called when the destination selection has changed. Updates UI elements.
     * @private
     */
    onDestinationSelect_: function() {
      var destinationSettingsBoxEl =
          this.getChildElement('.destination-settings-box');

      var destination = this.destinationStore_.selectedDestination;
      if (destination != null) {
        var nameEl = this.getElement().getElementsByClassName(
            DestinationSettings.Classes_.NAME)[0];
        nameEl.textContent = destination.displayName;
        nameEl.title = destination.displayName;

        var iconEl = this.getElement().getElementsByClassName(
            DestinationSettings.Classes_.ICON)[0];
        iconEl.src = destination.iconUrl;

        var hint = destination.hint;
        var locationEl = this.getElement().getElementsByClassName(
            DestinationSettings.Classes_.LOCATION)[0];
        locationEl.textContent = hint;
        locationEl.title = hint;

        var offlineStatusText = destination.offlineStatusText;
        var offlineStatusEl =
            this.getChildElement('.destination-settings-offline-status');
        offlineStatusEl.textContent = offlineStatusText;
        offlineStatusEl.title = offlineStatusText;

        var isOffline = destination.isOffline;
        destinationSettingsBoxEl.classList.toggle(
            DestinationSettings.Classes_.STALE, isOffline);
        setIsVisible(locationEl, !isOffline);
        setIsVisible(offlineStatusEl, isOffline);
      }

      setIsVisible(
          this.getChildElement('.throbber-container'),
          this.destinationStore_.isAutoSelectDestinationInProgress);
      setIsVisible(destinationSettingsBoxEl, !!destination);
    },

    onSelectedDestinationNameSet_: function() {
      var destinationName =
          this.destinationStore_.selectedDestination.displayName;
      var nameEl = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.THOBBER_NAME)[0];
      nameEl.textContent = destinationName;
      nameEl.title = destinationName;
    }
  };

  // Export
  return {
    DestinationSettings: DestinationSettings
  };
});
