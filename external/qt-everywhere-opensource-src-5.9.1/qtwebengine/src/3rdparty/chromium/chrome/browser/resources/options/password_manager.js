// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /////////////////////////////////////////////////////////////////////////////
  // PasswordManager class:

  /**
   * Encapsulated handling of password and exceptions page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function PasswordManager() {
    this.activeNavTab = null;
    Page.call(this, 'passwords',
              loadTimeData.getString('passwordsPageTabTitle'),
              'password-manager');
  }

  cr.addSingletonGetter(PasswordManager);

  PasswordManager.prototype = {
    __proto__: Page.prototype,

    /**
     * The saved passwords list.
     * @type {options.DeletableItemList}
     * @private
     */
    savedPasswordsList_: null,

    /**
     * The password exceptions list.
     * @type {options.DeletableItemList}
     * @private
     */
    passwordExceptionsList_: null,

    /**
     * The timer id of the timer set on search query change events.
     * @type {number}
     * @private
     */
    queryDelayTimerId_: 0,

    /**
     * The most recent search query, or null if the query is empty.
     * @type {?string}
     * @private
     */
    lastQuery_: null,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      $('auto-signin-block').hidden =
          !loadTimeData.getBoolean('enableCredentialManagerAPI');

      $('password-manager-confirm').onclick = function() {
        PageManager.closeOverlay();
      };

      $('password-manager-import').onclick = function() {
        chrome.send('importPassword');
      };

      $('password-manager-export').onclick = function() {
        chrome.send('exportPassword');
      };

      $('password-search-box').addEventListener('search',
          this.handleSearchQueryChange_.bind(this));

      $('exceptions-learn-more').onclick = function() {
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_PasswordManagerExceptionsLearnMore']);
        return true;  // Always follow the href
      };

      this.createSavedPasswordsList_();
      this.createPasswordExceptionsList_();
    },

    /** @override */
    canShowPage: function() {
      return !(cr.isChromeOS && UIAccountTweaks.loggedInAsGuest());
    },

    /** @override */
    didShowPage: function() {
      // Updating the password lists may cause a blocking platform dialog pop up
      // (Mac, Linux), so we delay this operation until the page is shown.
      chrome.send('updatePasswordLists');
      $('password-search-box').focus();
    },

    /**
     * Creates, decorates and initializes the saved passwords list.
     * @private
     */
    createSavedPasswordsList_: function() {
      var savedPasswordsList = $('saved-passwords-list');
      options.passwordManager.PasswordsList.decorate(savedPasswordsList);
      this.savedPasswordsList_ = assertInstanceof(savedPasswordsList,
          options.DeletableItemList);
    },

    /**
     * Creates, decorates and initializes the password exceptions list.
     * @private
     */
    createPasswordExceptionsList_: function() {
      var passwordExceptionsList = $('password-exceptions-list');
      options.passwordManager.PasswordExceptionsList.decorate(
          passwordExceptionsList);
      this.passwordExceptionsList_ = assertInstanceof(passwordExceptionsList,
          options.DeletableItemList);
    },

    /**
     * Handles search query changes.
     * @param {!Event} e The event object.
     * @private
     */
    handleSearchQueryChange_: function(e) {
      if (this.queryDelayTimerId_)
        window.clearTimeout(this.queryDelayTimerId_);

      // Searching cookies uses a timeout of 500ms. We use a shorter timeout
      // because there are probably fewer passwords and we want the UI to be
      // snappier since users will expect that it's "less work."
      this.queryDelayTimerId_ = window.setTimeout(
          this.searchPasswords_.bind(this), 250);

      chrome.send('coreOptionsUserMetricsAction',
                  ['Options_PasswordManagerSearch']);
    },

    /**
     * Search passwords using text in |password-search-box|.
     * @private
     */
    searchPasswords_: function() {
      this.queryDelayTimerId_ = 0;
      var filter = $('password-search-box').value;
      filter = (filter == '') ? null : filter;
      if (this.lastQuery_ != filter) {
        this.lastQuery_ = filter;
        // Searching for passwords has the side effect of requerying the
        // underlying password store. This is done intentionally, as on OS X and
        // Linux they can change from outside and we won't be notified of it.
        chrome.send('updatePasswordLists');
      }
    },

    /**
     * Updates the list with the given entries and updates the visibility of the
     * list and empty list placeholder.
     * @param {!cr.ui.List} list The list to toggle visilibility for.
     * @param {!Array} entries The list of entries.
     */
    updateListAndVisibility_: function(list, entries) {
      // Setting the dataModel results in a redraw of the viewport, which is why
      // the visibility needs to be updated first. Otherwise, redraw will not
      // render the updated entries when transitioning from a previously empty
      // list to a non-empty one. The attribute list.hidden would be true in
      // this case, resulting in |redraw()| not adding the new elements to the
      // viewport and thus showing a empty list to the user
      // (http://crbug.com/672869).
      var empty = entries.length == 0;
      var listPlaceHolderID = list.id + '-empty-placeholder';
      list.hidden = empty;
      $(listPlaceHolderID).hidden = !empty;
      list.dataModel = new ArrayDataModel(entries);
    },

    /**
     * Updates the data model for the saved passwords list with the values from
     * |entries|.
     * @param {!Array} entries The list of saved password data.
     */
    setSavedPasswordsList_: function(entries) {
      if (this.lastQuery_) {
        // Implement password searching here in javascript, rather than in C++.
        // The number of saved passwords shouldn't be too big for us to handle.
        var query = this.lastQuery_;
        var filter = function(entry, index, list) {
          // Search both shown URL and username.
          var shownOrigin = entry[options.passwordManager.SHOWN_ORIGIN_FIELD];
          var username = entry[options.passwordManager.USERNAME_FIELD];
          if (shownOrigin.toLowerCase().indexOf(query.toLowerCase()) >= 0 ||
              username.toLowerCase().indexOf(query.toLowerCase()) >= 0) {
            // Keep the original index so we can delete correctly. See also
            // deleteItemAtIndex() in password_manager_list.js that uses this.
            entry[options.passwordManager.ORIGINAL_INDEX_FIELD] = index;
            return true;
          }
          return false;
        };
        entries = entries.filter(filter);
      }
      this.updateListAndVisibility_(assert(this.savedPasswordsList_), entries);
    },

    /**
     * Updates the data model for the password exceptions list with the values
     * from |entries|.
     * @param {!Array} entries The list of password exception data.
     */
    setPasswordExceptionsList_: function(entries) {
      this.updateListAndVisibility_(
          assert(this.passwordExceptionsList_), entries);
    },

    /**
     * Reveals the password for a saved password entry. This is called by the
     * backend after it has authenticated the user.
     * @param {number} index The original index of the entry in the model.
     * @param {string} password The saved password.
     */
    showPassword_: function(index, password) {
      var model = this.savedPasswordsList_.dataModel;
      if (this.lastQuery_) {
        // When a filter is active, |index| does not represent the current
        // index in the model, but each entry stores its original index, so
        // we can find the item using a linear search.
        for (var i = 0; i < model.length; ++i) {
          if (model.item(i)[options.passwordManager.ORIGINAL_INDEX_FIELD] ==
              index) {
            index = i;
            break;
          }
        }
      }

      // Reveal the password in the UI.
      var item = this.savedPasswordsList_.getListItemByIndex(index);
      item.showPassword(password);
    },

    /**
     * @param {boolean} visible Whether the link should be visible.
     * @private
     */
    setManageAccountLinkVisibility_: function(visible) {
      $('manage-passwords-span').hidden = !visible;
    },

    /** @private */
    showImportExportButton_: function() {
      $('password-manager-import-export').hidden = false;
    },
  };

  /**
   * Removes a saved password.
   * @param {number} rowIndex indicating the row to remove.
   */
  PasswordManager.removeSavedPassword = function(rowIndex) {
    chrome.send('removeSavedPassword', [String(rowIndex)]);
    chrome.send('coreOptionsUserMetricsAction',
                ['Options_PasswordManagerDeletePassword']);
  };

  /**
   * Removes a password exception.
   * @param {number} rowIndex indicating the row to remove.
   */
  PasswordManager.removePasswordException = function(rowIndex) {
    chrome.send('removePasswordException', [String(rowIndex)]);
  };

  PasswordManager.requestShowPassword = function(index) {
    chrome.send('requestShowPassword', [index]);
  };

  // Forward public APIs to private implementations on the singleton instance.
  cr.makePublic(PasswordManager, [
    'setSavedPasswordsList',
    'setPasswordExceptionsList',
    'showImportExportButton',
    'showPassword',
  ]);

  // Export
  return {
    PasswordManager: PasswordManager
  };

});
