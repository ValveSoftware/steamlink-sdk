// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The section of the history page that shows tabs from sessions
                 on other devices.
 */

///////////////////////////////////////////////////////////////////////////////
// Globals:
/** @const */ var MAX_NUM_COLUMNS = 3;
/** @const */ var NB_ENTRIES_FIRST_ROW_COLUMN = 6;
/** @const */ var NB_ENTRIES_OTHER_ROWS_COLUMN = 0;

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
  OPEN_ALL: 8,
  HAS_FOREIGN_DATA: 9,
  HIDE_FOR_NOW: 10,
  LIMIT: 11  // Should always be the last one.
};

/**
 * Record an event in the UMA histogram.
 * @param {number} eventId The id of the event to be recorded.
 * @private
 */
function recordUmaEvent_(eventId) {
  chrome.send('metricsHandler:recordInHistogram',
      ['HistoryPage.OtherDevicesMenu', eventId, HISTOGRAM_EVENT.LIMIT]);
}

///////////////////////////////////////////////////////////////////////////////
// DeviceContextMenuController:

/**
 * Controller for the context menu for device names in the list of sessions.
 * @constructor
 */
function DeviceContextMenuController() {
  this.__proto__ = DeviceContextMenuController.prototype;
  this.initialize();
}
cr.addSingletonGetter(DeviceContextMenuController);

// DeviceContextMenuController, Public: ---------------------------------------

/**
 * Initialize the context menu for device names in the list of sessions.
 */
DeviceContextMenuController.prototype.initialize = function() {
  var menu = new cr.ui.Menu;
  cr.ui.decorate(menu, cr.ui.Menu);
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
};

/**
 * Set the session data for the session the context menu was invoked on.
 * This should never be called when the menu is visible.
 * @param {Object} session The model object for the session.
 */
DeviceContextMenuController.prototype.setSession = function(session) {
  this.session_ = session;
  this.updateMenuItems_();
};

// DeviceContextMenuController, Private: --------------------------------------

/**
 * Appends a menu item to |this.menu|.
 * @param {string} textId The ID for the localized string that acts as
 *     the item's label.
 * @return {Element} The button used for a given menu option.
 * @private
 */
DeviceContextMenuController.prototype.appendMenuItem_ = function(textId) {
  var button = document.createElement('button');
  this.menu.appendChild(button);
  cr.ui.decorate(button, cr.ui.MenuItem);
  button.textContent = loadTimeData.getString(textId);
  return button;
};

/**
 * Handler for the 'Collapse' and 'Expand' menu items.
 * @param {Event} e The activation event.
 * @private
 */
DeviceContextMenuController.prototype.onCollapseOrExpand_ = function(e) {
  this.session_.collapsed = !this.session_.collapsed;
  this.updateMenuItems_();
  chrome.send('setForeignSessionCollapsed',
              [this.session_.tag, this.session_.collapsed]);
  chrome.send('getForeignSessions');  // Refresh the list.

  var eventId = this.session_.collapsed ?
      HISTOGRAM_EVENT.COLLAPSE_SESSION : HISTOGRAM_EVENT.EXPAND_SESSION;
  recordUmaEvent_(eventId);
};

/**
 * Handler for the 'Open all' menu item.
 * @param {Event} e The activation event.
 * @private
 */
DeviceContextMenuController.prototype.onOpenAll_ = function(e) {
  chrome.send('openForeignSession', [this.session_.tag]);
  recordUmaEvent_(HISTOGRAM_EVENT.OPEN_ALL);
};

/**
 * Set the visibility of the Expand/Collapse menu items based on the state
 * of the session that this menu is currently associated with.
 * @private
 */
DeviceContextMenuController.prototype.updateMenuItems_ = function() {
  this.collapseItem_.hidden = this.session_.collapsed;
  this.expandItem_.hidden = !this.session_.collapsed;
  this.menu.selectedItem = this.menu.querySelector(':not([hidden])');
};


///////////////////////////////////////////////////////////////////////////////
// Device:

/**
 * Class to hold all the information about a device entry and generate a DOM
 * node for it.
 * @param {Object} session An object containing the device's session data.
 * @param {DevicesView} view The view object this entry belongs to.
 * @constructor
 */
function Device(session, view) {
  this.view_ = view;
  this.session_ = session;
  this.searchText_ = view.getSearchText();
}

// Device, Public: ------------------------------------------------------------

/**
 * Get the DOM node to display this device.
 * @param {int} maxNumTabs The maximum number of tabs to display.
 * @param {int} row The row in which this device is displayed.
 * @return {Object} A DOM node to draw the device.
 */
Device.prototype.getDOMNode = function(maxNumTabs, row) {
  var deviceDiv = createElementWithClassName('div', 'device');
  this.row_ = row;
  if (!this.session_)
    return deviceDiv;

  // Name heading
  var heading = document.createElement('h3');
  var name = heading.appendChild(
      createElementWithClassName('span', 'device-name'));
  name.textContent = this.session_.name;
  heading.sessionData_ = this.session_;
  deviceDiv.appendChild(heading);

  // Keep track of the drop down that triggered the menu, so we know
  // which element to apply the command to.
  var session = this.session_;
  function handleDropDownFocus(e) {
    DeviceContextMenuController.getInstance().setSession(session);
  }
  heading.addEventListener('contextmenu', handleDropDownFocus);

  var dropDownButton = new cr.ui.ContextMenuButton;
  dropDownButton.tabIndex = 0;
  dropDownButton.classList.add('drop-down');
  dropDownButton.title = loadTimeData.getString('actionMenuDescription');
  dropDownButton.addEventListener('mousedown', function(event) {
      handleDropDownFocus(event);
      // Mousedown handling of cr.ui.MenuButton.handleEvent calls
      // preventDefault, which prevents blur of the focused element. We need to
      // do blur manually.
      document.activeElement.blur();
  });
  dropDownButton.addEventListener('focus', handleDropDownFocus);
  heading.appendChild(dropDownButton);

  var timeSpan = createElementWithClassName('div', 'device-timestamp');
  timeSpan.textContent = this.session_.modifiedTime;
  deviceDiv.appendChild(timeSpan);

  cr.ui.contextMenuHandler.setContextMenu(
      heading, DeviceContextMenuController.getInstance().menu);
  if (!this.session_.collapsed)
    deviceDiv.appendChild(this.createSessionContents_(maxNumTabs));

  return deviceDiv;
};

/**
 * Marks tabs as hidden or not in our session based on the given searchText.
 * @param {string} searchText The search text used to filter the content.
 */
Device.prototype.setSearchText = function(searchText) {
  this.searchText_ = searchText.toLowerCase();
  for (var i = 0; i < this.session_.windows.length; i++) {
    var win = this.session_.windows[i];
    var foundMatch = false;
    for (var j = 0; j < win.tabs.length; j++) {
      var tab = win.tabs[j];
      if (tab.title.toLowerCase().indexOf(this.searchText_) != -1) {
        foundMatch = true;
        tab.hidden = false;
      } else {
        tab.hidden = true;
      }
    }
    win.hidden = !foundMatch;
  }
};

// Device, Private ------------------------------------------------------------

/**
 * Create the DOM tree representing the tabs and windows of this device.
 * @param {int} maxNumTabs The maximum number of tabs to display.
 * @return {Element} A single div containing the list of tabs & windows.
 * @private
 */
Device.prototype.createSessionContents_ = function(maxNumTabs) {
  var contents = createElementWithClassName('ol', 'device-contents');

  var sessionTag = this.session_.tag;
  var numTabsShown = 0;
  var numTabsHidden = 0;
  for (var i = 0; i < this.session_.windows.length; i++) {
    var win = this.session_.windows[i];
    if (win.hidden)
      continue;

    // Show a separator between multiple windows in the same session.
    if (i > 0 && numTabsShown < maxNumTabs)
      contents.appendChild(document.createElement('hr'));

    for (var j = 0; j < win.tabs.length; j++) {
      var tab = win.tabs[j];
      if (tab.hidden)
        continue;

      if (numTabsShown < maxNumTabs) {
        numTabsShown++;
        var a = createElementWithClassName('a', 'device-tab-entry');
        a.href = tab.url;
        a.style.backgroundImage = cr.icon.getFaviconImageSet(tab.url);
        this.addHighlightedText_(a, tab.title);
        // Add a tooltip, since it might be ellipsized. The ones that are not
        // necessary will be removed once added to the document, so we can
        // compute sizes.
        a.title = tab.title;

        // We need to use this to not lose the ids as we go through other loop
        // turns.
        function makeClickHandler(sessionTag, windowId, tabId) {
          return function(e) {
            recordUmaEvent_(HISTOGRAM_EVENT.LINK_CLICKED);
            chrome.send('openForeignSession', [sessionTag, windowId, tabId,
                e.button, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey]);
            e.preventDefault();
          };
        };
        a.addEventListener('click', makeClickHandler(sessionTag,
                                                     String(win.sessionId),
                                                     String(tab.sessionId)));
        var wrapper = createElementWithClassName('div', 'device-tab-wrapper');
        wrapper.appendChild(a);
        contents.appendChild(wrapper);
      } else {
        numTabsHidden++;
      }
    }
  }

  if (numTabsHidden > 0) {
    var moreLink = document.createElement('a', 'action-link');
    moreLink.classList.add('device-show-more-tabs');
    moreLink.addEventListener('click', this.view_.increaseRowHeight.bind(
        this.view_, this.row_, numTabsHidden));
    // TODO(jshin): Use plural message formatter when available in JS.
    moreLink.textContent = loadTimeData.getStringF('xMore',
        numTabsHidden.toLocaleString());
    var moreWrapper = createElementWithClassName('div', 'more-wrapper');
    moreWrapper.appendChild(moreLink);
    contents.appendChild(moreWrapper);
  }

  return contents;
};

/**
 * Add child text nodes to a node such that occurrences of this.searchText_ are
 * highlighted.
 * @param {Node} node The node under which new text nodes will be made as
 *     children.
 * @param {string} content Text to be added beneath |node| as one or more
 *     text nodes.
 * @private
 */
Device.prototype.addHighlightedText_ = function(node, content) {
  var endOfPreviousMatch = 0;
  if (this.searchText_) {
    var lowerContent = content.toLowerCase();
    var searchTextLenght = this.searchText_.length;
    var newMatch = lowerContent.indexOf(this.searchText_, 0);
    while (newMatch != -1) {
      if (newMatch > endOfPreviousMatch) {
        node.appendChild(document.createTextNode(
            content.slice(endOfPreviousMatch, newMatch)));
      }
      endOfPreviousMatch = newMatch + searchTextLenght;
      // Mark the highlighted text in bold.
      var b = document.createElement('b');
      b.textContent = content.substring(newMatch, endOfPreviousMatch);
      node.appendChild(b);
      newMatch = lowerContent.indexOf(this.searchText_, endOfPreviousMatch);
    }
  }
  if (endOfPreviousMatch < content.length) {
    node.appendChild(document.createTextNode(
        content.slice(endOfPreviousMatch)));
  }
};

///////////////////////////////////////////////////////////////////////////////
// DevicesView:

/**
 * Functions and state for populating the page with HTML.
 * @constructor
 */
function DevicesView() {
  this.devices_ = [];  // List of individual devices.
  this.resultDiv_ = $('other-devices');
  this.searchText_ = '';
  this.rowHeights_ = [NB_ENTRIES_FIRST_ROW_COLUMN];
  this.focusGrids_ = [];
  this.updateSignInState(loadTimeData.getBoolean('isUserSignedIn'));
  this.hasSeenForeignData_ = false;
  recordUmaEvent_(HISTOGRAM_EVENT.INITIALIZED);
}

// DevicesView, public: -------------------------------------------------------

/**
 * Updates our sign in state by clearing the view is not signed in or sending
 * a request to get the data to display otherwise.
 * @param {boolean} signedIn Whether the user is signed in or not.
 */
DevicesView.prototype.updateSignInState = function(signedIn) {
  if (signedIn)
    chrome.send('getForeignSessions');
  else
    this.clearDOM();
};

/**
 * Resets the view sessions.
 * @param {Object} sessionList The sessions to add.
 */
DevicesView.prototype.setSessionList = function(sessionList) {
  this.devices_ = [];
  for (var i = 0; i < sessionList.length; i++)
    this.devices_.push(new Device(sessionList[i], this));
  this.displayResults_();

  // This metric should only be emitted if we see foreign data, and it should
  // only be emitted once per page refresh. Flip flag to remember because this
  // method is called upon any update.
  if (!this.hasSeenForeignData_ && sessionList.length > 0) {
    this.hasSeenForeignData_ = true;
    recordUmaEvent_(HISTOGRAM_EVENT.HAS_FOREIGN_DATA);
  }
};


/**
 * Sets the current search text.
 * @param {string} searchText The text to search.
 */
DevicesView.prototype.setSearchText = function(searchText) {
  if (this.searchText_ != searchText) {
    this.searchText_ = searchText;
    for (var i = 0; i < this.devices_.length; i++)
      this.devices_[i].setSearchText(searchText);
    this.displayResults_();
  }
};

/**
 * @return {string} The current search text.
 */
DevicesView.prototype.getSearchText = function() {
  return this.searchText_;
};

/**
 * Clears the DOM content of the view.
 */
DevicesView.prototype.clearDOM = function() {
  while (this.resultDiv_.hasChildNodes()) {
    this.resultDiv_.removeChild(this.resultDiv_.lastChild);
  }
};

/**
 * Increase the height of a row by the given amount.
 * @param {int} row The row number.
 * @param {int} height The extra height to add to the givent row.
 */
DevicesView.prototype.increaseRowHeight = function(row, height) {
  for (var i = this.rowHeights_.length; i <= row; i++)
    this.rowHeights_.push(NB_ENTRIES_OTHER_ROWS_COLUMN);
  this.rowHeights_[row] += height;
  this.displayResults_();
};

// DevicesView, Private -------------------------------------------------------

/**
 * @param {!Element} root
 * @param {?Node} boundary
 * @constructor
 * @extends {cr.ui.FocusRow}
 */
function DevicesViewFocusRow(root, boundary) {
  cr.ui.FocusRow.call(this, root, boundary);
  assert(this.addItem('menu-button', 'button.drop-down') ||
         this.addItem('device-tab', '.device-tab-entry') ||
         this.addItem('more-tabs', '.device-show-more-tabs'));
}

DevicesViewFocusRow.prototype = {__proto__: cr.ui.FocusRow.prototype};

/**
 * Update the page with results.
 * @private
 */
DevicesView.prototype.displayResults_ = function() {
  this.clearDOM();
  var resultsFragment = document.createDocumentFragment();
  if (this.devices_.length == 0)
    return;

  // We'll increase to 0 as we create the first row.
  var rowIndex = -1;
  // We need to access the last row and device when we get out of the loop.
  var currentRowElement;
  // This is only set when changing rows, yet used on all device columns.
  var maxNumTabs;
  for (var i = 0; i < this.devices_.length; i++) {
    var device = this.devices_[i];
    // Should we start a new row?
    if (i % MAX_NUM_COLUMNS == 0) {
      if (currentRowElement)
        resultsFragment.appendChild(currentRowElement);
      currentRowElement = createElementWithClassName('div', 'device-row');
      rowIndex++;
      if (rowIndex < this.rowHeights_.length)
        maxNumTabs = this.rowHeights_[rowIndex];
      else
        maxNumTabs = 0;
    }

    currentRowElement.appendChild(device.getDOMNode(maxNumTabs, rowIndex));
  }
  if (currentRowElement)
    resultsFragment.appendChild(currentRowElement);

  this.resultDiv_.appendChild(resultsFragment);
  // Remove the tootltip on all lines that don't need it. It's easier to
  // remove them here, after adding them all above, since we have the data
  // handy above, but we don't have the width yet. Whereas here, we have the
  // width, and the nodeValue could contain sub nodes for highlighting, which
  // makes it harder to extract the text data here.
  tabs = document.getElementsByClassName('device-tab-entry');
  for (var i = 0; i < tabs.length; i++) {
    if (tabs[i].scrollWidth <= tabs[i].clientWidth)
      tabs[i].title = '';
  }

  this.resultDiv_.appendChild(
      createElementWithClassName('div', 'other-devices-bottom'));

  this.focusGrids_.forEach(function(grid) { grid.destroy(); });
  this.focusGrids_.length = 0;

  var devices = this.resultDiv_.querySelectorAll('.device-contents');
  for (var i = 0; i < devices.length; ++i) {
    var rows = devices[i].querySelectorAll(
        'h3, .device-tab-wrapper, .more-wrapper');
    if (!rows.length)
      continue;

    var grid = new cr.ui.FocusGrid();
    for (var j = 0; j < rows.length; ++j) {
      grid.addRow(new DevicesViewFocusRow(rows[j], devices[i]));
    }
    grid.ensureRowActive();
    this.focusGrids_.push(grid);
  }
};

/**
 * Sets the menu model data. An empty list means that either there are no
 * foreign sessions, or tab sync is disabled for this profile.
 * |isTabSyncEnabled| makes it possible to distinguish between the cases.
 *
 * @param {Array} sessionList Array of objects describing the sessions
 *     from other devices.
 * @param {boolean} isTabSyncEnabled Is tab sync enabled for this profile?
 */
function setForeignSessions(sessionList, isTabSyncEnabled) {
  // The other devices is shown iff tab sync is enabled.
  if (isTabSyncEnabled)
    devicesView.setSessionList(sessionList);
  else
    devicesView.clearDOM();
}

/**
 * Called when initialized or the user's signed in state changes,
 * @param {boolean} isUserSignedIn Is the user currently signed in?
 */
function updateSignInState(isUserSignedIn) {
  if (devicesView)
    devicesView.updateSignInState(isUserSignedIn);
}

///////////////////////////////////////////////////////////////////////////////
// Document Functions:
/**
 * Window onload handler, sets up the other devices view.
 */
function load() {
  if (!loadTimeData.getBoolean('isInstantExtendedApiEnabled'))
    return;

  devicesView = new DevicesView();

  // Create the context menu that appears when the user right clicks
  // on a device name or hit click on the button besides the device name
  document.body.appendChild(DeviceContextMenuController.getInstance().menu);

  var doSearch = function(e) {
    devicesView.setSearchText($('search-field').value);
  };
  $('search-field').addEventListener('search', doSearch);
  $('search-button').addEventListener('click', doSearch);

  chrome.send('otherDevicesInitialized');
}

// Add handlers to HTML elements.
document.addEventListener('DOMContentLoaded', load);
