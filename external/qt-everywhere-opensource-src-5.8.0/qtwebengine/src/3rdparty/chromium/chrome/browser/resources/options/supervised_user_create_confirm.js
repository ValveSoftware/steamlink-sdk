// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * SupervisedUserCreateConfirm class.
   * Encapsulated handling of the confirmation overlay page when creating a
   * supervised user.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function SupervisedUserCreateConfirmOverlay() {
    Page.call(this, 'supervisedUserCreateConfirm',
              '',  // The title will be based on the new profile name.
              'supervised-user-created');
  };

  cr.addSingletonGetter(SupervisedUserCreateConfirmOverlay);

  SupervisedUserCreateConfirmOverlay.prototype = {
    // Inherit from Page.
    __proto__: Page.prototype,

    // Info about the newly created profile.
    profileInfo_: null,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('supervised-user-created-done').onclick = function(event) {
        PageManager.closeOverlay();
      };

      var self = this;

      $('supervised-user-created-switch').onclick = function(event) {
        PageManager.closeOverlay();
        chrome.send('switchToProfile', [self.profileInfo_.filePath]);
      };
    },

    /** @override */
    didShowPage: function() {
      $('supervised-user-created-switch').focus();
    },

    /**
     * Sets the profile info used in the dialog and updates the profile name
     * displayed. Called by the profile creation overlay when this overlay is
     * opened.
     * @param {Object} info An object of the form:
     *     info = {
     *       name: "Profile Name",
     *       filePath: "/path/to/profile/data/on/disk",
     *       isSupervised: (true|false)
     *       custodianEmail: "example@gmail.com"
     *     };
     * @private
     */
    setProfileInfo_: function(info) {
      this.profileInfo_ = info;
      var MAX_LENGTH = 50;
      var elidedName = elide(info.name, MAX_LENGTH);
      $('supervised-user-created-title').textContent =
          loadTimeData.getStringF('supervisedUserCreatedTitle', elidedName);
      $('supervised-user-created-switch').textContent =
          loadTimeData.getStringF('supervisedUserCreatedSwitch', elidedName);

      // HTML-escape the user-supplied strings before putting them into
      // innerHTML. This is probably excessive for the email address, but
      // belt-and-suspenders is cheap here.
      $('supervised-user-created-text').innerHTML =
          loadTimeData.getStringF('supervisedUserCreatedText',
                                  HTMLEscape(elidedName),
                                  HTMLEscape(elide(info.custodianEmail,
                                                   MAX_LENGTH)));
    },

    /** @override */
    canShowPage: function() {
      return this.profileInfo_ != null;
    },

    /**
     * Updates the displayed profile name if it has changed. Called by the
     * handler.
     * @param {string} filePath The file path of the profile whose name
     *     changed.
     * @param {string} newName The changed profile's new name.
     * @private
     */
    onUpdatedProfileName_: function(filePath, newName) {
      if (filePath == this.profileInfo_.filePath) {
        this.profileInfo_.name = newName;
        this.setProfileInfo_(this.profileInfo_);
      }
    },

    /**
     * Closes this overlay if the new profile has been deleted. Called by the
     * handler.
     * @param {string} filePath The file path of the profile that was deleted.
     * @private
     */
    onDeletedProfile_: function(filePath) {
      if (filePath == this.profileInfo_.filePath) {
        PageManager.closeOverlay();
      }
    },
  };

  // Forward public APIs to private implementations.
  cr.makePublic(SupervisedUserCreateConfirmOverlay, [
    'onDeletedProfile',
    'onUpdatedProfileName',
    'setProfileInfo',
  ]);

  // Export
  return {
    SupervisedUserCreateConfirmOverlay: SupervisedUserCreateConfirmOverlay,
  };
});
