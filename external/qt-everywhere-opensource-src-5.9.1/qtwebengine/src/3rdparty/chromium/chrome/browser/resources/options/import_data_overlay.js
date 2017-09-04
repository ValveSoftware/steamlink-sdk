// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * ImportDataOverlay class
   * Encapsulated handling of the 'Import Data' overlay page.
   * @class
   */
  function ImportDataOverlay() {
    Page.call(this,
              'importData',
              loadTimeData.getString('importDataOverlayTabTitle'),
              'import-data-overlay');
  }

  cr.addSingletonGetter(ImportDataOverlay);

  /**
  * @param {string} type The type of data to import. Used in the element's ID.
  */
  function importable(type) {
    var id = 'import-' + type;
    return $(id).checked && !$(id + '-with-label').hidden;
  }

  ImportDataOverlay.prototype = {
    // Inherit from Page.
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      var self = this;
      var checkboxes =
          document.querySelectorAll('#import-checkboxes input[type=checkbox]');
      for (var i = 0; i < checkboxes.length; i++) {
        checkboxes[i].customPrefChangeHandler = function(e) {
          options.PrefCheckbox.prototype.defaultPrefChangeHandler.call(this, e);
          self.validateCommitButton_();
          return true;
        };
      }

      $('import-browsers').onchange = function() {
        self.updateCheckboxes_();
        self.validateCommitButton_();
      };

      $('import-data-commit').onclick = function() {
        chrome.send('importData', [
            String($('import-browsers').selectedIndex),
            String($('import-history').checked),
            String($('import-favorites').checked),
            String($('import-passwords').checked),
            String($('import-search').checked),
            String($('import-autofill-form-data').checked)]);
      };

      $('import-data-cancel').onclick = function() {
        ImportDataOverlay.dismiss();
      };

      $('import-choose-file').onclick = function() {
        chrome.send('chooseBookmarksFile');
      };

      $('import-data-confirm').onclick = function() {
        ImportDataOverlay.dismiss();
      };

      // Form controls are disabled until the profile list has been loaded.
      self.setAllControlsEnabled_(false);
    },

    /**
     * Sets the enabled and checked state of the commit button.
     * @private
     */
    validateCommitButton_: function() {
      var somethingToImport =
          importable('history') || importable('favorites') ||
          importable('passwords') || importable('search') ||
          importable('autofill-form-data');
      $('import-data-commit').disabled = !somethingToImport;
      $('import-choose-file').disabled = !$('import-favorites').checked;
    },

    /**
     * Sets the enabled state of all the checkboxes and the commit button.
     * @private
     */
    setAllControlsEnabled_: function(enabled) {
      var checkboxes =
          document.querySelectorAll('#import-checkboxes input[type=checkbox]');
      for (var i = 0; i < checkboxes.length; i++)
        this.setUpCheckboxState_(checkboxes[i], enabled);
      $('import-data-commit').disabled = !enabled;
      $('import-choose-file').hidden = !enabled;
    },

    /**
     * Sets the enabled state of a checkbox element.
     * @param {Object} checkbox A checkbox element.
     * @param {boolean} enabled The enabled state of the checkbox. If false,
     *     the checkbox is disabled. If true, the checkbox is enabled.
     * @private
     */
    setUpCheckboxState_: function(checkbox, enabled) {
      checkbox.setDisabled('noProfileData', !enabled);
    },

    /**
     * Update the enabled and visible states of all the checkboxes.
     * @private
     */
    updateCheckboxes_: function() {
      var index = $('import-browsers').selectedIndex;
      var bookmarksFileSelected = index == this.browserProfiles.length - 1;
      $('import-choose-file').hidden = !bookmarksFileSelected;
      $('import-data-commit').hidden = bookmarksFileSelected;

      var browserProfile;
      if (this.browserProfiles.length > index)
        browserProfile = this.browserProfiles[index];
      var importOptions = ['history',
                           'favorites',
                           'passwords',
                           'search',
                           'autofill-form-data'];
      for (var i = 0; i < importOptions.length; i++) {
        var id = 'import-' + importOptions[i];
        var enable = browserProfile && browserProfile[importOptions[i]];
        this.setUpCheckboxState_($(id), enable);
        $(id + '-with-label').hidden = !enable;
      }
    },

    /**
     * Update the supported browsers popup with given entries.
     * @param {Array} browsers List of supported browsers name.
     * @private
     */
    updateSupportedBrowsers_: function(browsers) {
      this.browserProfiles = browsers;
      var browserSelect = /** @type {HTMLSelectElement} */(
          $('import-browsers'));
      browserSelect.remove(0);  // Remove the 'Loading...' option.
      browserSelect.textContent = '';
      var browserCount = browsers.length;

      if (browserCount == 0) {
        var option = new Option(loadTimeData.getString('noProfileFound'), 0);
        browserSelect.appendChild(option);

        this.setAllControlsEnabled_(false);
      } else {
        this.setAllControlsEnabled_(true);
        for (var i = 0; i < browserCount; i++) {
          var browser = browsers[i];
          var option = new Option(browser.name, browser.index);
          browserSelect.appendChild(option);
        }

        this.updateCheckboxes_();
        this.validateCommitButton_();
      }
    },

    /**
     * Clear import prefs set when user checks/unchecks a checkbox so that each
     * checkbox goes back to the default "checked" state (or alternatively, to
     * the state set by a recommended policy).
     * @private
     */
    clearUserPrefs_: function() {
      var importPrefs = ['import_history',
                         'import_bookmarks',
                         'import_saved_passwords',
                         'import_search_engine',
                         'import_autofill_form_data'];
      for (var i = 0; i < importPrefs.length; i++)
        Preferences.clearPref(importPrefs[i], true);
    },

    /**
     * Update the dialog layout to reflect success state.
     * @param {boolean} success If true, show success dialog elements.
     * @private
     */
    updateSuccessState_: function(success) {
      var sections = document.querySelectorAll('.import-data-configure');
      for (var i = 0; i < sections.length; i++)
        sections[i].hidden = success;

      sections = document.querySelectorAll('.import-data-success');
      for (var i = 0; i < sections.length; i++)
        sections[i].hidden = !success;
    },
  };

  ImportDataOverlay.clearUserPrefs = function() {
    ImportDataOverlay.getInstance().clearUserPrefs_();
  };

  /**
   * Update the supported browsers popup with given entries.
   * @param {Array} browsers List of supported browsers name.
   */
  ImportDataOverlay.updateSupportedBrowsers = function(browsers) {
    ImportDataOverlay.getInstance().updateSupportedBrowsers_(browsers);
  };

  /**
   * Update the UI to reflect whether an import operation is in progress.
   * @param {boolean} importing True if an import operation is in progress.
   */
  ImportDataOverlay.setImportingState = function(importing) {
    var checkboxes =
        document.querySelectorAll('#import-checkboxes input[type=checkbox]');
    for (var i = 0; i < checkboxes.length; i++)
        checkboxes[i].setDisabled('Importing', importing);
    if (!importing)
      ImportDataOverlay.getInstance().updateCheckboxes_();
    $('import-browsers').disabled = importing;
    $('import-throbber').style.visibility = importing ? 'visible' : 'hidden';
    ImportDataOverlay.getInstance().validateCommitButton_();
  };

  /**
   * Remove the import overlay from display.
   */
  ImportDataOverlay.dismiss = function() {
    ImportDataOverlay.clearUserPrefs();
    PageManager.closeOverlay();
  };

  /**
   * Show a message confirming the success of the import operation.
   */
  ImportDataOverlay.confirmSuccess = function() {
    var showBookmarksMessage = $('import-favorites').checked;
    ImportDataOverlay.setImportingState(false);
    $('import-find-your-bookmarks').hidden = !showBookmarksMessage;
    ImportDataOverlay.getInstance().updateSuccessState_(true);
  };

  /**
   * Show the import data overlay.
   */
  ImportDataOverlay.show = function() {
    // Make sure that any previous import success message is hidden, and
    // we're showing the UI to import further data.
    ImportDataOverlay.getInstance().updateSuccessState_(false);
    ImportDataOverlay.getInstance().validateCommitButton_();

    PageManager.showPageByName('importData');
  };

  // Export
  return {
    ImportDataOverlay: ImportDataOverlay
  };
});
