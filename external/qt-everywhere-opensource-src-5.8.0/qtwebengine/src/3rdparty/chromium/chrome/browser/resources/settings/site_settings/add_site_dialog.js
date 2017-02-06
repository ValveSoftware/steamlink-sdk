// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'add-site-dialog' provides a dialog to add exceptions for a given Content
 * Settings category.
 */
Polymer({
  is: 'add-site-dialog',

  behaviors: [SiteSettingsBehavior],

  properties: {
    /**
     * The site to add an exception for.
     * @private
     */
    site_: String,

    /**
     * Whether this is an allow exception this dialog is adding.
     */
     allowException: Boolean,
  },

  /**
   * Opens the dialog.
   * @param {string} type Whether this was launched from an Allow list or a
   *     Block list.
   */
  open: function(type) {
    this.allowException = type == settings.PermissionValues.ALLOW;
    this.$.dialog.open();
  },

  /**
   * Validates that the pattern entered is valid.
   * @private
   */
  validate_: function() {
    var pattern = this.addPatternWildcard_(this.site_);
    this.browserProxy.isPatternValid(pattern).then(function(isValid) {
      this.$.add.disabled = !isValid;
    }.bind(this));
  },

  /**
   * The tap handler for the Add [Site] button (adds the pattern and closes
   * the dialog).
   * @private
   */
  onSubmit_: function() {
    if (this.$.add.disabled)
      return;  // Can happen when Enter is pressed.
    var pattern = this.addPatternWildcard_(this.site_);
    this.setCategoryPermissionForOrigin(
        pattern, '', this.category, this.allowException ?
            settings.PermissionValues.ALLOW : settings.PermissionValues.BLOCK);
    this.$.dialog.close();
  },
});
