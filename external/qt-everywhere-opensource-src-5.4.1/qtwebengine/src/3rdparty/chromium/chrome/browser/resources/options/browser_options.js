// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;
  var RepeatingButton = cr.ui.RepeatingButton;
  var HotwordSearchSettingIndicator = options.HotwordSearchSettingIndicator;

  //
  // BrowserOptions class
  // Encapsulated handling of browser options page.
  //
  function BrowserOptions() {
    OptionsPage.call(this, 'settings', loadTimeData.getString('settingsTitle'),
                     'settings');
  }

  cr.addSingletonGetter(BrowserOptions);

  /**
   * @param {HTMLElement} section The section to show or hide.
   * @return {boolean} Whether the section should be shown.
   * @private
   */
  BrowserOptions.shouldShowSection_ = function(section) {
    // If the section is hidden or hiding, it should be shown.
    return section.style.height == '' || section.style.height == '0px';
  };

  BrowserOptions.prototype = {
    __proto__: options.OptionsPage.prototype,

    /**
     * Keeps track of whether the user is signed in or not.
     * @type {boolean}
     * @private
     */
    signedIn_: false,

    /**
     * Indicates whether signing out is allowed or whether a complete profile
     * wipe is required to remove the current enterprise account.
     * @type {boolean}
     * @private
     */
    signoutAllowed_: true,

    /**
     * Keeps track of whether |onShowHomeButtonChanged_| has been called. See
     * |onShowHomeButtonChanged_|.
     * @type {boolean}
     * @private
     */
    onShowHomeButtonChangedCalled_: false,

    /**
     * Track if page initialization is complete.  All C++ UI handlers have the
     * chance to manipulate page content within their InitializePage methods.
     * This flag is set to true after all initializers have been called.
     * @type {boolean}
     * @private
     */
    initializationComplete_: false,

    /**
     * When a section is waiting to change its height, this will be a number.
     * Otherwise it'll be null.
     * @type {?number}
     * @private
     */
    sectionHeightChangeTimeout_: null,

    /** @override */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      var self = this;

      // Ensure that navigation events are unblocked on uber page. A reload of
      // the settings page while an overlay is open would otherwise leave uber
      // page in a blocked state, where tab switching is not possible.
      uber.invokeMethodOnParent('stopInterceptingEvents');

      window.addEventListener('message', this.handleWindowMessage_.bind(this));

      if (loadTimeData.getBoolean('allowAdvancedSettings')) {
        $('advanced-settings-expander').onclick = function() {
          var showAdvanced =
              BrowserOptions.shouldShowSection_($('advanced-settings'));
          if (showAdvanced) {
            chrome.send('coreOptionsUserMetricsAction',
                        ['Options_ShowAdvancedSettings']);
          }
          self.toggleSectionWithAnimation_(
              $('advanced-settings'),
              $('advanced-settings-container'));

          // If the link was focused (i.e., it was activated using the keyboard)
          // and it was used to show the section (rather than hiding it), focus
          // the first element in the container.
          if (document.activeElement === $('advanced-settings-expander') &&
              showAdvanced) {
            var focusElement = $('advanced-settings-container').querySelector(
                'button, input, list, select, a[href]');
            if (focusElement)
              focusElement.focus();
          }
        };
      } else {
        $('advanced-settings-expander').hidden = true;
        $('advanced-settings').hidden = true;
      }

      $('advanced-settings').addEventListener('webkitTransitionEnd',
          this.updateAdvancedSettingsExpander_.bind(this));

      if (cr.isChromeOS) {
        UIAccountTweaks.applyGuestModeVisibility(document);
        if (loadTimeData.getBoolean('secondaryUser'))
          $('secondary-user-banner').hidden = false;
      }

      // Sync (Sign in) section.
      this.updateSyncState_(loadTimeData.getValue('syncData'));

      $('start-stop-sync').onclick = function(event) {
        if (self.signedIn_) {
          if (self.signoutAllowed_)
            SyncSetupOverlay.showStopSyncingUI();
          else
            chrome.send('showDisconnectManagedProfileDialog');
        } else if (cr.isChromeOS) {
          SyncSetupOverlay.showSetupUI();
        } else {
          SyncSetupOverlay.startSignIn();
        }
      };
      $('customize-sync').onclick = function(event) {
        SyncSetupOverlay.showSetupUI();
      };

      // Internet connection section (ChromeOS only).
      if (cr.isChromeOS) {
        options.network.NetworkList.decorate($('network-list'));
        // Show that the network settings are shared if this is a secondary user
        // in a multi-profile session.
        if (loadTimeData.getBoolean('secondaryUser')) {
          var networkIndicator = document.querySelector(
              '#network-section-header > .controlled-setting-indicator');
          networkIndicator.setAttribute('controlled-by', 'shared');
          networkIndicator.location = cr.ui.ArrowLocation.TOP_START;
        }
        options.network.NetworkList.refreshNetworkData(
            loadTimeData.getValue('networkData'));
      }

      // On Startup section.
      Preferences.getInstance().addEventListener('session.restore_on_startup',
          this.onRestoreOnStartupChanged_.bind(this));
      Preferences.getInstance().addEventListener(
          'session.startup_urls',
          function(event) {
            $('startup-set-pages').disabled = event.value.disabled;
          });

      $('startup-set-pages').onclick = function() {
        OptionsPage.navigateToPage('startup');
      };

      // Appearance section.
      Preferences.getInstance().addEventListener('browser.show_home_button',
          this.onShowHomeButtonChanged_.bind(this));

      Preferences.getInstance().addEventListener('homepage',
          this.onHomePageChanged_.bind(this));
      Preferences.getInstance().addEventListener('homepage_is_newtabpage',
          this.onHomePageIsNtpChanged_.bind(this));

      $('change-home-page').onclick = function(event) {
        OptionsPage.navigateToPage('homePageOverlay');
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_Homepage_ShowSettings']);
      };

      chrome.send('requestHotwordAvailable');
      var hotwordIndicator = $('hotword-search-setting-indicator');
      HotwordSearchSettingIndicator.decorate(hotwordIndicator);
      hotwordIndicator.disabledOnErrorSection = $('hotword-search-enable');

      if ($('set-wallpaper')) {
        $('set-wallpaper').onclick = function(event) {
          chrome.send('openWallpaperManager');
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_OpenWallpaperManager']);
        };
      }

      $('themes-gallery').onclick = function(event) {
        window.open(loadTimeData.getString('themesGalleryURL'));
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_ThemesGallery']);
      };
      $('themes-reset').onclick = function(event) {
        chrome.send('themesReset');
      };

      if (loadTimeData.getBoolean('profileIsManaged')) {
        if ($('themes-native-button')) {
          $('themes-native-button').disabled = true;
          $('themes-native-button').hidden = true;
        }
        // Supervised users have just one default theme, even on Linux. So use
        // the same button for Linux as for the other platforms.
        $('themes-reset').textContent = loadTimeData.getString('themesReset');
      }

      // Device section (ChromeOS only).
      if (cr.isChromeOS) {
        $('keyboard-settings-button').onclick = function(evt) {
          OptionsPage.navigateToPage('keyboard-overlay');
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_ShowKeyboardSettings']);
        };
        $('pointer-settings-button').onclick = function(evt) {
          OptionsPage.navigateToPage('pointer-overlay');
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_ShowTouchpadSettings']);
        };
      }

      // Search section.
      $('manage-default-search-engines').onclick = function(event) {
        OptionsPage.navigateToPage('searchEngines');
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_ManageSearchEngines']);
      };
      $('default-search-engine').addEventListener('change',
          this.setDefaultSearchEngine_);

      // Users section.
      if (loadTimeData.valueExists('profilesInfo')) {
        $('profiles-section').hidden = false;
        this.maybeShowUserSection_();

        var profilesList = $('profiles-list');
        options.browser_options.ProfileList.decorate(profilesList);
        profilesList.autoExpands = true;

        // The profiles info data in |loadTimeData| might be stale.
        this.setProfilesInfo_(loadTimeData.getValue('profilesInfo'));
        chrome.send('requestProfilesInfo');

        profilesList.addEventListener('change',
            this.setProfileViewButtonsStatus_);
        $('profiles-create').onclick = function(event) {
          ManageProfileOverlay.showCreateDialog();
        };
        if (OptionsPage.isSettingsApp()) {
          $('profiles-app-list-switch').onclick = function(event) {
            var selectedProfile = self.getSelectedProfileItem_();
            chrome.send('switchAppListProfile', [selectedProfile.filePath]);
          };
        }
        $('profiles-manage').onclick = function(event) {
          ManageProfileOverlay.showManageDialog();
        };
        $('profiles-delete').onclick = function(event) {
          var selectedProfile = self.getSelectedProfileItem_();
          if (selectedProfile)
            ManageProfileOverlay.showDeleteDialog(selectedProfile);
        };
        if (loadTimeData.getBoolean('profileIsManaged')) {
          $('profiles-create').disabled = true;
          $('profiles-delete').disabled = true;
          $('profiles-list').canDeleteItems = false;
        }
      }

      if (cr.isChromeOS) {
        // Username (canonical email) of the currently logged in user or
        // |kGuestUser| if a guest session is active.
        this.username_ = loadTimeData.getString('username');

        this.updateAccountPicture_();

        $('account-picture').onclick = this.showImagerPickerOverlay_;
        $('change-picture-caption').onclick = this.showImagerPickerOverlay_;

        $('manage-accounts-button').onclick = function(event) {
          OptionsPage.navigateToPage('accounts');
          chrome.send('coreOptionsUserMetricsAction',
              ['Options_ManageAccounts']);
        };

        document.querySelector(
            '#enable-screen-lock + span > .controlled-setting-indicator').
            setAttribute('textshared',
                         loadTimeData.getString('screenLockShared'));
      } else {
        $('import-data').onclick = function(event) {
          ImportDataOverlay.show();
          chrome.send('coreOptionsUserMetricsAction', ['Import_ShowDlg']);
        };

        if ($('themes-native-button')) {
          $('themes-native-button').onclick = function(event) {
            chrome.send('themesSetNative');
          };
        }
      }

      // Date and time section (CrOS only).
      if ($('set-time-button'))
        $('set-time-button').onclick = this.handleSetTime_.bind(this);

      // Default browser section.
      if (!cr.isChromeOS) {
        if (!loadTimeData.getBoolean('showSetDefault')) {
          $('set-default-browser-section').hidden = true;
        }
        $('set-as-default-browser').onclick = function(event) {
          chrome.send('becomeDefaultBrowser');
        };

        $('auto-launch').onclick = this.handleAutoLaunchChanged_;
      }

      // Privacy section.
      $('privacyContentSettingsButton').onclick = function(event) {
        OptionsPage.navigateToPage('content');
        OptionsPage.showTab($('cookies-nav-tab'));
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ContentSettings']);
      };
      $('privacyClearDataButton').onclick = function(event) {
        OptionsPage.navigateToPage('clearBrowserData');
        chrome.send('coreOptionsUserMetricsAction', ['Options_ClearData']);
      };
      $('privacyClearDataButton').hidden = OptionsPage.isSettingsApp();
      // 'metricsReportingEnabled' element is only present on Chrome branded
      // builds, and the 'metricsReportingCheckboxAction' message is only
      // handled on ChromeOS.
      if ($('metricsReportingEnabled') && cr.isChromeOS) {
        $('metricsReportingEnabled').onclick = function(event) {
          chrome.send('metricsReportingCheckboxAction',
              [String(event.currentTarget.checked)]);
        };
      }

      // Bluetooth (CrOS only).
      if (cr.isChromeOS) {
        options.system.bluetooth.BluetoothDeviceList.decorate(
            $('bluetooth-paired-devices-list'));

        $('bluetooth-add-device').onclick =
            this.handleAddBluetoothDevice_.bind(this);

        $('enable-bluetooth').onchange = function(event) {
          var state = $('enable-bluetooth').checked;
          chrome.send('bluetoothEnableChange', [Boolean(state)]);
        };

        $('bluetooth-reconnect-device').onclick = function(event) {
          var device = $('bluetooth-paired-devices-list').selectedItem;
          var address = device.address;
          chrome.send('updateBluetoothDevice', [address, 'connect']);
          OptionsPage.closeOverlay();
        };

        $('bluetooth-paired-devices-list').addEventListener('change',
            function() {
          var item = $('bluetooth-paired-devices-list').selectedItem;
          var disabled = !item || item.connected || !item.connectable;
          $('bluetooth-reconnect-device').disabled = disabled;
        });
      }

      // Passwords and Forms section.
      $('autofill-settings').onclick = function(event) {
        OptionsPage.navigateToPage('autofill');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowAutofillSettings']);
      };
      $('manage-passwords').onclick = function(event) {
        OptionsPage.navigateToPage('passwords');
        OptionsPage.showTab($('passwords-nav-tab'));
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowPasswordManager']);
      };
      if (cr.isChromeOS && UIAccountTweaks.loggedInAsGuest()) {
        // Disable and turn off Autofill in guest mode.
        var autofillEnabled = $('autofill-enabled');
        autofillEnabled.disabled = true;
        autofillEnabled.checked = false;
        cr.dispatchSimpleEvent(autofillEnabled, 'change');
        $('autofill-settings').disabled = true;

        // Disable and turn off Password Manager in guest mode.
        var passwordManagerEnabled = $('password-manager-enabled');
        passwordManagerEnabled.disabled = true;
        passwordManagerEnabled.checked = false;
        cr.dispatchSimpleEvent(passwordManagerEnabled, 'change');
        $('manage-passwords').disabled = true;
      }

      if (cr.isMac) {
        $('mac-passwords-warning').hidden =
            !loadTimeData.getBoolean('multiple_profiles');
      }

      // Network section.
      if (!cr.isChromeOS) {
        $('proxiesConfigureButton').onclick = function(event) {
          chrome.send('showNetworkProxySettings');
        };
      }

      // Security section.
      if (cr.isChromeOS &&
          loadTimeData.getBoolean('consumerManagementEnabled')) {
        $('security-section').hidden = false;
        $('consumer-management-enroll-button').onclick = function(event) {
          chrome.send('enrollConsumerManagement');
        };
      }

      // Easy Unlock section.
      if (loadTimeData.getBoolean('easyUnlockEnabled')) {
        $('easy-unlock-section').hidden = false;
        $('easy-unlock-setup-button').onclick = function(event) {
          chrome.send('launchEasyUnlockSetup');
        };
      }

      // Web Content section.
      $('fontSettingsCustomizeFontsButton').onclick = function(event) {
        OptionsPage.navigateToPage('fonts');
        chrome.send('coreOptionsUserMetricsAction', ['Options_FontSettings']);
      };
      $('defaultFontSize').onchange = function(event) {
        var value = event.target.options[event.target.selectedIndex].value;
        Preferences.setIntegerPref(
             'webkit.webprefs.default_fixed_font_size',
             value - OptionsPage.SIZE_DIFFERENCE_FIXED_STANDARD, true);
        chrome.send('defaultFontSizeAction', [String(value)]);
      };
      $('defaultZoomFactor').onchange = function(event) {
        chrome.send('defaultZoomFactorAction',
            [String(event.target.options[event.target.selectedIndex].value)]);
      };

      // Languages section.
      var showLanguageOptions = function(event) {
        OptionsPage.navigateToPage('languages');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_LanuageAndSpellCheckSettings']);
      };
      $('language-button').onclick = showLanguageOptions;
      $('manage-languages').onclick = showLanguageOptions;

      // Downloads section.
      Preferences.getInstance().addEventListener('download.default_directory',
          this.onDefaultDownloadDirectoryChanged_.bind(this));
      $('downloadLocationChangeButton').onclick = function(event) {
        chrome.send('selectDownloadLocation');
      };
      if (cr.isChromeOS) {
        $('disable-drive-row').hidden =
            UIAccountTweaks.loggedInAsLocallyManagedUser();
      }
      $('autoOpenFileTypesResetToDefault').onclick = function(event) {
        chrome.send('autoOpenFileTypesAction');
      };

      // HTTPS/SSL section.
      if (cr.isWindows || cr.isMac) {
        $('certificatesManageButton').onclick = function(event) {
          chrome.send('showManageSSLCertificates');
        };
      } else {
        $('certificatesManageButton').onclick = function(event) {
          OptionsPage.navigateToPage('certificates');
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_ManageSSLCertificates']);
        };
      }

      if (loadTimeData.getBoolean('cloudPrintShowMDnsOptions')) {
        $('cloudprint-options-mdns').hidden = false;
        $('cloudPrintDevicesPageButton').onclick = function() {
          chrome.send('showCloudPrintDevicesPage');
        };
      }

      // Accessibility section (CrOS only).
      if (cr.isChromeOS) {
        var updateAccessibilitySettingsButton = function() {
          $('accessibility-settings').hidden =
              !($('accessibility-spoken-feedback-check').checked);
        };
        Preferences.getInstance().addEventListener(
            'settings.accessibility',
            updateAccessibilitySettingsButton);
        $('accessibility-learn-more').onclick = function(unused_event) {
          window.open(loadTimeData.getString('accessibilityLearnMoreURL'));
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_AccessibilityLearnMore']);
        };
        $('accessibility-settings-button').onclick = function(unused_event) {
          window.open(loadTimeData.getString('accessibilitySettingsURL'));
        };
        $('accessibility-spoken-feedback-check').onchange = function(
            unused_event) {
          chrome.send('spokenFeedbackChange',
                      [$('accessibility-spoken-feedback-check').checked]);
          updateAccessibilitySettingsButton();
        };
        updateAccessibilitySettingsButton();

        $('accessibility-high-contrast-check').onchange = function(
            unused_event) {
          chrome.send('highContrastChange',
                      [$('accessibility-high-contrast-check').checked]);
        };

        var updateDelayDropdown = function() {
          $('accessibility-autoclick-dropdown').disabled =
              !$('accessibility-autoclick-check').checked;
        };
        Preferences.getInstance().addEventListener(
            $('accessibility-autoclick-check').getAttribute('pref'),
            updateDelayDropdown);
      }

      // Display management section (CrOS only).
      if (cr.isChromeOS) {
        $('display-options').onclick = function(event) {
          OptionsPage.navigateToPage('display');
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_Display']);
        };
      }

      // Factory reset section (CrOS only).
      if (cr.isChromeOS) {
        $('factory-reset-restart').onclick = function(event) {
          OptionsPage.navigateToPage('factoryResetData');
          chrome.send('onPowerwashDialogShow');
        };
      }

      // System section.
      if (!cr.isChromeOS) {
        var updateGpuRestartButton = function() {
          $('gpu-mode-reset-restart').hidden =
              loadTimeData.getBoolean('gpuEnabledAtStart') ==
              $('gpu-mode-checkbox').checked;
        };
        Preferences.getInstance().addEventListener(
            $('gpu-mode-checkbox').getAttribute('pref'),
            updateGpuRestartButton);
        $('gpu-mode-reset-restart-button').onclick = function(event) {
          chrome.send('restartBrowser');
        };
        updateGpuRestartButton();
      }

      // Reset profile settings section.
      $('reset-profile-settings').onclick = function(event) {
        OptionsPage.navigateToPage('resetProfileSettings');
      };
      $('reset-profile-settings-section').hidden =
          !loadTimeData.getBoolean('enableResetProfileSettings');

      // Extension controlled UI.
      this.addExtensionControlledBox_('search-section-content',
                                      'search-engine-controlled',
                                      true);
      this.addExtensionControlledBox_('extension-controlled-container',
                                      'homepage-controlled',
                                      true);
      this.addExtensionControlledBox_('startup-section-content',
                                      'startpage-controlled',
                                      false);
      this.addExtensionControlledBox_('newtab-section-content',
                                      'newtab-controlled',
                                      false);
      this.addExtensionControlledBox_('proxy-section-content',
                                      'proxy-controlled',
                                      true);

      document.body.addEventListener('click', function(e) {
        var button = findAncestor(e.target, function(el) {
          return el.tagName == 'BUTTON' &&
                 el.dataset.extensionId !== undefined &&
                 el.dataset.extensionId.length;
        });
        if (button)
          chrome.send('disableExtension', [button.dataset.extensionId]);
      });
    },

    /** @override */
    didShowPage: function() {
      $('search-field').focus();
    },

   /**
    * Called after all C++ UI handlers have called InitializePage to notify
    * that initialization is complete.
    * @private
    */
    notifyInitializationComplete_: function() {
      this.initializationComplete_ = true;
      cr.dispatchSimpleEvent(document, 'initializationComplete');
    },

    /**
     * Event listener for the 'session.restore_on_startup' pref.
     * @param {Event} event The preference change event.
     * @private
     */
    onRestoreOnStartupChanged_: function(event) {
      /** @const */ var showHomePageValue = 0;

      if (event.value.value == showHomePageValue) {
        // If the user previously selected "Show the homepage", the
        // preference will already be migrated to "Open a specific page". So
        // the only way to reach this code is if the 'restore on startup'
        // preference is managed.
        assert(event.value.controlledBy);

        // Select "open the following pages" and lock down the list of URLs
        // to reflect the intention of the policy.
        $('startup-show-pages').checked = true;
        StartupOverlay.getInstance().setControlsDisabled(true);
      } else {
        // Re-enable the controls in the startup overlay if necessary.
        StartupOverlay.getInstance().updateControlStates();
      }
    },

    /**
     * Handler for messages sent from the main uber page.
     * @param {Event} e The 'message' event from the uber page.
     * @private
     */
    handleWindowMessage_: function(e) {
      if (e.data.method == 'frameSelected')
        $('search-field').focus();
    },

    /**
     * Animatedly changes height |from| a px number |to| a px number.
     * @param {HTMLElement} section The section to animate.
     * @param {HTMLElement} container The container of |section|.
     * @param {boolean} showing Whether to go from 0 -> container height or
     *     container height -> 0.
     * @private
     */
    animatedSectionHeightChange_: function(section, container, showing) {
      // If the section is already animating, dispatch a synthetic transition
      // end event as the upcoming code will cancel the current one.
      if (section.classList.contains('sliding'))
        cr.dispatchSimpleEvent(section, 'webkitTransitionEnd');

      this.addTransitionEndListener_(section);

      section.hidden = false;
      section.style.height = (showing ? 0 : container.offsetHeight) + 'px';
      section.classList.add('sliding');

      if (this.sectionHeightChangeTimeout_ !== null)
        clearTimeout(this.sectionHeightChangeTimeout_);

      this.sectionHeightChangeTimeout_ = setTimeout(function() {
        section.style.height = (showing ? container.offsetHeight : 0) + 'px';
        this.sectionHeightChangeTimeout_ = null;
      });
    },

    /**
     * Shows the given section.
     * @param {HTMLElement} section The section to be shown.
     * @param {HTMLElement} container The container for the section. Must be
     *     inside of |section|.
     * @param {boolean} animate Indicate if the expansion should be animated.
     * @private
     */
    showSection_: function(section, container, animate) {
      // Delay starting the transition if animating so that hidden change will
      // be processed.
      if (animate) {
        this.animatedSectionHeightChange_(section, container, true);
      } else {
        section.hidden = false;
        section.style.height = 'auto';
      }
    },

    /**
     * Shows the given section, with animation.
     * @param {HTMLElement} section The section to be shown.
     * @param {HTMLElement} container The container for the section. Must be
     *     inside of |section|.
     * @private
     */
    showSectionWithAnimation_: function(section, container) {
      this.showSection_(section, container, /* animate */ true);
    },

    /**
     * Hides the given |section| with animation.
     * @param {HTMLElement} section The section to be hidden.
     * @param {HTMLElement} container The container for the section. Must be
     *     inside of |section|.
     * @private
     */
    hideSectionWithAnimation_: function(section, container) {
      this.animatedSectionHeightChange_(section, container, false);
    },

    /**
     * Toggles the visibility of |section| in an animated way.
     * @param {HTMLElement} section The section to be toggled.
     * @param {HTMLElement} container The container for the section. Must be
     *     inside of |section|.
     * @private
     */
    toggleSectionWithAnimation_: function(section, container) {
      if (BrowserOptions.shouldShowSection_(section))
        this.showSectionWithAnimation_(section, container);
      else
        this.hideSectionWithAnimation_(section, container);
    },

    /**
     * Scrolls the settings page to make the section visible auto-expanding
     * advanced settings if required.  The transition is not animated.  This
     * method is used to ensure that a section associated with an overlay
     * is visible when the overlay is closed.
     * @param {!Element} section  The section to make visible.
     * @private
     */
    scrollToSection_: function(section) {
      var advancedSettings = $('advanced-settings');
      var container = $('advanced-settings-container');
      var expander = $('advanced-settings-expander');
      if (!expander.hidden &&
          advancedSettings.hidden &&
          section.parentNode == container) {
        this.showSection_($('advanced-settings'),
                          $('advanced-settings-container'),
                          /* animate */ false);
        this.updateAdvancedSettingsExpander_();
      }

      if (!this.initializationComplete_) {
        var self = this;
        var callback = function() {
           document.removeEventListener('initializationComplete', callback);
           self.scrollToSection_(section);
        };
        document.addEventListener('initializationComplete', callback);
        return;
      }

      var pageContainer = $('page-container');
      // pageContainer.offsetTop is relative to the screen.
      var pageTop = pageContainer.offsetTop;
      var sectionBottom = section.offsetTop + section.offsetHeight;
      // section.offsetTop is relative to the 'page-container'.
      var sectionTop = section.offsetTop;
      if (pageTop + sectionBottom > document.body.scrollHeight ||
          pageTop + sectionTop < 0) {
        // Currently not all layout updates are guaranteed to precede the
        // initializationComplete event (for example 'set-as-default-browser'
        // button) leaving some uncertainty in the optimal scroll position.
        // The section is placed approximately in the middle of the screen.
        var top = Math.min(0, document.body.scrollHeight / 2 - sectionBottom);
        pageContainer.style.top = top + 'px';
        pageContainer.oldScrollTop = -top;
      }
    },

    /**
     * Adds a |webkitTransitionEnd| listener to the given section so that
     * it can be animated. The listener will only be added to a given section
     * once, so this can be called as multiple times.
     * @param {HTMLElement} section The section to be animated.
     * @private
     */
    addTransitionEndListener_: function(section) {
      if (section.hasTransitionEndListener_)
        return;

      section.addEventListener('webkitTransitionEnd',
          this.onTransitionEnd_.bind(this));
      section.hasTransitionEndListener_ = true;
    },

    /**
     * Called after an animation transition has ended.
     * @param {Event} The webkitTransitionEnd event. NOTE: May be synthetic.
     * @private
     */
    onTransitionEnd_: function(event) {
      if (event.propertyName && event.propertyName != 'height') {
        // If not a synthetic event or a real transition we care about, bail.
        return;
      }

      var section = event.target;
      section.classList.remove('sliding');

      if (!event.propertyName) {
        // Only real transitions past this point.
        return;
      }

      if (section.style.height == '0px') {
        // Hide the content so it can't get tab focus.
        section.hidden = true;
        section.style.height = '';
      } else {
        // Set the section height to 'auto' to allow for size changes
        // (due to font change or dynamic content).
        section.style.height = 'auto';
      }
    },

    /** @private */
    updateAdvancedSettingsExpander_: function() {
      var expander = $('advanced-settings-expander');
      if (BrowserOptions.shouldShowSection_($('advanced-settings')))
        expander.textContent = loadTimeData.getString('showAdvancedSettings');
      else
        expander.textContent = loadTimeData.getString('hideAdvancedSettings');
    },

    /**
     * Updates the sync section with the given state.
     * @param {Object} syncData A bunch of data records that describe the status
     *     of the sync system.
     * @private
     */
    updateSyncState_: function(syncData) {
      if (!syncData.signinAllowed &&
          (!syncData.supervisedUser || !cr.isChromeOS)) {
        $('sync-section').hidden = true;
        this.maybeShowUserSection_();
        return;
      }

      $('sync-section').hidden = false;
      this.maybeShowUserSection_();

      if (cr.isChromeOS && syncData.supervisedUser) {
        var subSection = $('sync-section').firstChild;
        while (subSection) {
          if (subSection.nodeType == Node.ELEMENT_NODE)
            subSection.hidden = true;
          subSection = subSection.nextSibling;
        }

        $('account-picture-wrapper').hidden = false;
        $('sync-general').hidden = false;
        $('sync-status').hidden = true;

        return;
      }

      // If the user gets signed out while the advanced sync settings dialog is
      // visible, say, due to a dashboard clear, close the dialog.
      // However, if the user gets signed out as a result of abandoning first
      // time sync setup, do not call closeOverlay as it will redirect the
      // browser to the main settings page and override any in-progress
      // user-initiated navigation. See crbug.com/278030.
      // Note: SyncSetupOverlay.closeOverlay is a no-op if the overlay is
      // already hidden.
      if (this.signedIn_ && !syncData.signedIn && !syncData.setupInProgress)
        SyncSetupOverlay.closeOverlay();

      this.signedIn_ = syncData.signedIn;

      // Display the "advanced settings" button if we're signed in and sync is
      // not managed/disabled. If the user is signed in, but sync is disabled,
      // this button is used to re-enable sync.
      var customizeSyncButton = $('customize-sync');
      customizeSyncButton.hidden = !this.signedIn_ ||
                                   syncData.managed ||
                                   !syncData.syncSystemEnabled;

      // Only modify the customize button's text if the new text is different.
      // Otherwise, it can affect search-highlighting in the settings page.
      // See http://crbug.com/268265.
      var customizeSyncButtonNewText = syncData.setupCompleted ?
          loadTimeData.getString('customizeSync') :
          loadTimeData.getString('syncButtonTextStart');
      if (customizeSyncButton.textContent != customizeSyncButtonNewText)
        customizeSyncButton.textContent = customizeSyncButtonNewText;

      // Disable the "sign in" button if we're currently signing in, or if we're
      // already signed in and signout is not allowed.
      var signInButton = $('start-stop-sync');
      signInButton.disabled = syncData.setupInProgress;
      this.signoutAllowed_ = syncData.signoutAllowed;
      if (!syncData.signoutAllowed)
        $('start-stop-sync-indicator').setAttribute('controlled-by', 'policy');
      else
        $('start-stop-sync-indicator').removeAttribute('controlled-by');

      // Hide the "sign in" button on Chrome OS, and show it on desktop Chrome.
      signInButton.hidden = cr.isChromeOS;

      signInButton.textContent =
          this.signedIn_ ?
              loadTimeData.getString('syncButtonTextStop') :
              syncData.setupInProgress ?
                  loadTimeData.getString('syncButtonTextInProgress') :
                  loadTimeData.getString('syncButtonTextSignIn');
      $('start-stop-sync-indicator').hidden = signInButton.hidden;

      // TODO(estade): can this just be textContent?
      $('sync-status-text').innerHTML = syncData.statusText;
      var statusSet = syncData.statusText.length != 0;
      $('sync-overview').hidden = statusSet;
      $('sync-status').hidden = !statusSet;

      $('sync-action-link').textContent = syncData.actionLinkText;
      // Don't show the action link if it is empty or undefined.
      $('sync-action-link').hidden = syncData.actionLinkText.length == 0;
      $('sync-action-link').disabled = syncData.managed ||
                                       !syncData.syncSystemEnabled;

      // On Chrome OS, sign out the user and sign in again to get fresh
      // credentials on auth errors.
      $('sync-action-link').onclick = function(event) {
        if (cr.isChromeOS && syncData.hasError)
          SyncSetupOverlay.doSignOutOnAuthError();
        else
          SyncSetupOverlay.showSetupUI();
      };

      if (syncData.hasError)
        $('sync-status').classList.add('sync-error');
      else
        $('sync-status').classList.remove('sync-error');

      // Disable the "customize / set up sync" button if sync has an
      // unrecoverable error. Also disable the button if sync has not been set
      // up and the user is being presented with a link to re-auth.
      // See crbug.com/289791.
      customizeSyncButton.disabled =
          syncData.hasUnrecoverableError ||
          (!syncData.setupCompleted && !$('sync-action-link').hidden);
    },

    /**
     * Update the UI depending on whether the current profile has a pairing for
     * Easy Unlock.
     * @param {boolean} hasPairing True if the current profile has a pairing.
     */
    updateEasyUnlock_: function(hasPairing) {
      $('easy-unlock-setup').hidden = hasPairing;
      $('easy-unlock-enable').hidden = !hasPairing;
    },

    /**
     * Update the UI depending on whether the current profile manages any
     * supervised users.
     * @param {boolean} show True if the current profile manages any supervised
     *     users.
     */
    updateManagesSupervisedUsers_: function(show) {
      $('profiles-supervised-dashboard-tip').hidden = !show;
      this.maybeShowUserSection_();
    },

    /**
     * Get the start/stop sync button DOM element. Used for testing.
     * @return {DOMElement} The start/stop sync button.
     * @private
     */
    getStartStopSyncButton_: function() {
      return $('start-stop-sync');
    },

    /**
     * Event listener for the 'show home button' preference. Shows/hides the
     * UI for changing the home page with animation, unless this is the first
     * time this function is called, in which case there is no animation.
     * @param {Event} event The preference change event.
     */
    onShowHomeButtonChanged_: function(event) {
      var section = $('change-home-page-section');
      if (this.onShowHomeButtonChangedCalled_) {
        var container = $('change-home-page-section-container');
        if (event.value.value)
          this.showSectionWithAnimation_(section, container);
        else
          this.hideSectionWithAnimation_(section, container);
      } else {
        section.hidden = !event.value.value;
        this.onShowHomeButtonChangedCalled_ = true;
      }
    },

    /**
     * Activates the Hotword section from the System settings page.
     * @param {string} opt_error The error message to display.
     * @param {string} opt_help_link The link to a troubleshooting page.
     * @private
     */
    showHotwordSection_: function(opt_error, opt_help_link) {
      $('hotword-search').hidden = false;
      $('hotword-search-setting-indicator').errorText = opt_error;
      $('hotword-search-setting-indicator').helpLink = opt_help_link;
    },

    /**
     * Event listener for the 'homepage is NTP' preference. Updates the label
     * next to the 'Change' button.
     * @param {Event} event The preference change event.
     */
    onHomePageIsNtpChanged_: function(event) {
      if (!event.value.uncommitted) {
        $('home-page-url').hidden = event.value.value;
        $('home-page-ntp').hidden = !event.value.value;
      }
    },

    /**
     * Event listener for changes to the homepage preference. Updates the label
     * next to the 'Change' button.
     * @param {Event} event The preference change event.
     */
    onHomePageChanged_: function(event) {
      if (!event.value.uncommitted)
        $('home-page-url').textContent = this.stripHttp_(event.value.value);
    },

    /**
     * Removes the 'http://' from a URL, like the omnibox does. If the string
     * doesn't start with 'http://' it is returned unchanged.
     * @param {string} url The url to be processed
     * @return {string} The url with the 'http://' removed.
     */
    stripHttp_: function(url) {
      return url.replace(/^http:\/\//, '');
    },

   /**
    * Shows the autoLaunch preference and initializes its checkbox value.
    * @param {bool} enabled Whether autolaunch is enabled or or not.
    * @private
    */
    updateAutoLaunchState_: function(enabled) {
      $('auto-launch-option').hidden = false;
      $('auto-launch').checked = enabled;
    },

    /**
     * Called when the value of the download.default_directory preference
     * changes.
     * @param {Event} event Change event.
     * @private
     */
    onDefaultDownloadDirectoryChanged_: function(event) {
      $('downloadLocationPath').value = event.value.value;
      if (cr.isChromeOS) {
        // On ChromeOS, replace /special/drive-<hash>/root with "Google Drive"
        // for remote files, /home/chronos/user/Downloads or
        // /home/chronos/u-<hash>/Downloads with "Downloads" for local paths,
        // and '/' with ' \u203a ' (angled quote sign) everywhere. The modified
        // path is used only for display purpose.
        var path = $('downloadLocationPath').value;
        path = path.replace(/^\/special\/drive[^\/]*\/root/, 'Google Drive');
        path = path.replace(/^\/home\/chronos\/(user|u-[^\/]*)\//, '');
        path = path.replace(/\//g, ' \u203a ');
        $('downloadLocationPath').value = path;
      }
      $('download-location-label').classList.toggle('disabled',
                                                    event.value.disabled);
      $('downloadLocationChangeButton').disabled = event.value.disabled;
    },

    /**
     * Update the Default Browsers section based on the current state.
     * @param {string} statusString Description of the current default state.
     * @param {boolean} isDefault Whether or not the browser is currently
     *     default.
     * @param {boolean} canBeDefault Whether or not the browser can be default.
     * @private
     */
    updateDefaultBrowserState_: function(statusString, isDefault,
                                         canBeDefault) {
      if (!cr.isChromeOS) {
        var label = $('default-browser-state');
        label.textContent = statusString;

        $('set-as-default-browser').hidden = !canBeDefault || isDefault;
      }
    },

    /**
     * Clears the search engine popup.
     * @private
     */
    clearSearchEngines_: function() {
      $('default-search-engine').textContent = '';
    },

    /**
     * Updates the search engine popup with the given entries.
     * @param {Array} engines List of available search engines.
     * @param {number} defaultValue The value of the current default engine.
     * @param {boolean} defaultManaged Whether the default search provider is
     *     managed. If true, the default search provider can't be changed.
     * @private
     */
    updateSearchEngines_: function(engines, defaultValue, defaultManaged) {
      this.clearSearchEngines_();
      engineSelect = $('default-search-engine');
      engineSelect.disabled = defaultManaged;
      if (defaultManaged && defaultValue == -1)
        return;
      engineCount = engines.length;
      var defaultIndex = -1;
      for (var i = 0; i < engineCount; i++) {
        var engine = engines[i];
        var option = new Option(engine.name, engine.index);
        if (defaultValue == option.value)
          defaultIndex = i;
        engineSelect.appendChild(option);
      }
      if (defaultIndex >= 0)
        engineSelect.selectedIndex = defaultIndex;
    },

    /**
     * Set the default search engine based on the popup selection.
     * @private
     */
    setDefaultSearchEngine_: function() {
      var engineSelect = $('default-search-engine');
      var selectedIndex = engineSelect.selectedIndex;
      if (selectedIndex >= 0) {
        var selection = engineSelect.options[selectedIndex];
        chrome.send('setDefaultSearchEngine', [String(selection.value)]);
      }
    },

   /**
     * Sets or clear whether Chrome should Auto-launch on computer startup.
     * @private
     */
    handleAutoLaunchChanged_: function() {
      chrome.send('toggleAutoLaunch', [$('auto-launch').checked]);
    },

    /**
     * Get the selected profile item from the profile list. This also works
     * correctly if the list is not displayed.
     * @return {Object} the profile item object, or null if nothing is selected.
     * @private
     */
    getSelectedProfileItem_: function() {
      var profilesList = $('profiles-list');
      if (profilesList.hidden) {
        if (profilesList.dataModel.length > 0)
          return profilesList.dataModel.item(0);
      } else {
        return profilesList.selectedItem;
      }
      return null;
    },

    /**
     * Helper function to set the status of profile view buttons to disabled or
     * enabled, depending on the number of profiles and selection status of the
     * profiles list.
     * @private
     */
    setProfileViewButtonsStatus_: function() {
      var profilesList = $('profiles-list');
      var selectedProfile = profilesList.selectedItem;
      var hasSelection = selectedProfile != null;
      var hasSingleProfile = profilesList.dataModel.length == 1;
      var isManaged = loadTimeData.getBoolean('profileIsManaged');
      $('profiles-manage').disabled = !hasSelection ||
          !selectedProfile.isCurrentProfile;
      if (hasSelection && !selectedProfile.isCurrentProfile)
        $('profiles-manage').title = loadTimeData.getString('currentUserOnly');
      else
        $('profiles-manage').title = '';
      $('profiles-delete').disabled = isManaged ||
                                      (!hasSelection && !hasSingleProfile);
      if (OptionsPage.isSettingsApp()) {
        $('profiles-app-list-switch').disabled = !hasSelection ||
            selectedProfile.isCurrentProfile;
      }
      var importData = $('import-data');
      if (importData) {
        importData.disabled = $('import-data').disabled = hasSelection &&
          !selectedProfile.isCurrentProfile;
      }
    },

    /**
     * Display the correct dialog layout, depending on how many profiles are
     * available.
     * @param {number} numProfiles The number of profiles to display.
     * @private
     */
    setProfileViewSingle_: function(numProfiles) {
      var hasSingleProfile = numProfiles == 1;
      $('profiles-list').hidden = hasSingleProfile;
      $('profiles-single-message').hidden = !hasSingleProfile;
      $('profiles-manage').hidden =
          hasSingleProfile || OptionsPage.isSettingsApp();
      $('profiles-delete').textContent = hasSingleProfile ?
          loadTimeData.getString('profilesDeleteSingle') :
          loadTimeData.getString('profilesDelete');
      if (OptionsPage.isSettingsApp())
        $('profiles-app-list-switch').hidden = hasSingleProfile;
    },

    /**
     * Adds all |profiles| to the list.
     * @param {Array.<Object>} profiles An array of profile info objects.
     *     each object is of the form:
     *       profileInfo = {
     *         name: "Profile Name",
     *         iconURL: "chrome://path/to/icon/image",
     *         filePath: "/path/to/profile/data/on/disk",
     *         isCurrentProfile: false,
     *         isManaged: false
     *       };
     * @private
     */
    setProfilesInfo_: function(profiles) {
      this.setProfileViewSingle_(profiles.length);
      // add it to the list, even if the list is hidden so we can access it
      // later.
      $('profiles-list').dataModel = new ArrayDataModel(profiles);

      // Received new data. If showing the "manage" overlay, keep it up to
      // date. If showing the "delete" overlay, close it.
      if (ManageProfileOverlay.getInstance().visible &&
          !$('manage-profile-overlay-manage').hidden) {
        ManageProfileOverlay.showManageDialog();
      } else {
        ManageProfileOverlay.getInstance().visible = false;
      }

      this.setProfileViewButtonsStatus_();
    },

    /**
     * Reports managed user import errors to the ManagedUserImportOverlay.
     * @param {string} error The error message to display.
     * @private
     */
    showManagedUserImportError_: function(error) {
      ManagedUserImportOverlay.onError(error);
    },

    /**
     * Reports successful importing of a managed user to
     * the ManagedUserImportOverlay.
     * @private
     */
    showManagedUserImportSuccess_: function() {
      ManagedUserImportOverlay.onSuccess();
    },

    /**
     * Reports an error to the "create" overlay during profile creation.
     * @param {string} error The error message to display.
     * @private
     */
    showCreateProfileError_: function(error) {
      CreateProfileOverlay.onError(error);
    },

    /**
    * Sends a warning message to the "create" overlay during profile creation.
    * @param {string} warning The warning message to display.
    * @private
    */
    showCreateProfileWarning_: function(warning) {
      CreateProfileOverlay.onWarning(warning);
    },

    /**
    * Reports successful profile creation to the "create" overlay.
     * @param {Object} profileInfo An object of the form:
     *     profileInfo = {
     *       name: "Profile Name",
     *       filePath: "/path/to/profile/data/on/disk"
     *       isManaged: (true|false),
     *     };
    * @private
    */
    showCreateProfileSuccess_: function(profileInfo) {
      CreateProfileOverlay.onSuccess(profileInfo);
    },

    /**
     * Returns the currently active profile for this browser window.
     * @return {Object} A profile info object.
     * @private
     */
    getCurrentProfile_: function() {
      for (var i = 0; i < $('profiles-list').dataModel.length; i++) {
        var profile = $('profiles-list').dataModel.item(i);
        if (profile.isCurrentProfile)
          return profile;
      }

      assert(false,
             'There should always be a current profile, but none found.');
    },

    /**
     * Propmpts user to confirm deletion of the profile for this browser
     * window.
     * @private
     */
    deleteCurrentProfile_: function() {
      ManageProfileOverlay.showDeleteDialog(this.getCurrentProfile_());
    },

    setNativeThemeButtonEnabled_: function(enabled) {
      var button = $('themes-native-button');
      if (button)
        button.disabled = !enabled;
    },

    setThemesResetButtonEnabled_: function(enabled) {
      $('themes-reset').disabled = !enabled;
    },

    setAccountPictureManaged_: function(managed) {
      var picture = $('account-picture');
      if (managed || UIAccountTweaks.loggedInAsGuest()) {
        picture.disabled = true;
        ChangePictureOptions.closeOverlay();
      } else {
        picture.disabled = false;
      }

      // Create a synthetic pref change event decorated as
      // CoreOptionsHandler::CreateValueForPref() does.
      var event = new Event('account-picture');
      if (managed)
        event.value = { controlledBy: 'policy' };
      else
        event.value = {};
      $('account-picture-indicator').handlePrefChange(event);
    },

    /**
     * (Re)loads IMG element with current user account picture.
     * @private
     */
    updateAccountPicture_: function() {
      var picture = $('account-picture');
      if (picture) {
        picture.src = 'chrome://userimage/' + this.username_ + '?id=' +
            Date.now();
      }
    },

    setWallpaperManaged_: function(managed) {
      var button = $('set-wallpaper');
      button.disabled = !!managed;

      // Create a synthetic pref change event decorated as
      // CoreOptionsHandler::CreateValueForPref() does.
      var event = new Event('wallpaper');
      if (managed)
        event.value = { controlledBy: 'policy' };
      else
        event.value = {};
      $('wallpaper-indicator').handlePrefChange(event);
    },

    /**
     * Handle the 'add device' button click.
     * @private
     */
    handleAddBluetoothDevice_: function() {
      chrome.send('findBluetoothDevices');
      OptionsPage.showPageByName('bluetooth', false);
    },

    /**
     * Enables or disables the Manage SSL Certificates button.
     * @private
     */
    enableCertificateButton_: function(enabled) {
      $('certificatesManageButton').disabled = !enabled;
    },

    /**
     * Enables factory reset section.
     * @private
     */
    enableFactoryResetSection_: function() {
      $('factory-reset-section').hidden = false;
    },

    /**
     * Set the checked state of the metrics reporting checkbox.
     * @private
     */
    setMetricsReportingCheckboxState_: function(checked, disabled) {
      $('metricsReportingEnabled').checked = checked;
      $('metricsReportingEnabled').disabled = disabled;
    },

    /**
     * @private
     */
    setMetricsReportingSettingVisibility_: function(visible) {
      if (visible)
        $('metricsReportingSetting').style.display = 'block';
      else
        $('metricsReportingSetting').style.display = 'none';
    },

    /**
     * Set the font size selected item. This item actually reflects two
     * preferences: the default font size and the default fixed font size.
     *
     * @param {Object} pref Information about the font size preferences.
     * @param {number} pref.value The value of the default font size pref.
     * @param {boolean} pref.disabled True if either pref not user modifiable.
     * @param {string} pref.controlledBy The source of the pref value(s) if
     *     either pref is currently not controlled by the user.
     * @private
     */
    setFontSize_: function(pref) {
      var selectCtl = $('defaultFontSize');
      selectCtl.disabled = pref.disabled;
      // Create a synthetic pref change event decorated as
      // CoreOptionsHandler::CreateValueForPref() does.
      var event = new Event('synthetic-font-size');
      event.value = {
        value: pref.value,
        controlledBy: pref.controlledBy,
        disabled: pref.disabled
      };
      $('font-size-indicator').handlePrefChange(event);

      for (var i = 0; i < selectCtl.options.length; i++) {
        if (selectCtl.options[i].value == pref.value) {
          selectCtl.selectedIndex = i;
          if ($('Custom'))
            selectCtl.remove($('Custom').index);
          return;
        }
      }

      // Add/Select Custom Option in the font size label list.
      if (!$('Custom')) {
        var option = new Option(loadTimeData.getString('fontSizeLabelCustom'),
                                -1, false, true);
        option.setAttribute('id', 'Custom');
        selectCtl.add(option);
      }
      $('Custom').selected = true;
    },

    /**
     * Populate the page zoom selector with values received from the caller.
     * @param {Array} items An array of items to populate the selector.
     *     each object is an array with three elements as follows:
     *       0: The title of the item (string).
     *       1: The value of the item (number).
     *       2: Whether the item should be selected (boolean).
     * @private
     */
    setupPageZoomSelector_: function(items) {
      var element = $('defaultZoomFactor');

      // Remove any existing content.
      element.textContent = '';

      // Insert new child nodes into select element.
      var value, title, selected;
      for (var i = 0; i < items.length; i++) {
        title = items[i][0];
        value = items[i][1];
        selected = items[i][2];
        element.appendChild(new Option(title, value, false, selected));
      }
    },

    /**
     * Shows/hides the autoOpenFileTypesResetToDefault button and label, with
     * animation.
     * @param {boolean} display Whether to show the button and label or not.
     * @private
     */
    setAutoOpenFileTypesDisplayed_: function(display) {
      if ($('advanced-settings').hidden) {
        // If the Advanced section is hidden, don't animate the transition.
        $('auto-open-file-types-section').hidden = !display;
      } else {
        if (display) {
          this.showSectionWithAnimation_(
              $('auto-open-file-types-section'),
              $('auto-open-file-types-container'));
        } else {
          this.hideSectionWithAnimation_(
              $('auto-open-file-types-section'),
              $('auto-open-file-types-container'));
        }
      }
    },

    /**
     * Set the enabled state for the proxy settings button and its associated
     * message when extension controlled.
     * @param {boolean} disabled Whether the button should be disabled.
     * @param {boolean} extensionControlled Whether the proxy is extension
     *     controlled.
     * @private
     */
    setupProxySettingsButton_: function(disabled, extensionControlled) {
      if (!cr.isChromeOS) {
        $('proxiesConfigureButton').disabled = disabled;
        $('proxiesLabel').textContent =
            loadTimeData.getString(extensionControlled ?
                'proxiesLabelExtension' : 'proxiesLabelSystem');
      }
    },

    /**
     * Set the initial state of the spoken feedback checkbox.
     * @private
     */
    setSpokenFeedbackCheckboxState_: function(checked) {
      $('accessibility-spoken-feedback-check').checked = checked;
    },

    /**
     * Set the initial state of the high contrast checkbox.
     * @private
     */
    setHighContrastCheckboxState_: function(checked) {
      $('accessibility-high-contrast-check').checked = checked;
    },

    /**
     * Set the initial state of the virtual keyboard checkbox.
     * @private
     */
    setVirtualKeyboardCheckboxState_: function(checked) {
      // TODO(zork): Update UI
    },

    /**
     * Show/hide mouse settings slider.
     * @private
     */
    showMouseControls_: function(show) {
      $('mouse-settings').hidden = !show;
    },

    /**
     * Adds hidden warning boxes for settings potentially controlled by
     * extensions.
     * @param {string} parentDiv The div name to append the bubble to.
     * @param {string} bubbleId The ID to use for the bubble.
     * @param {boolean} first Add as first node if true, otherwise last.
     * @private
     */
    addExtensionControlledBox_: function(parentDiv, bubbleId, first) {
      var bubble = $('extension-controlled-warning-template').cloneNode(true);
      bubble.id = bubbleId;
      var parent = $(parentDiv);
      if (first)
        parent.insertBefore(bubble, parent.firstChild);
      else
        parent.appendChild(bubble);
    },

    /**
     * Adds a bubble showing that an extension is controlling a particular
     * setting.
     * @param {string} parentDiv The div name to append the bubble to.
     * @param {string} bubbleId The ID to use for the bubble.
     * @param {string} extensionId The ID of the controlling extension.
     * @param {string} extensionName The name of the controlling extension.
     * @private
     */
    toggleExtensionControlledBox_: function(
        parentDiv, bubbleId, extensionId, extensionName) {
      var bubble = $(bubbleId);
      assert(bubble);
      bubble.hidden = extensionId.length == 0;
      if (bubble.hidden)
        return;

      // Set the extension image.
      var div = bubble.firstElementChild;
      div.style.backgroundImage =
          'url(chrome://extension-icon/' + extensionId + '/24/1)';

      // Set the bubble label.
      var label = loadTimeData.getStringF('extensionControlled', extensionName);
      var docFrag = parseHtmlSubset('<div>' + label + '</div>', ['B', 'DIV']);
      div.innerHTML = docFrag.firstChild.innerHTML;

      // Wire up the button to disable the right extension.
      var button = div.nextElementSibling;
      button.dataset.extensionId = extensionId;
    },

    /**
     * Toggles the warning boxes that show which extension is controlling
     * various settings of Chrome.
     * @param {object} details A dictionary of ID+name pairs for each of the
     *     settings controlled by an extension.
     * @private
     */
    toggleExtensionIndicators_: function(details) {
      this.toggleExtensionControlledBox_('search-section-content',
                                         'search-engine-controlled',
                                         details.searchEngine.id,
                                         details.searchEngine.name);
      this.toggleExtensionControlledBox_('extension-controlled-container',
                                         'homepage-controlled',
                                         details.homePage.id,
                                         details.homePage.name);
      this.toggleExtensionControlledBox_('startup-section-content',
                                         'startpage-controlled',
                                         details.startUpPage.id,
                                         details.startUpPage.name);
      this.toggleExtensionControlledBox_('newtab-section-content',
                                         'newtab-controlled',
                                         details.newTabPage.id,
                                         details.newTabPage.name);
      this.toggleExtensionControlledBox_('proxy-section-content',
                                         'proxy-controlled',
                                         details.proxy.id,
                                         details.proxy.name);

      // The proxy section contains just the warning box and nothing else, so
      // if we're hiding the proxy warning box, we should also hide its header
      // section.
      $('proxy-section').hidden = details.proxy.id.length == 0;
    },


    /**
     * Show/hide touchpad-related settings.
     * @private
     */
    showTouchpadControls_: function(show) {
      $('touchpad-settings').hidden = !show;
      $('accessibility-tap-dragging').hidden = !show;
    },

    /**
     * Activate the Bluetooth settings section on the System settings page.
     * @private
     */
    showBluetoothSettings_: function() {
      $('bluetooth-devices').hidden = false;
    },

    /**
     * Dectivates the Bluetooth settings section from the System settings page.
     * @private
     */
    hideBluetoothSettings_: function() {
      $('bluetooth-devices').hidden = true;
    },

    /**
     * Sets the state of the checkbox indicating if Bluetooth is turned on. The
     * state of the "Find devices" button and the list of discovered devices may
     * also be affected by a change to the state.
     * @param {boolean} checked Flag Indicating if Bluetooth is turned on.
     * @private
     */
    setBluetoothState_: function(checked) {
      $('enable-bluetooth').checked = checked;
      $('bluetooth-paired-devices-list').parentNode.hidden = !checked;
      $('bluetooth-add-device').hidden = !checked;
      $('bluetooth-reconnect-device').hidden = !checked;
      // Flush list of previously discovered devices if bluetooth is turned off.
      if (!checked) {
        $('bluetooth-paired-devices-list').clear();
        $('bluetooth-unpaired-devices-list').clear();
      } else {
        chrome.send('getPairedBluetoothDevices');
      }
    },

    /**
     * Adds an element to the list of available Bluetooth devices. If an element
     * with a matching address is found, the existing element is updated.
     * @param {{name: string,
     *          address: string,
     *          paired: boolean,
     *          connected: boolean}} device
     *     Decription of the Bluetooth device.
     * @private
     */
    addBluetoothDevice_: function(device) {
      var list = $('bluetooth-unpaired-devices-list');
      // Display the "connecting" (already paired or not yet paired) and the
      // paired devices in the same list.
      if (device.paired || device.connecting) {
        // Test to see if the device is currently in the unpaired list, in which
        // case it should be removed from that list.
        var index = $('bluetooth-unpaired-devices-list').find(device.address);
        if (index != undefined)
          $('bluetooth-unpaired-devices-list').deleteItemAtIndex(index);
        list = $('bluetooth-paired-devices-list');
      } else {
        // Test to see if the device is currently in the paired list, in which
        // case it should be removed from that list.
        var index = $('bluetooth-paired-devices-list').find(device.address);
        if (index != undefined)
          $('bluetooth-paired-devices-list').deleteItemAtIndex(index);
      }
      list.appendDevice(device);

      // One device can be in the process of pairing.  If found, display
      // the Bluetooth pairing overlay.
      if (device.pairing)
        BluetoothPairing.showDialog(device);
    },

    /**
     * Removes an element from the list of available devices.
     * @param {string} address Unique address of the device.
     * @private
     */
    removeBluetoothDevice_: function(address) {
      var index = $('bluetooth-unpaired-devices-list').find(address);
      if (index != undefined) {
        $('bluetooth-unpaired-devices-list').deleteItemAtIndex(index);
      } else {
        index = $('bluetooth-paired-devices-list').find(address);
        if (index != undefined)
          $('bluetooth-paired-devices-list').deleteItemAtIndex(index);
      }
    },

    /**
     * Shows the overlay dialog for changing the user avatar image.
     * @private
     */
    showImagerPickerOverlay_: function() {
      OptionsPage.navigateToPage('changePicture');
    },

    /**
     * Shows (or not) the "User" section of the settings page based on whether
     * any of the sub-sections are present (or not).
     * @private
     */
    maybeShowUserSection_: function() {
      $('sync-users-section').hidden =
          $('profiles-section').hidden &&
          $('sync-section').hidden &&
          $('profiles-supervised-dashboard-tip').hidden;
    },

    /**
     * Updates the date and time section with time sync information.
     * @param {boolean} canSetTime Whether the system time can be set.
     * @private
     */
    setCanSetTime_: function(canSetTime) {
      // If the time has been network-synced, it cannot be set manually.
      $('time-synced-explanation').hidden = canSetTime;
      $('set-time').hidden = !canSetTime;
    },

    /**
     * Handle the 'set date and time' button click.
     * @private
     */
    handleSetTime_: function() {
      chrome.send('showSetTime');
    },
  };

  //Forward public APIs to private implementations.
  [
    'addBluetoothDevice',
    'deleteCurrentProfile',
    'enableCertificateButton',
    'enableFactoryResetSection',
    'getCurrentProfile',
    'getStartStopSyncButton',
    'hideBluetoothSettings',
    'notifyInitializationComplete',
    'removeBluetoothDevice',
    'scrollToSection',
    'setAccountPictureManaged',
    'setWallpaperManaged',
    'setAutoOpenFileTypesDisplayed',
    'setBluetoothState',
    'setCanSetTime',
    'setFontSize',
    'setNativeThemeButtonEnabled',
    'setHighContrastCheckboxState',
    'setMetricsReportingCheckboxState',
    'setMetricsReportingSettingVisibility',
    'setProfilesInfo',
    'setSpokenFeedbackCheckboxState',
    'setThemesResetButtonEnabled',
    'setVirtualKeyboardCheckboxState',
    'setupPageZoomSelector',
    'setupProxySettingsButton',
    'showBluetoothSettings',
    'showCreateProfileError',
    'showCreateProfileSuccess',
    'showCreateProfileWarning',
    'showHotwordSection',
    'showManagedUserImportError',
    'showManagedUserImportSuccess',
    'showMouseControls',
    'showTouchpadControls',
    'toggleExtensionIndicators',
    'updateAccountPicture',
    'updateAutoLaunchState',
    'updateDefaultBrowserState',
    'updateEasyUnlock',
    'updateManagesSupervisedUsers',
    'updateSearchEngines',
    'updateStartupPages',
    'updateSyncState',
  ].forEach(function(name) {
    BrowserOptions[name] = function() {
      var instance = BrowserOptions.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  if (cr.isChromeOS) {
    /**
     * Returns username (canonical email) of the user logged in (ChromeOS only).
     * @return {string} user email.
     */
    // TODO(jhawkins): Investigate the use case for this method.
    BrowserOptions.getLoggedInUsername = function() {
      return BrowserOptions.getInstance().username_;
    };
  }

  // Export
  return {
    BrowserOptions: BrowserOptions
  };
});
