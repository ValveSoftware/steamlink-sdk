// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * ClearBrowserDataOverlay class
   * Encapsulated handling of the 'Clear Browser Data' overlay page.
   * @class
   */
  function ClearBrowserDataOverlay() {
    OptionsPage.call(this, 'clearBrowserData',
                     loadTimeData.getString('clearBrowserDataOverlayTabTitle'),
                     'clear-browser-data-overlay');
  }

  cr.addSingletonGetter(ClearBrowserDataOverlay);

  ClearBrowserDataOverlay.prototype = {
    // Inherit ClearBrowserDataOverlay from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Whether deleting history and downloads is allowed.
     * @type {boolean}
     * @private
     */
    allowDeletingHistory_: true,

    /**
     * Whether or not clearing browsing data is currently in progress.
     * @type {boolean}
     * @private
     */
    isClearingInProgress_: false,

    /**
     * Whether or not the WebUI handler has completed initialization.
     *
     * Unless this becomes true, it must be assumed that the above flags might
     * not contain the authoritative values.
     *
     * @type {boolean}
     * @private
     */
    isInitializationComplete_: false,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var f = this.updateStateOfControls_.bind(this);
      var types = ['browser.clear_data.browsing_history',
                   'browser.clear_data.download_history',
                   'browser.clear_data.cache',
                   'browser.clear_data.cookies',
                   'browser.clear_data.passwords',
                   'browser.clear_data.form_data',
                   'browser.clear_data.hosted_apps_data',
                   'browser.clear_data.content_licenses'];
      types.forEach(function(type) {
          Preferences.getInstance().addEventListener(type, f);
      });

      var checkboxes = document.querySelectorAll(
          '#cbd-content-area input[type=checkbox]');
      for (var i = 0; i < checkboxes.length; i++) {
        checkboxes[i].onclick = f;
      }

      this.createStuffRemainsFooter_();

      $('clear-browser-data-dismiss').onclick = function(event) {
        ClearBrowserDataOverlay.dismiss();
      };
      $('clear-browser-data-commit').onclick = function(event) {
        ClearBrowserDataOverlay.setClearing(true);
        chrome.send('performClearBrowserData');
      };

      // For managed profiles, hide the checkboxes controlling whether or not
      // browsing and download history should be cleared. Note that this is
      // different than just disabling them as a result of enterprise policies.
      if (!loadTimeData.getBoolean('showDeleteBrowsingHistoryCheckboxes')) {
        $('delete-browsing-history-container').hidden = true;
        $('delete-download-history-container').hidden = true;
      }

      this.updateStateOfControls_();
    },

    /**
     * Create a footer that explains that some content is not cleared by the
     * clear browsing history dialog.
     */
    createStuffRemainsFooter_: function() {
      // The localized string is of the form "Saved [content settings] and
      // {search engines} will not be cleared and may reflect your browsing
      // habits.". The following parses out the parts in brackts and braces and
      // converts them into buttons whereas the remainders are represented as
      // span elements.
      var footer =
          document.querySelector('#some-stuff-remains-footer p');
      var footerFragments =
          loadTimeData.getString('contentSettingsAndSearchEnginesRemain')
                      .split(/([|#])/);
      for (var i = 0; i < footerFragments.length;) {
        var buttonId = '';
        if (i + 2 < footerFragments.length) {
          if (footerFragments[i] == '|' && footerFragments[i + 2] == '|') {
            buttonId = 'open-content-settings-from-clear-browsing-data';
          } else if (footerFragments[i] == '#' &&
                     footerFragments[i + 2] == '#') {
            buttonId = 'open-search-engines-from-clear-browsing-data';
          }
        }

        if (buttonId != '') {
          var button = document.createElement('button');
          button.setAttribute('id', buttonId);
          button.setAttribute('class', 'link-button');
          button.textContent = footerFragments[i + 1];
          footer.appendChild(button);
          i += 3;
        } else {
          var span = document.createElement('span');
          span.textContent = footerFragments[i];
          footer.appendChild(span);
          i += 1;
        }
      }
      $('open-content-settings-from-clear-browsing-data').onclick =
          function(event) {
        OptionsPage.navigateToPage('content');
      }
      $('open-search-engines-from-clear-browsing-data').onclick =
          function(event) {
        OptionsPage.navigateToPage('searchEngines');
      }
    },

    /**
     * Sets whether or not we are in the process of clearing data.
     * @param {boolean} clearing Whether the browsing data is currently being
     *     cleared.
     * @private
     */
    setClearing_: function(clearing) {
      this.isClearingInProgress_ = clearing;
      this.updateStateOfControls_();
    },

    /**
     * Sets whether deleting history and downloads is disallowed by enterprise
     * policies. This is called on initialization and in response to a change in
     * the corresponding preference.
     * @param {boolean} allowed Whether to allow deleting history and downloads.
     * @private
     */
    setAllowDeletingHistory_: function(allowed) {
      this.allowDeletingHistory_ = allowed;
      this.updateStateOfControls_();
    },

    /**
     * Called by the WebUI handler to signal that it has finished calling all
     * initialization methods.
     * @private
     */
    markInitializationComplete_: function() {
      this.isInitializationComplete_ = true;
      this.updateStateOfControls_();
    },

    /**
     * Updates the enabled/disabled/hidden status of all controls on the dialog.
     * @private
     */
    updateStateOfControls_: function() {
      // The commit button is enabled if at least one data type selected to be
      // cleared, and if we are not already in the process of clearing.
      // To prevent the commit button from being hazardously enabled for a very
      // short time before setClearing() is called the first time by the native
      // side, also disable the button if |isInitializationComplete_| is false.
      var enabled = false;
      if (this.isInitializationComplete_ && !this.isClearingInProgress_) {
        var checkboxes = document.querySelectorAll(
            '#cbd-content-area input[type=checkbox]');
        for (var i = 0; i < checkboxes.length; i++) {
          if (checkboxes[i].checked) {
            enabled = true;
            break;
          }
        }
      }
      $('clear-browser-data-commit').disabled = !enabled;

      // The checkboxes for clearing history/downloads are enabled unless they
      // are disallowed by policies, or we are in the process of clearing data.
      // To prevent flickering, these, and the rest of the controls can safely
      // be enabled for a short time before the first call to setClearing().
      var enabled = this.allowDeletingHistory_ && !this.isClearingInProgress_;
      $('delete-browsing-history-checkbox').disabled = !enabled;
      $('delete-download-history-checkbox').disabled = !enabled;
      if (!this.allowDeletingHistory_) {
        $('delete-browsing-history-checkbox').checked = false;
        $('delete-download-history-checkbox').checked = false;
      }

      // Enable everything else unless we are in the process of clearing.
      var clearing = this.isClearingInProgress_;
      $('delete-cache-checkbox').disabled = clearing;
      $('delete-cookies-checkbox').disabled = clearing;
      $('delete-passwords-checkbox').disabled = clearing;
      $('delete-form-data-checkbox').disabled = clearing;
      $('delete-hosted-apps-data-checkbox').disabled = clearing;
      $('deauthorize-content-licenses-checkbox').disabled = clearing;
      $('clear-browser-data-time-period').disabled = clearing;
      $('cbd-throbber').style.visibility = clearing ? 'visible' : 'hidden';
      $('clear-browser-data-dismiss').disabled = clearing;
    }
  };

  //
  // Chrome callbacks
  //
  ClearBrowserDataOverlay.setAllowDeletingHistory = function(allowed) {
    ClearBrowserDataOverlay.getInstance().setAllowDeletingHistory_(allowed);
  };

  ClearBrowserDataOverlay.setClearing = function(clearing) {
    ClearBrowserDataOverlay.getInstance().setClearing_(clearing);
  };

  ClearBrowserDataOverlay.markInitializationComplete = function() {
    ClearBrowserDataOverlay.getInstance().markInitializationComplete_();
  };

  ClearBrowserDataOverlay.setBannerVisibility = function(args) {
    var visible = args[0];
    $('clear-browser-data-info-banner').hidden = !visible;
  };

  ClearBrowserDataOverlay.doneClearing = function() {
    // The delay gives the user some feedback that the clearing
    // actually worked. Otherwise the dialog just vanishes instantly in most
    // cases.
    window.setTimeout(function() {
      ClearBrowserDataOverlay.setClearing(false);
      ClearBrowserDataOverlay.dismiss();
    }, 200);
  };

  ClearBrowserDataOverlay.dismiss = function() {
    var topmostVisiblePage = OptionsPage.getTopmostVisiblePage();
    if (topmostVisiblePage && topmostVisiblePage.name == 'clearBrowserData')
      OptionsPage.closeOverlay();
  };

  // Export
  return {
    ClearBrowserDataOverlay: ClearBrowserDataOverlay
  };
});
