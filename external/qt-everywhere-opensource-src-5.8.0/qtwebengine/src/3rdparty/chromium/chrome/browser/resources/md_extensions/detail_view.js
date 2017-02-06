// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  var DetailView = Polymer({
    is: 'extensions-detail-view',

    behaviors: [Polymer.NeonAnimatableBehavior],

    properties: {
      /**
       * The underlying ExtensionInfo for the details being displayed.
       * @type {chrome.developerPrivate.ExtensionInfo}
       */
      data: Object,

      /** @type {!extensions.ItemDelegate} */
      delegate: Object,
    },

    ready: function() {
      this.sharedElements = {hero: this.$.main};
      /** @type {!extensions.AnimationHelper} */
      this.animationHelper = new extensions.AnimationHelper(this, this.$.main);
    },

    /** @private */
    onCloseButtonClick_: function() {
      this.fire('close');
    },

    /**
     * @return {boolean}
     * @private
     */
    hasDependentExtensions_: function() {
      return this.data.dependentExtensions.length > 0;
    },

    /**
     * @return {boolean}
     * @private
     */
    hasPermissions_: function() {
      return this.data.permissions.length > 0;
    },

    /**
     * @return {boolean}
     * @private
     */
    shouldShowOptionsSection_: function() {
      return this.data.incognitoAccess.isEnabled ||
             this.data.fileAccess.isEnabled ||
             this.data.runOnAllUrls.isEnabled ||
             this.data.errorCollection.isEnabled;
    },

    /** @private */
    onAllowIncognitoChange_: function() {
      this.delegate.setItemAllowedIncognito(
          this.data.id, this.$$('#allow-incognito').checked);
    },

    /** @private */
    onAllowOnFileUrlsChange_: function() {
      this.delegate.setItemAllowedOnFileUrls(
          this.data.id, this.$$('#allow-on-file-urls').checked);
    },

    /** @private */
    onAllowOnAllSitesChange_: function() {
      this.delegate.setItemAllowedOnAllSites(
          this.data.id, this.$$('#allow-on-all-sites').checked);
    },

    /** @private */
    onCollectErrorsChange_: function() {
      this.delegate.setItemCollectsErrors(
          this.data.id, this.$$('#collect-errors').checked);
    },

    /**
     * @param {!chrome.developerPrivate.DependentExtension} item
     * @private
     */
    computeDependentEntry_: function(item) {
      return loadTimeData.getStringF('itemDependentEntry', item.name, item.id);
    },
  });

  return {DetailView: DetailView};
});
