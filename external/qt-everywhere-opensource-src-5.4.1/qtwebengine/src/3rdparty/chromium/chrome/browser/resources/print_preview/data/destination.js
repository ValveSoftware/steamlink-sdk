// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Print destination data object that holds data for both local and cloud
   * destinations.
   * @param {string} id ID of the destination.
   * @param {!print_preview.Destination.Type} type Type of the destination.
   * @param {!print_preview.Destination.Origin} origin Origin of the
   *     destination.
   * @param {string} displayName Display name of the destination.
   * @param {boolean} isRecent Whether the destination has been used recently.
   * @param {!print_preview.Destination.ConnectionStatus} connectionStatus
   *     Connection status of the print destination.
   * @param {{tags: Array.<string>,
   *          isOwned: ?boolean,
   *          account: ?string,
   *          lastAccessTime: ?number,
   *          isTosAccepted: ?boolean,
   *          cloudID: ?string,
   *          description: ?string}=} opt_params Optional parameters for the
   *     destination.
   * @constructor
   */
  function Destination(id, type, origin, displayName, isRecent,
                       connectionStatus, opt_params) {
    /**
     * ID of the destination.
     * @private {string}
     */
    this.id_ = id;

    /**
     * Type of the destination.
     * @private {!print_preview.Destination.Type}
     */
    this.type_ = type;

    /**
     * Origin of the destination.
     * @private {!print_preview.Destination.Origin}
     */
    this.origin_ = origin;

    /**
     * Display name of the destination.
     * @private {string}
     */
    this.displayName_ = displayName || '';

    /**
     * Whether the destination has been used recently.
     * @private {boolean}
     */
    this.isRecent_ = isRecent;

    /**
     * Tags associated with the destination.
     * @private {!Array.<string>}
     */
    this.tags_ = (opt_params && opt_params.tags) || [];

    /**
     * Print capabilities of the destination.
     * @private {print_preview.Cdd}
     */
    this.capabilities_ = null;

    /**
     * Whether the destination is owned by the user.
     * @private {boolean}
     */
    this.isOwned_ = (opt_params && opt_params.isOwned) || false;

    /**
     * Account this destination is registered for, if known.
     * @private {string}
     */
    this.account_ = (opt_params && opt_params.account) || '';

    /**
     * Cache of destination location fetched from tags.
     * @private {?string}
     */
    this.location_ = null;

    /**
     * Printer description.
     * @private {string}
     */
    this.description_ = (opt_params && opt_params.description) || '';

    /**
     * Connection status of the destination.
     * @private {!print_preview.Destination.ConnectionStatus}
     */
    this.connectionStatus_ = connectionStatus;

    /**
     * Number of milliseconds since the epoch when the printer was last
     * accessed.
     * @private {number}
     */
    this.lastAccessTime_ = (opt_params && opt_params.lastAccessTime) ||
                           Date.now();

    /**
     * Whether the user has accepted the terms-of-service for the print
     * destination. Only applies to the FedEx Office cloud-based printer.
     * {@code null} if terms-of-service does not apply to the print destination.
     * @private {?boolean}
     */
    this.isTosAccepted_ = opt_params && opt_params.isTosAccepted;

    /**
     * Cloud ID for Privet printers.
     * @private {string}
     */
    this.cloudID_ = (opt_params && opt_params.cloudID) || '';
  };

  /**
   * Prefix of the location destination tag.
   * @type {!Array.<string>}
   * @const
   */
  Destination.LOCATION_TAG_PREFIXES = [
    '__cp__location=',
    '__cp__printer-location='
  ];

  /**
   * Enumeration of Google-promoted destination IDs.
   * @enum {string}
   */
  Destination.GooglePromotedId = {
    DOCS: '__google__docs',
    FEDEX: '__google__fedex',
    SAVE_AS_PDF: 'Save as PDF'
  };

  /**
   * Enumeration of the types of destinations.
   * @enum {string}
   */
  Destination.Type = {
    GOOGLE: 'google',
    LOCAL: 'local',
    MOBILE: 'mobile'
  };

  /**
   * Enumeration of the origin types for cloud destinations.
   * @enum {string}
   */
  Destination.Origin = {
    LOCAL: 'local',
    COOKIES: 'cookies',
    PROFILE: 'profile',
    DEVICE: 'device',
    PRIVET: 'privet'
  };

  /**
   * Enumeration of the connection statuses of printer destinations.
   * @enum {string}
   */
  Destination.ConnectionStatus = {
    DORMANT: 'DORMANT',
    OFFLINE: 'OFFLINE',
    ONLINE: 'ONLINE',
    UNKNOWN: 'UNKNOWN',
    UNREGISTERED: 'UNREGISTERED'
  };

  /**
   * Enumeration of relative icon URLs for various types of destinations.
   * @enum {string}
   * @private
   */
  Destination.IconUrl_ = {
    CLOUD: 'images/printer.png',
    CLOUD_SHARED: 'images/printer_shared.png',
    LOCAL: 'images/printer.png',
    MOBILE: 'images/mobile.png',
    MOBILE_SHARED: 'images/mobile_shared.png',
    THIRD_PARTY: 'images/third_party.png',
    PDF: 'images/pdf.png',
    DOCS: 'images/google_doc.png',
    FEDEX: 'images/third_party_fedex.png'
  };

  Destination.prototype = {
    /** @return {string} ID of the destination. */
    get id() {
      return this.id_;
    },

    /** @return {!print_preview.Destination.Type} Type of the destination. */
    get type() {
      return this.type_;
    },

    /**
     * @return {!print_preview.Destination.Origin} Origin of the destination.
     */
    get origin() {
      return this.origin_;
    },

    /** @return {string} Display name of the destination. */
    get displayName() {
      return this.displayName_;
    },

    /** @return {boolean} Whether the destination has been used recently. */
    get isRecent() {
      return this.isRecent_;
    },

    /**
     * @param {boolean} isRecent Whether the destination has been used recently.
     */
    set isRecent(isRecent) {
      this.isRecent_ = isRecent;
    },

    /**
     * @return {boolean} Whether the user owns the destination. Only applies to
     *     cloud-based destinations.
     */
    get isOwned() {
      return this.isOwned_;
    },

    /**
     * @return {string} Account this destination is registered for, if known.
     */
    get account() {
      return this.account_;
    },

    /** @return {boolean} Whether the destination is local or cloud-based. */
    get isLocal() {
      return this.origin_ == Destination.Origin.LOCAL ||
             (this.origin_ == Destination.Origin.PRIVET &&
              this.connectionStatus_ !=
              Destination.ConnectionStatus.UNREGISTERED);
    },

    /** @return {boolean} Whether the destination is a Privet local printer */
    get isPrivet() {
      return this.origin_ == Destination.Origin.PRIVET;
    },

    /**
     * @return {string} The location of the destination, or an empty string if
     *     the location is unknown.
     */
    get location() {
      if (this.location_ == null) {
        this.location_ = '';
        this.tags_.some(function(tag) {
          return Destination.LOCATION_TAG_PREFIXES.some(function(prefix) {
            if (tag.indexOf(prefix) == 0) {
              this.location_ = tag.substring(prefix.length) || '';
              return true;
            }
          }, this);
        }, this);
      }
      return this.location_;
    },

    /**
     * @return {string} The description of the destination, or an empty string,
     *     if it was not provided.
     */
    get description() {
      return this.description_;
    },

    /**
     * @return {string} Most relevant string to help user to identify this
     *     destination.
     */
    get hint() {
      if (this.id_ == Destination.GooglePromotedId.DOCS ||
          this.id_ == Destination.GooglePromotedId.FEDEX) {
        return this.account_;
      }
      return this.location || this.description;
    },

    /** @return {!Array.<string>} Tags associated with the destination. */
    get tags() {
      return this.tags_.slice(0);
    },

    /** @return {string} Cloud ID associated with the destination */
    get cloudID() {
      return this.cloudID_;
    },

    /** @return {print_preview.Cdd} Print capabilities of the destination. */
    get capabilities() {
      return this.capabilities_;
    },

    /**
     * @param {!print_preview.Cdd} capabilities Print capabilities of the
     *     destination.
     */
    set capabilities(capabilities) {
      this.capabilities_ = capabilities;
    },

    /**
     * @return {!print_preview.Destination.ConnectionStatus} Connection status
     *     of the print destination.
     */
    get connectionStatus() {
      return this.connectionStatus_;
    },

    /**
     * @param {!print_preview.Destination.ConnectionStatus} status Connection
     *     status of the print destination.
     */
    set connectionStatus(status) {
      this.connectionStatus_ = status;
    },

    /** @return {boolean} Whether the destination is considered offline. */
    get isOffline() {
      return arrayContains([print_preview.Destination.ConnectionStatus.OFFLINE,
                            print_preview.Destination.ConnectionStatus.DORMANT],
                            this.connectionStatus_);
    },

    /** @return {string} Human readable status for offline destination. */
    get offlineStatusText() {
      if (!this.isOffline) {
        return '';
      }
      var offlineDurationMs = Date.now() - this.lastAccessTime_;
      var offlineMessageId;
      if (offlineDurationMs > 31622400000.0) {  // One year.
        offlineMessageId = 'offlineForYear';
      } else if (offlineDurationMs > 2678400000.0) {  // One month.
        offlineMessageId = 'offlineForMonth';
      } else if (offlineDurationMs > 604800000.0) {  // One week.
        offlineMessageId = 'offlineForWeek';
      } else {
        offlineMessageId = 'offline';
      }
      return localStrings.getString(offlineMessageId);
    },

    /**
     * @return {number} Number of milliseconds since the epoch when the printer
     *     was last accessed.
     */
    get lastAccessTime() {
      return this.lastAccessTime_;
    },

    /** @return {string} Relative URL of the destination's icon. */
    get iconUrl() {
      if (this.id_ == Destination.GooglePromotedId.DOCS) {
        return Destination.IconUrl_.DOCS;
      } else if (this.id_ == Destination.GooglePromotedId.FEDEX) {
        return Destination.IconUrl_.FEDEX;
      } else if (this.id_ == Destination.GooglePromotedId.SAVE_AS_PDF) {
        return Destination.IconUrl_.PDF;
      } else if (this.isLocal) {
        return Destination.IconUrl_.LOCAL;
      } else if (this.type_ == Destination.Type.MOBILE && this.isOwned_) {
        return Destination.IconUrl_.MOBILE;
      } else if (this.type_ == Destination.Type.MOBILE) {
        return Destination.IconUrl_.MOBILE_SHARED;
      } else if (this.isOwned_) {
        return Destination.IconUrl_.CLOUD;
      } else {
        return Destination.IconUrl_.CLOUD_SHARED;
      }
    },

    /**
     * @return {?boolean} Whether the user has accepted the terms-of-service of
     *     the print destination or {@code null} if a terms-of-service does not
     *     apply.
     */
    get isTosAccepted() {
      return this.isTosAccepted_;
    },

    /**
     * @param {?boolean} Whether the user has accepted the terms-of-service of
     *     the print destination or {@code null} if a terms-of-service does not
     *     apply.
     */
    set isTosAccepted(isTosAccepted) {
      this.isTosAccepted_ = isTosAccepted;
    },

    /**
     * @return {!Array.<string>} Properties (besides display name) to match
     *     search queries against.
     */
    get extraPropertiesToMatch() {
      return [this.location, this.description];
    },

    /**
     * Matches a query against the destination.
     * @param {!RegExp} query Query to match against the destination.
     * @return {boolean} {@code true} if the query matches this destination,
     *     {@code false} otherwise.
     */
    matches: function(query) {
      return this.displayName_.match(query) ||
          this.extraPropertiesToMatch.some(function(property) {
            return property.match(query);
          });
    }
  };

  /**
   * The CDD (Cloud Device Description) describes the capabilities of a print
   * destination.
   *
   * @typedef {{
   *   version: string,
   *   printer: {
   *     vendor_capability: !Array.<{Object}>,
   *     collate: {default: boolean=}=,
   *     color: {
   *       option: !Array.<{
   *         type: string=,
   *         vendor_id: string=,
   *         custom_display_name: string=,
   *         is_default: boolean=
   *       }>
   *     }=,
   *     copies: {default: number=, max: number=}=,
   *     duplex: {option: !Array.<{type: string=, is_default: boolean=}>}=,
   *     page_orientation: {
   *       option: !Array.<{type: string=, is_default: boolean=}>
   *     }=
   *   }
   * }}
   */
  var Cdd = Object;

  // Export
  return {
    Destination: Destination,
    Cdd: Cdd
  };
});
