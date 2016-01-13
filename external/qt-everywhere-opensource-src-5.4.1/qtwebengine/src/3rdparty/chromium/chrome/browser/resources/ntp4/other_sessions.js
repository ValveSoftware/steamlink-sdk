// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The menu that shows tabs from sessions on other devices.
 */

cr.define('ntp', function() {
  'use strict';

  /** @const */ var ContextMenuButton = cr.ui.ContextMenuButton;
  /** @const */ var Menu = cr.ui.Menu;
  /** @const */ var MenuItem = cr.ui.MenuItem;
  /** @const */ var MenuButton = cr.ui.MenuButton;
  /** @const */ var OtherSessionsMenuButton = cr.ui.define('button');

  // Histogram buckets for UMA tracking of menu usage.
  /** @const */ var HISTOGRAM_EVENT = {
      INITIALIZED: 0,
      SHOW_MENU: 1,
      LINK_CLICKED: 2,
      LINK_RIGHT_CLICKED: 3,
      SESSION_NAME_RIGHT_CLICKED: 4,
      SHOW_SESSION_MENU: 5,
      COLLAPSE_SESSION: 6,
      EXPAND_SESSION: 7,
      OPEN_ALL: 8
  };
  /** @const */ var HISTOGRAM_EVENT_LIMIT =
      HISTOGRAM_EVENT.OPEN_ALL + 1;

  /**
   * Record an event in the UMA histogram.
   * @param {number} eventId The id of the event to be recorded.
   * @private
   */
  function recordUmaEvent_(eventId) {
    chrome.send('metricsHandler:recordInHistogram',
        ['NewTabPage.OtherSessionsMenu', eventId, HISTOGRAM_EVENT_LIMIT]);
  }

  OtherSessionsMenuButton.prototype = {
    __proto__: MenuButton.prototype,

    decorate: function() {
      MenuButton.prototype.decorate.call(this);
      this.menu = new Menu;
      cr.ui.decorate(this.menu, Menu);
      this.menu.menuItemSelector = '[role=menuitem]';
      this.menu.classList.add('footer-menu');
      this.menu.addEventListener('contextmenu',
                                 this.onContextMenu_.bind(this), true);
      document.body.appendChild(this.menu);

      // Create the context menu that appears when the user right clicks
      // on a device name.
      this.deviceContextMenu_ = DeviceContextMenuController.getInstance().menu;
      document.body.appendChild(this.deviceContextMenu_);

      this.promoMessage_ = $('other-sessions-promo-template').cloneNode(true);
      this.promoMessage_.removeAttribute('id');  // Prevent a duplicate id.

      this.sessions_ = [];
      this.anchorType = cr.ui.AnchorType.ABOVE;
      this.invertLeftRight = true;

      // Initialize the images for the drop-down buttons that appear beside the
      // session names.
      MenuButton.createDropDownArrows();

      recordUmaEvent_(HISTOGRAM_EVENT.INITIALIZED);
    },

    /**
     * Initialize this element.
     * @param {boolean} signedIn Is the current user signed in?
     */
    initialize: function(signedIn) {
      this.updateSignInState(signedIn);
    },

    /**
     * Handle a context menu event for an object in the menu's DOM subtree.
     */
    onContextMenu_: function(e) {
      // Only record the action if it occurred in one of the menu items or
      // on one of the session headings.
      if (findAncestorByClass(e.target, 'footer-menu-item')) {
        recordUmaEvent_(HISTOGRAM_EVENT.LINK_RIGHT_CLICKED);
      } else {
        var heading = findAncestorByClass(e.target, 'session-heading');
        if (heading) {
          recordUmaEvent_(HISTOGRAM_EVENT.SESSION_NAME_RIGHT_CLICKED);

          // Let the context menu know which session it was invoked on,
          // since they all share the same instance of the menu.
          DeviceContextMenuController.getInstance().setSession(
              heading.sessionData_);
        }
      }
    },

    /**
     * Hides the menu.
     * @override
     */
    hideMenu: function() {
      // Don't hide if the device context menu is currently showing.
      if (this.deviceContextMenu_.hidden)
        MenuButton.prototype.hideMenu.call(this);
    },

    /**
     * Shows the menu, first rebuilding it if necessary.
     * TODO(estade): the right of the menu should align with the right of the
     * button.
     * @override
     */
    showMenu: function(shouldSetFocus) {
      if (this.sessions_.length == 0)
        chrome.send('getForeignSessions');
      recordUmaEvent_(HISTOGRAM_EVENT.SHOW_MENU);
      MenuButton.prototype.showMenu.apply(this, arguments);

      // Work around https://bugs.webkit.org/show_bug.cgi?id=85884.
      this.menu.scrollTop = 0;
    },

    /**
     * Reset the menu contents to the default state.
     * @private
     */
    resetMenuContents_: function() {
      this.menu.innerHTML = '';
      this.menu.appendChild(this.promoMessage_);
    },

    /**
     * Create a custom click handler for a link, so that clicking on a link
     * restores the session (including back stack) rather than just opening
     * the URL.
     */
    makeClickHandler_: function(sessionTag, windowId, tabId) {
      var self = this;
      return function(e) {
        recordUmaEvent_(HISTOGRAM_EVENT.LINK_CLICKED);
        chrome.send('openForeignSession', [sessionTag, windowId, tabId,
            e.button, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey]);
        e.preventDefault();
      };
    },

    /**
     * Add the UI for a foreign session to the menu.
     * @param {Object} session Object describing the foreign session.
     */
    addSession_: function(session) {
      var doc = this.ownerDocument;

      var section = doc.createElement('section');
      this.menu.appendChild(section);

      var heading = doc.createElement('h3');
      heading.className = 'session-heading';
      heading.textContent = session.name;
      heading.sessionData_ = session;
      section.appendChild(heading);

      var dropDownButton = new ContextMenuButton;
      dropDownButton.classList.add('drop-down');
      // Keep track of the drop down that triggered the menu, so we know
      // which element to apply the command to.
      function handleDropDownFocus(e) {
        DeviceContextMenuController.getInstance().setSession(session);
      }
      dropDownButton.addEventListener('mousedown', handleDropDownFocus);
      dropDownButton.addEventListener('focus', handleDropDownFocus);
      heading.appendChild(dropDownButton);

      var timeSpan = doc.createElement('span');
      timeSpan.className = 'details';
      timeSpan.textContent = session.modifiedTime;
      heading.appendChild(timeSpan);

      cr.ui.contextMenuHandler.setContextMenu(heading,
                                              this.deviceContextMenu_);

      if (!session.collapsed)
        section.appendChild(this.createSessionContents_(session));
    },

    /**
     * Create the DOM tree representing the tabs and windows in a session.
     * @param {Object} session The session model object.
     * @return {Element} A single div containing the list of tabs & windows.
     * @private
     */
    createSessionContents_: function(session) {
      var doc = this.ownerDocument;
      var contents = doc.createElement('div');

      for (var i = 0; i < session.windows.length; i++) {
        var window = session.windows[i];

        // Show a separator between multiple windows in the same session.
        if (i > 0)
          contents.appendChild(doc.createElement('hr'));

        for (var j = 0; j < window.tabs.length; j++) {
          var tab = window.tabs[j];
          var a = doc.createElement('a');
          a.className = 'footer-menu-item';
          a.textContent = tab.title;
          a.href = tab.url;
          a.style.backgroundImage = getFaviconImageSet(tab.url);

          var clickHandler = this.makeClickHandler_(
              session.tag, String(window.sessionId), String(tab.sessionId));
          a.addEventListener('click', clickHandler);
          contents.appendChild(a);
          cr.ui.decorate(a, MenuItem);
        }
      }

      return contents;
    },

    /**
     * Sets the menu model data. An empty list means that either there are no
     * foreign sessions, or tab sync is disabled for this profile.
     * |isTabSyncEnabled| makes it possible to distinguish between the cases.
     *
     * @param {Array} sessionList Array of objects describing the sessions
     *     from other devices.
     * @param {boolean} isTabSyncEnabled Is tab sync enabled for this profile?
     */
    setForeignSessions: function(sessionList, isTabSyncEnabled) {
      this.sessions_ = sessionList;
      this.resetMenuContents_();
      if (sessionList.length > 0) {
        // Rebuild the menu with the new data.
        for (var i = 0; i < sessionList.length; i++) {
          this.addSession_(sessionList[i]);
        }
      }

      // The menu button is shown iff tab sync is enabled.
      this.hidden = !isTabSyncEnabled;
    },

    /**
     * Called when this element is initialized, and from the new tab page when
     * the user's signed in state changes,
     * @param {boolean} signedIn Is the user currently signed in?
     */
    updateSignInState: function(signedIn) {
      if (signedIn)
        chrome.send('getForeignSessions');
      else
        this.hidden = true;
    },
  };

  /**
   * Controller for the context menu for device names in the list of sessions.
   * This class is designed to be used as a singleton.
   *
   * @constructor
   */
  function DeviceContextMenuController() {
    this.__proto__ = DeviceContextMenuController.prototype;
    this.initialize();
  }
  cr.addSingletonGetter(DeviceContextMenuController);

  DeviceContextMenuController.prototype = {

    initialize: function() {
      var menu = new cr.ui.Menu;
      cr.ui.decorate(menu, cr.ui.Menu);
      menu.classList.add('device-context-menu');
      menu.classList.add('footer-menu-context-menu');
      this.menu = menu;
      this.collapseItem_ = this.appendMenuItem_('collapseSessionMenuItemText');
      this.collapseItem_.addEventListener('activate',
                                          this.onCollapseOrExpand_.bind(this));
      this.expandItem_ = this.appendMenuItem_('expandSessionMenuItemText');
      this.expandItem_.addEventListener('activate',
                                        this.onCollapseOrExpand_.bind(this));
      this.openAllItem_ = this.appendMenuItem_('restoreSessionMenuItemText');
      this.openAllItem_.addEventListener('activate',
                                         this.onOpenAll_.bind(this));
    },

    /**
     * Appends a menu item to |this.menu|.
     * @param {string} textId The ID for the localized string that acts as
     *     the item's label.
     */
    appendMenuItem_: function(textId) {
      var button = cr.doc.createElement('button');
      this.menu.appendChild(button);
      cr.ui.decorate(button, cr.ui.MenuItem);
      button.textContent = loadTimeData.getString(textId);
      return button;
    },

    /**
     * Handler for the 'Collapse' and 'Expand' menu items.
     * @param {Event} e The activation event.
     * @private
     */
    onCollapseOrExpand_: function(e) {
      this.session_.collapsed = !this.session_.collapsed;
      this.updateMenuItems_();
      chrome.send('setForeignSessionCollapsed',
                  [this.session_.tag, this.session_.collapsed]);
      chrome.send('getForeignSessions');  // Refresh the list.

      var eventId = this.session_.collapsed ?
          HISTOGRAM_EVENT.COLLAPSE_SESSION : HISTOGRAM_EVENT.EXPAND_SESSION;
      recordUmaEvent_(eventId);
    },

    /**
     * Handler for the 'Open all' menu item.
     * @param {Event} e The activation event.
     * @private
     */
    onOpenAll_: function(e) {
      chrome.send('openForeignSession', [this.session_.tag]);
      recordUmaEvent_(HISTOGRAM_EVENT.OPEN_ALL);
    },

    /**
     * Set the session data for the session the context menu was invoked on.
     * This should never be called when the menu is visible.
     * @param {Object} session The model object for the session.
     */
    setSession: function(session) {
      this.session_ = session;
      this.updateMenuItems_();
    },

    /**
     * Set the visibility of the Expand/Collapse menu items based on the state
     * of the session that this menu is currently associated with.
     * @private
     */
    updateMenuItems_: function() {
      this.collapseItem_.hidden = this.session_.collapsed;
      this.expandItem_.hidden = !this.session_.collapsed;
    }
  };

  return {
    OtherSessionsMenuButton: OtherSessionsMenuButton,
  };
});
