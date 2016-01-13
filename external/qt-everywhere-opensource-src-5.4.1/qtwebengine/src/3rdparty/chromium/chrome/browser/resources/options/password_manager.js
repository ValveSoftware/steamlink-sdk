// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  /////////////////////////////////////////////////////////////////////////////
  // PasswordManager class:

  /**
   * Encapsulated handling of password and exceptions page.
   * @constructor
   */
  function PasswordManager() {
    this.activeNavTab = null;
    OptionsPage.call(this,
                     'passwords',
                     loadTimeData.getString('passwordsPageTabTitle'),
                     'password-manager');
  }

  cr.addSingletonGetter(PasswordManager);

  PasswordManager.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * The saved passwords list.
     * @type {DeletableItemList}
     * @private
     */
    savedPasswordsList_: null,

    /**
     * The password exceptions list.
     * @type {DeletableItemList}
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
      OptionsPage.prototype.initializePage.call(this);

      $('password-manager-confirm').onclick = function() {
        OptionsPage.closeOverlay();
      };

      $('password-search-box').addEventListener('search',
          this.handleSearchQueryChange_.bind(this));

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
      this.savedPasswordsList_ = $('saved-passwords-list');
      options.passwordManager.PasswordsList.decorate(this.savedPasswordsList_);
      this.savedPasswordsList_.autoExpands = true;
    },

    /**
     * Creates, decorates and initializes the password exceptions list.
     * @private
     */
    createPasswordExceptionsList_: function() {
      this.passwordExceptionsList_ = $('password-exceptions-list');
      options.passwordManager.PasswordExceptionsList.decorate(
          this.passwordExceptionsList_);
      this.passwordExceptionsList_.autoExpands = true;
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
     * Updates the visibility of the list and empty list placeholder.
     * @param {!List} list The list to toggle visilibility for.
     */
    updateListVisibility_: function(list) {
      var empty = list.dataModel.length == 0;
      var listPlaceHolderID = list.id + '-empty-placeholder';
      list.hidden = empty;
      $(listPlaceHolderID).hidden = !empty;
    },

    /**
     * Updates the data model for the saved passwords list with the values from
     * |entries|.
     * @param {Array} entries The list of saved password data.
     */
    setSavedPasswordsList_: function(entries) {
      if (this.lastQuery_) {
        // Implement password searching here in javascript, rather than in C++.
        // The number of saved passwords shouldn't be too big for us to handle.
        var query = this.lastQuery_;
        var filter = function(entry, index, list) {
          // Search both URL and username.
          if (entry[0].toLowerCase().indexOf(query.toLowerCase()) >= 0 ||
              entry[1].toLowerCase().indexOf(query.toLowerCase()) >= 0) {
            // Keep the original index so we can delete correctly. See also
            // deleteItemAtIndex() in password_manager_list.js that uses this.
            entry[3] = index;
            return true;
          }
          return false;
        };
        entries = entries.filter(filter);
      }
      this.savedPasswordsList_.dataModel = new ArrayDataModel(entries);
      this.updateListVisibility_(this.savedPasswordsList_);
    },

    /**
     * Updates the data model for the password exceptions list with the values
     * from |entries|.
     * @param {Array} entries The list of password exception data.
     */
    setPasswordExceptionsList_: function(entries) {
      this.passwordExceptionsList_.dataModel = new ArrayDataModel(entries);
      this.updateListVisibility_(this.passwordExceptionsList_);
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
          if (model.item(i)[3] == index) {
            index = i;
            break;
          }
        }
      }

      // Reveal the password in the UI.
      var item = this.savedPasswordsList_.getListItemByIndex(index);
      item.showPassword(password);
    },
  };

  /**
   * Removes a saved password.
   * @param {number} rowIndex indicating the row to remove.
   */
  PasswordManager.removeSavedPassword = function(rowIndex) {
      chrome.send('removeSavedPassword', [String(rowIndex)]);
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
  [
    'setSavedPasswordsList',
    'setPasswordExceptionsList',
    'showPassword'
   ].forEach(function(name) {
     PasswordManager[name] = function() {
      var instance = PasswordManager.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    PasswordManager: PasswordManager
  };

});
