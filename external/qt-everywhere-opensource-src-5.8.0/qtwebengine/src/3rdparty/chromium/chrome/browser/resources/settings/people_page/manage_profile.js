// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-profile' is the settings subpage containing controls to
 * edit a profile's name, icon, and desktop shortcut.
 */
Polymer({
  is: 'settings-manage-profile',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * The currently selected profile icon URL. May be a data URL.
     */
    profileIconUrl: String,

    /**
     * The current profile name.
     */
    profileName: String,

    /**
     * The available icons for selection.
     * @type {!Array<string>}
     */
    availableIcons: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * @private {!settings.ManageProfileBrowserProxy}
     */
    browserProxy_: {
      type: Object,
      value: function() {
        return settings.ManageProfileBrowserProxyImpl.getInstance();
      },
    },
  },

  /** @override */
  attached: function() {
    var setIcons = function(icons) {
      this.availableIcons = icons;
    }.bind(this);

    this.addWebUIListener('available-icons-changed', setIcons);
    this.browserProxy_.getAvailableIcons().then(setIcons);
  },

  /**
   * Handler for when the profile name field is changed, then blurred.
   * @private
   * @param {!Event} event
   */
  onProfileNameChanged_: function(event) {
    if (event.target.invalid)
      return;

    this.browserProxy_.setProfileIconAndName(this.profileIconUrl,
                                             event.target.value);
  },

  /**
   * Handler for when an avatar is activated.
   * @param {!Event} event
   * @private
   */
  onIconActivate_: function(event) {
    this.browserProxy_.setProfileIconAndName(event.detail.selected,
                                             this.profileName);
  },
});
