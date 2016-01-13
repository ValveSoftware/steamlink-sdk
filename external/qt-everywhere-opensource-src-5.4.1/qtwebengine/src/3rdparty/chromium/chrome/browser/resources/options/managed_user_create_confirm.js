// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * ManagedUserCreateConfirm class.
   * Encapsulated handling of the confirmation overlay page when creating a
   * managed user.
   * @constructor
   * @class
   */
  function ManagedUserCreateConfirmOverlay() {
    OptionsPage.call(this, 'managedUserCreateConfirm',
                     '',  // The title will be based on the new profile name.
                     'managed-user-created');
  };

  cr.addSingletonGetter(ManagedUserCreateConfirmOverlay);

  ManagedUserCreateConfirmOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    // Info about the newly created profile.
    profileInfo_: null,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      $('managed-user-created-done').onclick = function(event) {
        OptionsPage.closeOverlay();
      };

      var self = this;

      $('managed-user-created-switch').onclick = function(event) {
        OptionsPage.closeOverlay();
        chrome.send('switchToProfile', [self.profileInfo_.filePath]);
      };
    },

    /** @override */
    didShowPage: function() {
      $('managed-user-created-switch').focus();
    },

    /**
     * Sets the profile info used in the dialog and updates the profile name
     * displayed. Called by the profile creation overlay when this overlay is
     * opened.
     * @param {Object} info An object of the form:
     *     info = {
     *       name: "Profile Name",
     *       filePath: "/path/to/profile/data/on/disk",
     *       isManaged: (true|false)
     *       custodianEmail: "example@gmail.com"
     *     };
     * @private
     */
    setProfileInfo_: function(info) {
      this.profileInfo_ = info;
      var MAX_LENGTH = 50;
      var elidedName = elide(info.name, MAX_LENGTH);
      $('managed-user-created-title').textContent =
          loadTimeData.getStringF('managedUserCreatedTitle', elidedName);
      $('managed-user-created-switch').textContent =
          loadTimeData.getStringF('managedUserCreatedSwitch', elidedName);

      // HTML-escape the user-supplied strings before putting them into
      // innerHTML. This is probably excessive for the email address, but
      // belt-and-suspenders is cheap here.
      $('managed-user-created-text').innerHTML =
          loadTimeData.getStringF('managedUserCreatedText',
                                  HTMLEscape(elidedName),
                                  HTMLEscape(elide(info.custodianEmail,
                                                   MAX_LENGTH)));
    },

    /** @override */
    canShowPage: function() {
      return this.profileInfo_ != null;
    },
  };

  // Forward public APIs to private implementations.
  [
    'setProfileInfo',
  ].forEach(function(name) {
    ManagedUserCreateConfirmOverlay[name] = function() {
      var instance = ManagedUserCreateConfirmOverlay.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ManagedUserCreateConfirmOverlay: ManagedUserCreateConfirmOverlay,
  };
});
