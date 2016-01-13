// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implement the recommended apps card in the launcher start page.
 */

cr.define('appList.startPage', function() {
  'use strict';

  /**
   * Create a view with icon and label for the given app data.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var AppItemView = cr.ui.define('div');

  AppItemView.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * The app id of the app displayed by this view. Used to launch
     * the app when the view is clicked.
     * @type {string}
     */
    appId: '',

    /**
     * Sets the icon URL to display the app icon.
     * @type {string}
     */
    set iconUrl(url) {
      this.style.backgroundImage = 'url(' + url + ')';
    },

    /**
     * Sets the text title.
     * @type {string}
     */
    set textTitle(title) {
      this.textContent = title;
    },

    /** @override */
    decorate: function() {
      this.className = 'app';
      this.addEventListener('click', this.handleClick_.bind(this));
    },

    /**
     * Handles 'click' event.
     * @private
     */
    handleClick_: function() {
      assert(this.appId);
      chrome.send('launchApp', [this.appId]);
    }
  };

  /**
   * Create recommended apps card.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var RecommendedApps = cr.ui.define('div');

  RecommendedApps.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      this.className = 'recommended-apps';
    },

    /**
     * Sets the apps to be displayed in this card.
     */
    setApps: function(apps) {
      this.textContent = '';
      for (var i = 0; i < apps.length; ++i) {
        this.appendChild(new AppItemView(apps[i]));
      }
    }
  };

  return {
    RecommendedApps: RecommendedApps
  };
});
