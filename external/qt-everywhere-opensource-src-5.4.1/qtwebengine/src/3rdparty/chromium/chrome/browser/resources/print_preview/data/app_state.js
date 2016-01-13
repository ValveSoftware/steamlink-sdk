// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Object used to get and persist the print preview application state.
   * @constructor
   */
  function AppState() {
    /**
     * Internal representation of application state.
     * @type {Object.<string: Object>}
     * @private
     */
    this.state_ = {};
    this.state_[AppState.Field.VERSION] = AppState.VERSION_;
    this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED] = true;

    /**
     * Whether the app state has been initialized. The app state will ignore all
     * writes until it has been initialized.
     * @type {boolean}
     * @private
     */
    this.isInitialized_ = false;
  };

  /**
   * Enumeration of field names for serialized app state.
   * @enum {string}
   */
  AppState.Field = {
    VERSION: 'version',
    SELECTED_DESTINATION_ID: 'selectedDestinationId',
    SELECTED_DESTINATION_ACCOUNT: 'selectedDestinationAccount',
    SELECTED_DESTINATION_ORIGIN: 'selectedDestinationOrigin',
    SELECTED_DESTINATION_CAPABILITIES: 'selectedDestinationCapabilities',
    SELECTED_DESTINATION_NAME: 'selectedDestinationName',
    IS_GCP_PROMO_DISMISSED: 'isGcpPromoDismissed',
    MEDIA_SIZE: 'mediaSize',
    MARGINS_TYPE: 'marginsType',
    CUSTOM_MARGINS: 'customMargins',
    IS_COLOR_ENABLED: 'isColorEnabled',
    IS_DUPLEX_ENABLED: 'isDuplexEnabled',
    IS_HEADER_FOOTER_ENABLED: 'isHeaderFooterEnabled',
    IS_LANDSCAPE_ENABLED: 'isLandscapeEnabled',
    IS_COLLATE_ENABLED: 'isCollateEnabled',
    IS_CSS_BACKGROUND_ENABLED: 'isCssBackgroundEnabled'
  };

  /**
   * Current version of the app state. This value helps to understand how to
   * parse earlier versions of the app state.
   * @type {number}
   * @const
   * @private
   */
  AppState.VERSION_ = 2;

  /**
   * Name of C++ layer function to persist app state.
   * @type {string}
   * @const
   * @private
   */
  AppState.NATIVE_FUNCTION_NAME_ = 'saveAppState';

  AppState.prototype = {
    /** @return {?string} ID of the selected destination. */
    get selectedDestinationId() {
      return this.state_[AppState.Field.SELECTED_DESTINATION_ID];
    },

    /** @return {?string} Account the selected destination is registered for. */
    get selectedDestinationAccount() {
      return this.state_[AppState.Field.SELECTED_DESTINATION_ACCOUNT];
    },

    /** @return {?string} Origin of the selected destination. */
    get selectedDestinationOrigin() {
      return this.state_[AppState.Field.SELECTED_DESTINATION_ORIGIN];
    },

    /** @return {?print_preview.Cdd} CDD of the selected destination. */
    get selectedDestinationCapabilities() {
      return this.state_[AppState.Field.SELECTED_DESTINATION_CAPABILITIES];
    },

    /** @return {?string} Name of the selected destination. */
    get selectedDestinationName() {
      return this.state_[AppState.Field.SELECTED_DESTINATION_NAME];
    },

    /** @return {boolean} Whether the GCP promotion has been dismissed. */
    get isGcpPromoDismissed() {
      return this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED];
    },

    /**
     * @param {!print_preview.AppState.Field} field App state field to check if
     *     set.
     * @return {boolean} Whether a field has been set in the app state.
     */
    hasField: function(field) {
      return this.state_.hasOwnProperty(field);
    },

    /**
     * @param {!print_preview.AppState.Field} field App state field to get.
     * @return {Object} Value of the app state field.
     */
    getField: function(field) {
      if (field == AppState.Field.CUSTOM_MARGINS) {
        return this.state_[field] ?
            print_preview.Margins.parse(this.state_[field]) : null;
      } else {
        return this.state_[field];
      }
    },

    /**
     * Initializes the app state from a serialized string returned by the native
     * layer.
     * @param {?string} serializedAppStateStr Serialized string representation
     *     of the app state.
     * @param {?string} systemDefaultDestinationId ID of the system default
     *     destination.
     */
    init: function(serializedAppStateStr, systemDefaultDestinationId) {
      if (serializedAppStateStr) {
        var state = JSON.parse(serializedAppStateStr);
        if (state[AppState.Field.VERSION] == AppState.VERSION_) {
          this.state_ = state;
        }
      } else {
        // Set some state defaults.
        this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED] = false;
      }
      // Default to system destination, if no destination was selected.
      if (!this.state_[AppState.Field.SELECTED_DESTINATION_ID] ||
          !this.state_[AppState.Field.SELECTED_DESTINATION_ORIGIN]) {
        if (systemDefaultDestinationId) {
          this.state_[AppState.Field.SELECTED_DESTINATION_ID] =
              systemDefaultDestinationId;
          this.state_[AppState.Field.SELECTED_DESTINATION_ORIGIN] =
              print_preview.Destination.Origin.LOCAL;
          this.state_[AppState.Field.SELECTED_DESTINATION_ACCOUNT] = '';
        }
      }
    },

    /**
     * Sets to initialized state. Now object will accept persist requests.
     */
    setInitialized: function() {
      this.isInitialized_ = true;
    },

    /**
     * Persists the given value for the given field.
     * @param {!print_preview.AppState.Field} field Field to persist.
     * @param {Object} value Value of field to persist.
     */
    persistField: function(field, value) {
      if (!this.isInitialized_)
        return;
      if (field == AppState.Field.CUSTOM_MARGINS) {
        this.state_[field] = value ? value.serialize() : null;
      } else {
        this.state_[field] = value;
      }
      this.persist_();
    },

    /**
     * Persists the selected destination.
     * @param {!print_preview.Destination} dest Destination to persist.
     */
    persistSelectedDestination: function(dest) {
      if (!this.isInitialized_)
        return;
      this.state_[AppState.Field.SELECTED_DESTINATION_ID] = dest.id;
      this.state_[AppState.Field.SELECTED_DESTINATION_ACCOUNT] = dest.account;
      this.state_[AppState.Field.SELECTED_DESTINATION_ORIGIN] = dest.origin;
      this.state_[AppState.Field.SELECTED_DESTINATION_CAPABILITIES] =
          dest.capabilities;
      this.state_[AppState.Field.SELECTED_DESTINATION_NAME] = dest.displayName;
      this.persist_();
    },

    /**
     * Persists whether the GCP promotion has been dismissed.
     * @param {boolean} isGcpPromoDismissed Whether the GCP promotion has been
     *     dismissed.
     */
    persistIsGcpPromoDismissed: function(isGcpPromoDismissed) {
      if (!this.isInitialized_)
        return;
      this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED] = isGcpPromoDismissed;
      this.persist_();
    },

    /**
     * Calls into the native layer to persist the application state.
     * @private
     */
    persist_: function() {
      chrome.send(AppState.NATIVE_FUNCTION_NAME_,
                  [JSON.stringify(this.state_)]);
    }
  };

  return {
    AppState: AppState
  };
});
