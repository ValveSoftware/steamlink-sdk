// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The recently closed menu: button, model data, and menu.
 */

cr.define('ntp', function() {
  'use strict';

  /**
   * Returns the text used for a recently closed window.
   * @param {number} numTabs Number of tabs in the window.
   * @return {string} The text to use.
   */
  function formatTabsText(numTabs) {
    if (numTabs == 1)
      return loadTimeData.getString('closedwindowsingle');
    return loadTimeData.getStringF('closedwindowmultiple', numTabs);
  }

  var Menu = cr.ui.Menu;
  var MenuItem = cr.ui.MenuItem;
  var MenuButton = cr.ui.MenuButton;
  var RecentMenuButton = cr.ui.define('button');

  RecentMenuButton.prototype = {
    __proto__: MenuButton.prototype,

    decorate: function() {
      MenuButton.prototype.decorate.call(this);
      this.menu = new Menu;
      cr.ui.decorate(this.menu, Menu);
      this.menu.classList.add('footer-menu');
      document.body.appendChild(this.menu);

      this.needsRebuild_ = true;
      this.anchorType = cr.ui.AnchorType.ABOVE;
      this.invertLeftRight = true;
    },

    /**
     * Shows the menu, first rebuilding it if necessary.
     * TODO(estade): the right of the menu should align with the right of the
     * button.
     * @override
     */
    showMenu: function(shouldSetFocus) {
      if (this.needsRebuild_) {
        this.menu.textContent = '';
        this.dataItems_.forEach(this.addItem_, this);
        this.needsRebuild_ = false;
      }

      MenuButton.prototype.showMenu.apply(this, arguments);
    },

    /**
     * Sets the menu model data.
     * @param {Array} dataItems Array of objects that describe the apps.
     */
    set dataItems(dataItems) {
      this.dataItems_ = dataItems;
      this.needsRebuild_ = true;
      this.hidden = !dataItems.length;
    },

    /**
     * Adds an app to the menu.
     * @param {Object} data An object encapsulating all data about the app.
     * @private
     */
    addItem_: function(data) {
      var isWindow = data.type == 'window';
      var a = this.ownerDocument.createElement('a');
      a.className = 'footer-menu-item';
      if (isWindow) {
        a.href = '';
        a.classList.add('recent-window');
        a.textContent = formatTabsText(data.tabs.length);
        a.title = data.tabs.map(function(tab) { return tab.title; }).join('\n');
      } else {
        a.href = data.url;
        a.style.backgroundImage = getFaviconImageSet(data.url);
        a.textContent = data.title;
      }

      function onActivated(e) {
        ntp.logTimeToClick('RecentlyClosed');
        chrome.send('recordAppLaunchByURL',
                    [encodeURIComponent(data.url),
                     ntp.APP_LAUNCH.NTP_RECENTLY_CLOSED]);
        var index = Array.prototype.indexOf.call(a.parentNode.children, a);
        var orig = e.originalEvent;
        var button = 0;
        if (orig instanceof MouseEvent)
          button = orig.button;
        var params = [data.sessionId,
                      index,
                      button,
                      orig.altKey,
                      orig.ctrlKey,
                      orig.metaKey,
                      orig.shiftKey];
        chrome.send('reopenTab', params);

        e.preventDefault();
        e.stopPropagation();
      }
      a.addEventListener('activate', onActivated);
      a.addEventListener('click', function(e) { e.preventDefault(); });

      this.menu.appendChild(a);
      cr.ui.decorate(a, MenuItem);
    },
  };

  return {
    RecentMenuButton: RecentMenuButton,
  };
});
