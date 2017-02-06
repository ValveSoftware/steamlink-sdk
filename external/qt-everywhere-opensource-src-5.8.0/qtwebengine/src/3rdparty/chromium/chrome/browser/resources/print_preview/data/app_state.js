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
     * @type {Object}
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
    SELECTED_DESTINATION_EXTENSION_ID: 'selectedDestinationExtensionId',
    SELECTED_DESTINATION_EXTENSION_NAME: 'selectedDestinationExtensionName',
    IS_GCP_PROMO_DISMISSED: 'isGcpPromoDismissed',
    DPI: 'dpi',
    MEDIA_SIZE: 'mediaSize',
    MARGINS_TYPE: 'marginsType',
    CUSTOM_MARGINS: 'customMargins',
    IS_COLOR_ENABLED: 'isColorEnabled',
    IS_DUPLEX_ENABLED: 'isDuplexEnabled',
    IS_HEADER_FOOTER_ENABLED: 'isHeaderFooterEnabled',
    IS_LANDSCAPE_ENABLED: 'isLandscapeEnabled',
    IS_COLLATE_ENABLED: 'isCollateEnabled',
    IS_FIT_TO_PAGE_ENABLED: 'isFitToPageEnabled',
    IS_CSS_BACKGROUND_ENABLED: 'isCssBackgroundEnabled',
    VENDOR_OPTIONS: 'vendorOptions'
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

    /**
     * @return {?print_preview.Destination.Origin<string>} Origin of the
     *     selected destination.
     */
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

    /**
     * @return {?string} Extension ID associated with the selected destination.
     */
    get selectedDestinationExtensionId() {
      return this.state_[AppState.Field.SELECTED_DESTINATION_EXTENSION_ID];
    },

    /**
     * @return {?string} Extension name associated with the selected
     *     destination.
     */
    get selectedDestinationExtensionName() {
      return this.state_[AppState.Field.SELECTED_DESTINATION_EXTENSION_NAME];
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
     * @return {?} Value of the app state field.
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
     */
    init: function(serializedAppStateStr) {
      if (serializedAppStateStr) {
        try {
          var state = JSON.parse(serializedAppStateStr);
          if (state[AppState.Field.VERSION] == AppState.VERSION_) {
            this.state_ = state;
          }
        } catch(e) {
          console.error('Unable to parse state: ' + e);
          // Proceed with default state.
        }
      } else {
        // Set some state defaults.
        this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED] = false;
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
     * @param {?} value Value of field to persist.
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
      this.state_[AppState.Field.SELECTED_DESTINATION_EXTENSION_ID] =
          dest.extensionId;
      this.state_[AppState.Field.SELECTED_DESTINATION_EXTENSION_NAME] =
          dest.extensionName;
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
