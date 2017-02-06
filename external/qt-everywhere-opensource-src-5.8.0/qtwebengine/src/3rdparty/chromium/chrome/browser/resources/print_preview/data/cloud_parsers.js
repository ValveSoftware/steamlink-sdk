// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function() {
  'use strict';

  /** Namespace which contains a method to parse cloud destinations directly. */
  function CloudDestinationParser() {};

  /**
   * Enumeration of cloud destination field names.
   * @enum {string}
   * @private
   */
  CloudDestinationParser.Field_ = {
    CAPABILITIES: 'capabilities',
    CONNECTION_STATUS: 'connectionStatus',
    DESCRIPTION: 'description',
    DISPLAY_NAME: 'displayName',
    ID: 'id',
    IS_TOS_ACCEPTED: 'isTosAccepted',
    LAST_ACCESS: 'accessTime',
    TAGS: 'tags',
    TYPE: 'type'
  };

  /**
   * Special tag that denotes whether the destination has been recently used.
   * @type {string}
   * @const
   * @private
   */
  CloudDestinationParser.RECENT_TAG_ = '^recent';

  /**
   * Special tag that denotes whether the destination is owned by the user.
   * @type {string}
   * @const
   * @private
   */
  CloudDestinationParser.OWNED_TAG_ = '^own';

  /**
   * Enumeration of cloud destination types that are supported by print preview.
   * @enum {string}
   * @private
   */
  CloudDestinationParser.CloudType_ = {
    ANDROID: 'ANDROID_CHROME_SNAPSHOT',
    DOCS: 'DOCS',
    IOS: 'IOS_CHROME_SNAPSHOT'
  };

  /**
   * Parses a destination from JSON from a Google Cloud Print search or printer
   * response.
   * @param {!Object} json Object that represents a Google Cloud Print search or
   *     printer response.
   * @param {!print_preview.Destination.Origin} origin The origin of the
   *     response.
   * @param {string} account The account this destination is registered for or
   *     empty string, if origin != COOKIES.
   * @return {!print_preview.Destination} Parsed destination.
   */
  CloudDestinationParser.parse = function(json, origin, account) {
    if (!json.hasOwnProperty(CloudDestinationParser.Field_.ID) ||
        !json.hasOwnProperty(CloudDestinationParser.Field_.TYPE) ||
        !json.hasOwnProperty(CloudDestinationParser.Field_.DISPLAY_NAME)) {
      throw Error('Cloud destination does not have an ID or a display name');
    }
    var id = json[CloudDestinationParser.Field_.ID];
    var tags = json[CloudDestinationParser.Field_.TAGS] || [];
    var connectionStatus =
        json[CloudDestinationParser.Field_.CONNECTION_STATUS] ||
        print_preview.Destination.ConnectionStatus.UNKNOWN;
    var optionalParams = {
      account: account,
      tags: tags,
      isOwned: arrayContains(tags, CloudDestinationParser.OWNED_TAG_),
      lastAccessTime: parseInt(
          json[CloudDestinationParser.Field_.LAST_ACCESS], 10) || Date.now(),
      isTosAccepted: (id == print_preview.Destination.GooglePromotedId.FEDEX) ?
          json[CloudDestinationParser.Field_.IS_TOS_ACCEPTED] : null,
      cloudID: id,
      description: json[CloudDestinationParser.Field_.DESCRIPTION]
    };
    var cloudDest = new print_preview.Destination(
        id,
        CloudDestinationParser.parseType_(
            json[CloudDestinationParser.Field_.TYPE]),
        origin,
        json[CloudDestinationParser.Field_.DISPLAY_NAME],
        arrayContains(tags, CloudDestinationParser.RECENT_TAG_) /*isRecent*/,
        connectionStatus,
        optionalParams);
    if (json.hasOwnProperty(CloudDestinationParser.Field_.CAPABILITIES)) {
      cloudDest.capabilities = /** @type {!print_preview.Cdd} */(
          json[CloudDestinationParser.Field_.CAPABILITIES]);
    }
    return cloudDest;
  };

  /**
   * Parses the destination type.
   * @param {string} typeStr Destination type given by the Google Cloud Print
   *     server.
   * @return {!print_preview.Destination.Type} Destination type.
   * @private
   */
  CloudDestinationParser.parseType_ = function(typeStr) {
    if (typeStr == CloudDestinationParser.CloudType_.ANDROID ||
        typeStr == CloudDestinationParser.CloudType_.IOS) {
      return print_preview.Destination.Type.MOBILE;
    } else if (typeStr == CloudDestinationParser.CloudType_.DOCS) {
      return print_preview.Destination.Type.GOOGLE_PROMOTED;
    } else {
      return print_preview.Destination.Type.GOOGLE;
    }
  };

  /** Namespace which contains a method to parse printer sharing invitation. */
  function InvitationParser() {};

  /**
   * Enumeration of invitation field names.
   * @enum {string}
   * @private
   */
  InvitationParser.Field_ = {
    PRINTER: 'printer',
    RECEIVER: 'receiver',
    SENDER: 'sender'
  };

  /**
   * Enumeration of cloud destination types that are supported by print preview.
   * @enum {string}
   * @private
   */
  InvitationParser.AclType_ = {
    DOMAIN: 'DOMAIN',
    GROUP: 'GROUP',
    PUBLIC: 'PUBLIC',
    USER: 'USER'
  };

  /**
   * Parses printer sharing invitation from JSON from GCP invite API response.
   * @param {!Object} json Object that represents a invitation search response.
   * @param {string} account The account this invitation is sent for.
   * @return {!print_preview.Invitation} Parsed invitation.
   */
  InvitationParser.parse = function(json, account) {
    if (!json.hasOwnProperty(InvitationParser.Field_.SENDER) ||
        !json.hasOwnProperty(InvitationParser.Field_.RECEIVER) ||
        !json.hasOwnProperty(InvitationParser.Field_.PRINTER)) {
      throw Error('Invitation does not have necessary info.');
    }

    var nameFormatter = function(name, scope) {
      return name && scope ? (name + ' (' + scope + ')') : (name || scope);
    };

    var sender = json[InvitationParser.Field_.SENDER];
    var senderName = nameFormatter(sender['name'], sender['email']);

    var receiver = json[InvitationParser.Field_.RECEIVER];
    var receiverName = '';
    var receiverType = receiver['type'];
    if (receiverType == InvitationParser.AclType_.USER) {
      // It's a personal invitation, empty name indicates just that.
    } else if (receiverType == InvitationParser.AclType_.GROUP ||
               receiverType == InvitationParser.AclType_.DOMAIN) {
      receiverName = nameFormatter(receiver['name'], receiver['scope']);
    } else {
      throw Error('Invitation of unsupported receiver type');
    }

    var destination = cloudprint.CloudDestinationParser.parse(
        json[InvitationParser.Field_.PRINTER],
        print_preview.Destination.Origin.COOKIES,
        account);

    return new print_preview.Invitation(
        senderName, receiverName, destination, receiver, account);
  };

  // Export
  return {
    CloudDestinationParser: CloudDestinationParser,
    InvitationParser: InvitationParser
  };
});
