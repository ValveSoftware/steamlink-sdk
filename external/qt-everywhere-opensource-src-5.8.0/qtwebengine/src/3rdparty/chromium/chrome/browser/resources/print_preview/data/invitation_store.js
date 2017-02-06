// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Printer sharing invitations data store.
   * @param {!print_preview.UserInfo} userInfo User information repository.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function InvitationStore(userInfo) {
    cr.EventTarget.call(this);

    /**
     * User information repository.
     * @private {!print_preview.UserInfo}
     */
    this.userInfo_ = userInfo;

    /**
     * Maps user account to the list of invitations for this account.
     * @private {!Object<!Array<!print_preview.Invitation>>}
     */
    this.invitations_ = {};

    /**
     * Maps user account to the flag whether the invitations for this account
     * were successfully loaded.
     * @private {!Object<print_preview.InvitationStore.LoadStatus_>}
     */
    this.loadStatus_ = {};

    /**
     * Event tracker used to track event listeners of the destination store.
     * @private {!EventTracker}
     */
    this.tracker_ = new EventTracker();

    /**
     * Used to fetch and process invitations.
     * @private {print_preview.CloudPrintInterface}
     */
    this.cloudPrintInterface_ = null;

    /**
     * Invitation being processed now. Only one invitation can be processed at
     * a time.
     * @private {print_preview.Invitation}
     */
    this.invitationInProgress_ = null;
  };

  /**
   * Event types dispatched by the data store.
   * @enum {string}
   */
  InvitationStore.EventType = {
    INVITATION_PROCESSED:
        'print_preview.InvitationStore.INVITATION_PROCESSED',
    INVITATION_SEARCH_DONE:
        'print_preview.InvitationStore.INVITATION_SEARCH_DONE'
  };

  /**
   * @enum {number}
   * @private
   */
  InvitationStore.LoadStatus_ = {
    IN_PROGRESS: 1,
    DONE: 2,
    FAILED: 3
  };

  InvitationStore.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * @return {print_preview.Invitation} Currently processed invitation or
     *     {@code null}.
     */
    get invitationInProgress() {
      return this.invitationInProgress_;
    },

    /**
     * @param {string} account Account to filter invitations by.
     * @return {!Array<!print_preview.Invitation>} List of invitations for the
     *     {@code account}.
     */
    invitations: function(account) {
      return this.invitations_[account] || [];
    },

    /**
     * Sets the invitation store's Google Cloud Print interface.
     * @param {!print_preview.CloudPrintInterface} cloudPrintInterface Interface
     *     to set.
     */
    setCloudPrintInterface: function(cloudPrintInterface) {
      this.cloudPrintInterface_ = cloudPrintInterface;
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.INVITES_DONE,
          this.onCloudPrintInvitesDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.INVITES_FAILED,
          this.onCloudPrintInvitesDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PROCESS_INVITE_DONE,
          this.onCloudPrintProcessInviteDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PROCESS_INVITE_FAILED,
          this.onCloudPrintProcessInviteFailed_.bind(this));
    },

    /** Initiates loading of cloud printer sharing invitations. */
    startLoadingInvitations: function() {
      if (!this.cloudPrintInterface_)
        return;
      if (!this.userInfo_.activeUser)
        return;
      if (this.loadStatus_.hasOwnProperty(this.userInfo_.activeUser)) {
        if (this.loadStatus_[this.userInfo_.activeUser] ==
            InvitationStore.LoadStatus_.DONE) {
          cr.dispatchSimpleEvent(
              this, InvitationStore.EventType.INVITATION_SEARCH_DONE);
        }
        return;
      }

      this.loadStatus_[this.userInfo_.activeUser] =
          InvitationStore.LoadStatus_.IN_PROGRESS;
      this.cloudPrintInterface_.invites(this.userInfo_.activeUser);
    },

    /**
     * Accepts or rejects the {@code invitation}, based on {@code accept} value.
     * @param {!print_preview.Invitation} invitation Invitation to process.
     * @param {boolean} accept Whether to accept this invitation.
     */
    processInvitation: function(invitation, accept) {
      if (!!this.invitationInProgress_)
        return;
      this.invitationInProgress_ = invitation;
      this.cloudPrintInterface_.processInvite(invitation, accept);
    },

    /**
     * Removes processed invitation from the internal storage.
     * @param {!print_preview.Invitation} invitation Processed invitation.
     * @private
     */
    invitationProcessed_: function(invitation) {
      if (this.invitations_.hasOwnProperty(invitation.account)) {
        this.invitations_[invitation.account] =
            this.invitations_[invitation.account].filter(function(i) {
              return i != invitation;
            });
      }
      if (this.invitationInProgress_ == invitation)
        this.invitationInProgress_ = null;
    },

    /**
     * Called when printer sharing invitations are fetched.
     * @param {Event} event Contains the list of invitations.
     * @private
     */
    onCloudPrintInvitesDone_: function(event) {
      this.loadStatus_[event.user] = InvitationStore.LoadStatus_.DONE;
      this.invitations_[event.user] = event.invitations;

      cr.dispatchSimpleEvent(
          this, InvitationStore.EventType.INVITATION_SEARCH_DONE);
    },

    /**
     * Called when printer sharing invitations fetch has failed.
     * @param {Event} event Contains the reason of failure.
     * @private
     */
    onCloudPrintInvitesFailed_: function(event) {
      this.loadStatus_[event.user] = InvitationStore.LoadStatus_.FAILED;
    },

    /**
     * Called when printer sharing invitation was processed successfully.
     * @param {Event} event Contains detailed information about the invite and
     *     newly accepted destination.
     * @private
     */
    onCloudPrintProcessInviteDone_: function(event) {
      this.invitationProcessed_(event.invitation);
      cr.dispatchSimpleEvent(
          this, InvitationStore.EventType.INVITATION_PROCESSED);
    },

    /**
     * Called when /printer call completes. Updates the specified destination's
     * print capabilities.
     * @param {Event} event Contains detailed information about the
     *     destination.
     * @private
     */
    onCloudPrintProcessInviteFailed_: function(event) {
      this.invitationProcessed_(event.invitation);
      // TODO: Display an error.
      cr.dispatchSimpleEvent(
          this, InvitationStore.EventType.INVITATION_PROCESSED);
    }
  };

  // Export
  return {
    InvitationStore: InvitationStore
  };
});
