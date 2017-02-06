// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../../../../ui/webui/resources/js/cr/ui/focus_row.js">
<include src="../../../../ui/webui/resources/js/cr/ui/focus_grid.js">
<include src="../uber/uber_utils.js">
<include src="extension_code.js">
<include src="extension_commands_overlay.js">
<include src="extension_error_overlay.js">
<include src="extension_focus_manager.js">
<include src="focus_row.js">
<include src="extension_list.js">
<include src="pack_extension_overlay.js">
<include src="extension_loader.js">
<include src="extension_options_overlay.js">

<if expr="chromeos">
<include src="chromeos/kiosk_apps.js">
</if>

// Used for observing function of the backend datasource for this page by
// tests.
var webuiResponded = false;

cr.define('extensions', function() {
  var ExtensionList = extensions.ExtensionList;

  // Implements the DragWrapper handler interface.
  var dragWrapperHandler = {
    /** @override */
    shouldAcceptDrag: function(e) {
      // External Extension installation can be disabled globally, e.g. while a
      // different overlay is already showing.
      if (!ExtensionSettings.getInstance().dragEnabled_)
        return false;

      // We can't access filenames during the 'dragenter' event, so we have to
      // wait until 'drop' to decide whether to do something with the file or
      // not.
      // See: http://www.w3.org/TR/2011/WD-html5-20110113/dnd.html#concept-dnd-p
      return (e.dataTransfer.types &&
              e.dataTransfer.types.indexOf('Files') > -1);
    },
    /** @override */
    doDragEnter: function() {
      chrome.send('startDrag');
      ExtensionSettings.showOverlay($('drop-target-overlay'));
    },
    /** @override */
    doDragLeave: function() {
      this.hideDropTargetOverlay_();
      chrome.send('stopDrag');
    },
    /** @override */
    doDragOver: function(e) {
      e.preventDefault();
    },
    /** @override */
    doDrop: function(e) {
      this.hideDropTargetOverlay_();
      if (e.dataTransfer.files.length != 1)
        return;

      var toSend = null;
      // Files lack a check if they're a directory, but we can find out through
      // its item entry.
      for (var i = 0; i < e.dataTransfer.items.length; ++i) {
        if (e.dataTransfer.items[i].kind == 'file' &&
            e.dataTransfer.items[i].webkitGetAsEntry().isDirectory) {
          toSend = 'installDroppedDirectory';
          break;
        }
      }
      // Only process files that look like extensions. Other files should
      // navigate the browser normally.
      if (!toSend &&
          /\.(crx|user\.js|zip)$/i.test(e.dataTransfer.files[0].name)) {
        toSend = 'installDroppedFile';
      }

      if (toSend) {
        e.preventDefault();
        chrome.send(toSend);
      }
    },

    /**
     * Hide the current overlay if it is the drop target overlay.
     * @private
     */
    hideDropTargetOverlay_: function() {
      var currentOverlay = ExtensionSettings.getCurrentOverlay();
      if (currentOverlay && currentOverlay.id === 'drop-target-overlay')
        ExtensionSettings.showOverlay(null);
    }
  };

  /**
   * ExtensionSettings class
   * @class
   * @constructor
   * @implements {extensions.ExtensionListDelegate}
   */
  function ExtensionSettings() {}

  cr.addSingletonGetter(ExtensionSettings);

  ExtensionSettings.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * The drag-drop wrapper for installing external Extensions, if available.
     * null if external Extension installation is not available.
     * @type {cr.ui.DragWrapper}
     * @private
     */
    dragWrapper_: null,

    /**
     * True if drag-drop is both available and currently enabled - it can be
     * temporarily disabled while overlays are showing.
     * @type {boolean}
     * @private
     */
    dragEnabled_: false,

    /**
     * True if the page has finished the initial load.
     * @private {boolean}
     */
    hasLoaded_: false,

    /**
     * Perform initial setup.
     */
    initialize: function() {
      this.setLoading_(true);
      uber.onContentFrameLoaded();
      cr.ui.FocusOutlineManager.forDocument(document);
      measureCheckboxStrings();

      // Set the title.
      uber.setTitle(loadTimeData.getString('extensionSettings'));

      var extensionList = new ExtensionList(this);
      extensionList.id = 'extension-settings-list';
      var wrapper = $('extension-list-wrapper');
      wrapper.insertBefore(extensionList, wrapper.firstChild);

      // Get the initial profile state, and register to be notified of any
      // future changes.
      chrome.developerPrivate.getProfileConfiguration(
          this.update_.bind(this));
      chrome.developerPrivate.onProfileStateChanged.addListener(
          this.update_.bind(this));

      var extensionLoader = extensions.ExtensionLoader.getInstance();

      $('toggle-dev-on').addEventListener('change', function(e) {
        this.updateDevControlsVisibility_(true);
        chrome.developerPrivate.updateProfileConfiguration(
            {inDeveloperMode: e.target.checked});
        var suffix = $('toggle-dev-on').checked ? 'Enabled' : 'Disabled';
        chrome.send('metricsHandler:recordAction',
                    ['Options_ToggleDeveloperMode_' + suffix]);
      }.bind(this));

      window.addEventListener('resize', function() {
        this.updateDevControlsVisibility_(false);
      }.bind(this));

      // Set up the three dev mode buttons (load unpacked, pack and update).
      $('load-unpacked').addEventListener('click', function(e) {
        chrome.send('metricsHandler:recordAction',
                    ['Options_LoadUnpackedExtension']);
        extensionLoader.loadUnpacked();
      });
      $('pack-extension').addEventListener('click',
          this.handlePackExtension_.bind(this));
      $('update-extensions-now').addEventListener('click',
          this.handleUpdateExtensionNow_.bind(this));

      if (!loadTimeData.getBoolean('offStoreInstallEnabled')) {
        this.dragWrapper_ = new cr.ui.DragWrapper(document.documentElement,
                                                  dragWrapperHandler);
        this.dragEnabled_ = true;
      }

      extensions.PackExtensionOverlay.getInstance().initializePage();

      // Hook up the configure commands link to the overlay.
      var link = document.querySelector('.extension-commands-config');
      link.addEventListener('click',
          this.handleExtensionCommandsConfig_.bind(this));

      // Initialize the Commands overlay.
      extensions.ExtensionCommandsOverlay.getInstance().initializePage();

      extensions.ExtensionErrorOverlay.getInstance().initializePage(
          extensions.ExtensionSettings.showOverlay);

      extensions.ExtensionOptionsOverlay.getInstance().initializePage(
          extensions.ExtensionSettings.showOverlay);

      // Add user action logging for bottom links.
      var moreExtensionLink =
          document.getElementsByClassName('more-extensions-link');
      for (var i = 0; i < moreExtensionLink.length; i++) {
        moreExtensionLink[i].addEventListener('click', function(e) {
          chrome.send('metricsHandler:recordAction',
                      ['Options_GetMoreExtensions']);
        });
      }

      // Initialize the kiosk overlay.
      if (cr.isChromeOS) {
        var kioskOverlay = extensions.KioskAppsOverlay.getInstance();
        kioskOverlay.initialize();

        $('add-kiosk-app').addEventListener('click', function() {
          ExtensionSettings.showOverlay($('kiosk-apps-page'));
          kioskOverlay.didShowPage();
        });

        extensions.KioskDisableBailoutConfirm.getInstance().initialize();
      }

      cr.ui.overlay.setupOverlay($('drop-target-overlay'));
      cr.ui.overlay.globalInitialization();

      extensions.ExtensionFocusManager.getInstance().initialize();

      var path = document.location.pathname;
      if (path.length > 1) {
        // Skip starting slash and remove trailing slash (if any).
        var overlayName = path.slice(1).replace(/\/$/, '');
        if (overlayName == 'configureCommands')
          this.showExtensionCommandsConfigUi_();
      }
    },

    /**
     * [Re]-Populates the page with data representing the current state of
     * installed extensions.
     * @param {chrome.developerPrivate.ProfileInfo} profileInfo
     * @private
     */
    update_: function(profileInfo) {
      // We only set the page to be loading if we haven't already finished an
      // initial load, because otherwise the updates are all incremental and
      // don't need to display the interstitial spinner.
      if (!this.hasLoaded_)
        this.setLoading_(true);
      webuiResponded = true;

      /** @const */
      var supervised = profileInfo.isSupervised;

      var pageDiv = $('extension-settings');
      pageDiv.classList.toggle('profile-is-supervised', supervised);
      pageDiv.classList.toggle('showing-banner', supervised);

      var devControlsCheckbox = $('toggle-dev-on');
      devControlsCheckbox.checked = profileInfo.inDeveloperMode;
      devControlsCheckbox.disabled = supervised;

      $('load-unpacked').disabled = !profileInfo.canLoadUnpacked;
      var extensionList = $('extension-settings-list');
      extensionList.updateExtensionsData(
          profileInfo.isIncognitoAvailable,
          profileInfo.appInfoDialogEnabled).then(function() {
        if (!this.hasLoaded_) {
          this.hasLoaded_ = true;
          this.setLoading_(false);
        }
        this.onExtensionCountChanged();
      }.bind(this));
    },

    /**
     * Shows the loading spinner and hides elements that shouldn't be visible
     * while loading.
     * @param {boolean} isLoading
     * @private
     */
    setLoading_: function(isLoading) {
      document.documentElement.classList.toggle('loading', isLoading);
      $('loading-spinner').hidden = !isLoading;
      $('dev-controls').hidden = isLoading;
      this.updateDevControlsVisibility_(false);

      // The extension list is already hidden/shown elsewhere and shouldn't be
      // updated here because it can be hidden if there are no extensions.
    },

    /**
     * Handles the Pack Extension button.
     * @param {Event} e Change event.
     * @private
     */
    handlePackExtension_: function(e) {
      ExtensionSettings.showOverlay($('pack-extension-overlay'));
      chrome.send('metricsHandler:recordAction', ['Options_PackExtension']);
    },

    /**
     * Shows the Extension Commands configuration UI.
     * @private
     */
    showExtensionCommandsConfigUi_: function() {
      ExtensionSettings.showOverlay($('extension-commands-overlay'));
      chrome.send('metricsHandler:recordAction',
                  ['Options_ExtensionCommands']);
    },

    /**
     * Handles the Configure (Extension) Commands link.
     * @param {Event} e Change event.
     * @private
     */
    handleExtensionCommandsConfig_: function(e) {
      this.showExtensionCommandsConfigUi_();
    },

    /**
     * Handles the Update Extension Now button.
     * @param {Event} e Change event.
     * @private
     */
    handleUpdateExtensionNow_: function(e) {
      chrome.developerPrivate.autoUpdate();
      chrome.send('metricsHandler:recordAction',
                  ['Options_UpdateExtensions']);
    },

    /**
     * Updates the visibility of the developer controls based on whether the
     * [x] Developer mode checkbox is checked.
     * @param {boolean} animated Whether to animate any updates.
     * @private
     */
    updateDevControlsVisibility_: function(animated) {
      var showDevControls = $('toggle-dev-on').checked;
      $('extension-settings').classList.toggle('dev-mode', showDevControls);

      var devControls = $('dev-controls');
      devControls.classList.toggle('animated', animated);

      var buttons = devControls.querySelector('.button-container');
      Array.prototype.forEach.call(buttons.querySelectorAll('a, button'),
                                   function(control) {
        control.tabIndex = showDevControls ? 0 : -1;
      });
      buttons.setAttribute('aria-hidden', !showDevControls);

      window.requestAnimationFrame(function() {
        devControls.style.height = !showDevControls ? '' :
            buttons.offsetHeight + 'px';

        document.dispatchEvent(new Event('devControlsVisibilityUpdated'));
      }.bind(this));
    },

    /** @override */
    onExtensionCountChanged: function() {
      /** @const */
      var hasExtensions = $('extension-settings-list').getNumExtensions() != 0;
      $('no-extensions').hidden = hasExtensions;
      $('extension-list-wrapper').hidden = !hasExtensions;
    },
  };

  /**
   * Returns the current overlay or null if one does not exist.
   * @return {Element} The overlay element.
   */
  ExtensionSettings.getCurrentOverlay = function() {
    return document.querySelector('#overlay .page.showing');
  };

  /**
   * Sets the given overlay to show. If the overlay is already showing, this is
   * a no-op; otherwise, hides any currently-showing overlay.
   * @param {HTMLElement} node The overlay page to show. If null, all overlays
   *     are hidden.
   */
  ExtensionSettings.showOverlay = function(node) {
    var pageDiv = $('extension-settings');
    pageDiv.style.width = node ? window.getComputedStyle(pageDiv).width : '';
    document.body.classList.toggle('no-scroll', !!node);

    var currentlyShowingOverlay = ExtensionSettings.getCurrentOverlay();
    if (currentlyShowingOverlay) {
      if (currentlyShowingOverlay == node)  // Already displayed.
        return;
      currentlyShowingOverlay.classList.remove('showing');
    }

    if (node) {
      var lastFocused;

      var focusOutlineManager = cr.ui.FocusOutlineManager.forDocument(document);
      if (focusOutlineManager.visible)
        lastFocused = document.activeElement;

      $('overlay').addEventListener('cancelOverlay', function f() {
        if (lastFocused && focusOutlineManager.visible)
          lastFocused.focus();

        $('overlay').removeEventListener('cancelOverlay', f);
        uber.replaceState({}, '');
      });
      node.classList.add('showing');
    }

    var pages = document.querySelectorAll('.page');
    for (var i = 0; i < pages.length; i++) {
      pages[i].setAttribute('aria-hidden', node ? 'true' : 'false');
    }

    $('overlay').hidden = !node;

    if (node)
      ExtensionSettings.focusOverlay();

    // If drag-drop for external Extension installation is available, enable
    // drag-drop when there is any overlay showing other than the usual overlay
    // shown when drag-drop is started.
    var settings = ExtensionSettings.getInstance();
    if (settings.dragWrapper_)
      settings.dragEnabled_ = !node || node == $('drop-target-overlay');

    uber.invokeMethodOnParent(node ? 'beginInterceptingEvents' :
                                     'stopInterceptingEvents');
  };

  ExtensionSettings.focusOverlay = function() {
    var currentlyShowingOverlay = ExtensionSettings.getCurrentOverlay();
    assert(currentlyShowingOverlay);

    if (cr.ui.FocusOutlineManager.forDocument(document).visible)
      cr.ui.setInitialFocus(currentlyShowingOverlay);

    if (!currentlyShowingOverlay.contains(document.activeElement)) {
      // Make sure focus isn't stuck behind the overlay.
      document.activeElement.blur();
    }
  };

  /**
   * Utility function to find the width of various UI strings and synchronize
   * the width of relevant spans. This is crucial for making sure the
   * Enable/Enabled checkboxes align, as well as the Developer Mode checkbox.
   */
  function measureCheckboxStrings() {
    var trashWidth = 30;
    var measuringDiv = $('font-measuring-div');
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsEnabled');
    measuringDiv.className = 'enabled-text';
    var pxWidth = measuringDiv.clientWidth + trashWidth;
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsEnable');
    measuringDiv.className = 'enable-text';
    pxWidth = Math.max(measuringDiv.clientWidth + trashWidth, pxWidth);
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsDeveloperMode');
    measuringDiv.className = '';
    pxWidth = Math.max(measuringDiv.clientWidth, pxWidth);

    var style = document.createElement('style');
    style.type = 'text/css';
    style.textContent =
        '.enable-checkbox-text {' +
        '  min-width: ' + (pxWidth - trashWidth) + 'px;' +
        '}' +
        '#dev-toggle span {' +
        '  min-width: ' + pxWidth + 'px;' +
        '}';
    document.querySelector('head').appendChild(style);
  };

  // Export
  return {
    ExtensionSettings: ExtensionSettings
  };
});

window.addEventListener('load', function(e) {
  extensions.ExtensionSettings.getInstance().initialize();
});
