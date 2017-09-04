// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-site-settings-page' is the settings page containing privacy and
 * security site settings.
 */
Polymer({
  is: 'settings-site-settings-page',

  behaviors: [SiteSettingsBehavior],

  properties: {
    /** @private */
    enableSiteSettings_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableSiteSettings');
      },
    },

    /**
     * The category selected by the user.
     */
    categorySelected: {
      type: String,
      notify: true,
    },
  },

  ready: function() {
    this.ContentSettingsTypes = settings.ContentSettingsTypes;
    this.ALL_SITES = settings.ALL_SITES;

    // Look up the default value for each category and show it.
    this.setDefaultValue_(this.ContentSettingsTypes.AUTOMATIC_DOWNLOADS,
        '#automaticDownloads');
    this.setDefaultValue_(this.ContentSettingsTypes.BACKGROUND_SYNC,
        '#backgroundSync');
    this.setDefaultValue_(this.ContentSettingsTypes.CAMERA, '#camera');
    this.setDefaultValue_(this.ContentSettingsTypes.COOKIES, '#cookies');
    this.setDefaultValue_(this.ContentSettingsTypes.GEOLOCATION,
        '#geolocation');
    this.setDefaultValue_(this.ContentSettingsTypes.IMAGES, '#images');
    this.setDefaultValue_(this.ContentSettingsTypes.JAVASCRIPT,
        '#javascript');
    this.setDefaultValue_(this.ContentSettingsTypes.KEYGEN, '#keygen');
    this.setDefaultValue_(this.ContentSettingsTypes.MIC, '#mic');
    this.setDefaultValue_(this.ContentSettingsTypes.NOTIFICATIONS,
        '#notifications');
    this.setDefaultValue_(this.ContentSettingsTypes.PLUGINS, '#plugins');
    this.setDefaultValue_(this.ContentSettingsTypes.POPUPS, '#popups');
    this.setDefaultValue_(this.ContentSettingsTypes.PROTOCOL_HANDLERS,
        '#handlers');
    this.setDefaultValue_(this.ContentSettingsTypes.UNSANDBOXED_PLUGINS,
        '#unsandboxedPlugins');
  },

  setDefaultValue_: function(category, id) {
    this.browserProxy.getDefaultValueForContentType(
        category).then(function(setting) {
          var description = this.computeCategoryDesc(category, setting, false);
          this.$$(id).innerText = description;
        }.bind(this));
  },

  /**
   * Navigate to the route specified in the event dataset.
   * @param {!Event} event The tap event.
   * @private
   */
  onTapNavigate_: function(event) {
    var dataSet = /** @type {{route: string}} */(event.currentTarget.dataset);
    settings.navigateTo(settings.Route[dataSet.route]);
  },
});
