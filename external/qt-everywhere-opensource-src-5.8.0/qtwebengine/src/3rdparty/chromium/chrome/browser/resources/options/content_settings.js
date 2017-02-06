// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('options');

/**
 * @typedef {{appId: string,
 *            appName: (string|undefined),
 *            embeddingOrigin: (string|undefined),
 *            origin: string,
 *            setting: string,
 *            source: string,
 *            video: (string|undefined)}}
 */
options.Exception;

cr.define('options', function() {
  /** @const */ var Page = cr.ui.pageManager.Page;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  //////////////////////////////////////////////////////////////////////////////
  // ContentSettings class:

  /**
   * Encapsulated handling of content settings page.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function ContentSettings() {
    this.activeNavTab = null;
    Page.call(this, 'content',
              loadTimeData.getString('contentSettingsPageTabTitle'),
              'content-settings-page');
  }

  cr.addSingletonGetter(ContentSettings);

  ContentSettings.prototype = {
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      var exceptionsButtons =
          this.pageDiv.querySelectorAll('.exceptions-list-button');
      for (var i = 0; i < exceptionsButtons.length; i++) {
        exceptionsButtons[i].onclick = function(event) {
          var hash = event.currentTarget.getAttribute('contentType');
          PageManager.showPageByName('contentExceptions', true,
                                     {hash: '#' + hash});
        };
      }

      var manageHandlersButton = $('manage-handlers-button');
      if (manageHandlersButton) {
        manageHandlersButton.onclick = function(event) {
          PageManager.showPageByName('handlers');
        };
      }

      if (cr.isChromeOS) {
        // Disable some controls for Guest in Chrome OS.
        UIAccountTweaks.applyGuestSessionVisibility(document);

        // Disable some controls for Public session in Chrome OS.
        UIAccountTweaks.applyPublicSessionVisibility(document);
      }

      // Cookies filter page ---------------------------------------------------
      $('show-cookies-button').onclick = function(event) {
        chrome.send('coreOptionsUserMetricsAction', ['Options_ShowCookies']);
        PageManager.showPageByName('cookies');
      };

      $('content-settings-overlay-confirm').onclick =
          PageManager.closeOverlay.bind(PageManager);

      $('media-pepper-flash-default-mic').hidden = true;
      $('media-pepper-flash-default-camera').hidden = true;
      $('media-pepper-flash-exceptions-mic').hidden = true;
      $('media-pepper-flash-exceptions-camera').hidden = true;

      $('media-select-mic').addEventListener('change',
          ContentSettings.setDefaultMicrophone_);
      $('media-select-camera').addEventListener('change',
          ContentSettings.setDefaultCamera_);
    },
  };

  ContentSettings.updateHandlersEnabledRadios = function(enabled) {
    var selector = '#content-settings-page input[type=radio][value=' +
        (enabled ? 'allow' : 'block') + '].handler-radio';
    document.querySelector(selector).checked = true;
  };

  /**
   * Sets the values for all the content settings radios and labels.
   * @param {Object<{managedBy: string, value: string}>} dict A mapping from
   *     radio groups to the checked value for that group.
   */
  ContentSettings.setContentFilterSettingsValue = function(dict) {
    for (var group in dict) {
      var managedBy = dict[group].managedBy;
      var controlledBy = managedBy == 'policy' || managedBy == 'extension' ?
          managedBy : null;
      document.querySelector('input[type=radio][name=' + group + '][value=' +
                             dict[group].value + ']').checked = true;
      var radios = document.querySelectorAll('input[type=radio][name=' +
                                             group + ']');
      for (var i = 0, len = radios.length; i < len; i++) {
        radios[i].disabled = (managedBy != 'default');
        radios[i].controlledBy = controlledBy;
      }
      var indicators = document.querySelectorAll(
          'span.controlled-setting-indicator[content-setting=' + group + ']');
      if (indicators.length == 0)
        continue;
      // Create a synthetic pref change event decorated as
      // CoreOptionsHandler::CreateValueForPref() does.
      var event = new Event(group);
      event.value = {
        value: dict[group].value,
        controlledBy: controlledBy,
      };
      for (var i = 0; i < indicators.length; i++) {
        indicators[i].handlePrefChange(event);
      }
      if (controlledBy)
        this.getExceptionsList(group, 'normal').setOverruledBy(controlledBy);
    }
  };

  /**
   * Initializes an exceptions list.
   * @param {string} type The content type that we are setting exceptions for.
   * @param {Array<options.Exception>} exceptions An array of pairs, where the
   *     first element of each pair is the filter string, and the second is the
   *     setting (allow/block).
   */
  ContentSettings.setExceptions = function(type, exceptions) {
    this.getExceptionsList(type, 'normal').setExceptions(exceptions);
  };

  ContentSettings.setHandlers = function(handlers) {
    $('handlers-list').setHandlers(handlers);
  };

  ContentSettings.setIgnoredHandlers = function(ignoredHandlers) {
    $('ignored-handlers-list').setHandlers(ignoredHandlers);
  };

  ContentSettings.setOTRExceptions = function(type, otrExceptions) {
    var exceptionsList = this.getExceptionsList(type, 'otr');
    // Settings for Guest hides many sections, so check for null first.
    if (exceptionsList) {
      exceptionsList.parentNode.hidden = false;
      exceptionsList.setExceptions(otrExceptions);
    }
  };

  /**
   * @param {string} type The type of exceptions (e.g. "location") to get.
   * @param {string} mode The mode of the desired exceptions list (e.g. otr).
   * @return {?options.contentSettings.ExceptionsList} The corresponding
   *     exceptions list or null.
   */
  ContentSettings.getExceptionsList = function(type, mode) {
    var exceptionsList = document.querySelector(
        'div[contentType=' + type + '] list[mode=' + mode + ']');
    return !exceptionsList ? null :
        assertInstanceof(exceptionsList,
                         options.contentSettings.ExceptionsList);
  };

  /**
   * The browser's response to a request to check the validity of a given URL
   * pattern.
   * @param {string} type The content type.
   * @param {string} mode The browser mode.
   * @param {string} pattern The pattern.
   * @param {boolean} valid Whether said pattern is valid in the context of
   *     a content exception setting.
   */
  ContentSettings.patternValidityCheckComplete =
      function(type, mode, pattern, valid) {
    this.getExceptionsList(type, mode).patternValidityCheckComplete(pattern,
                                                                    valid);
  };

  /**
   * Shows/hides the link to the Pepper Flash camera or microphone,
   * default or exceptions settings.
   * Please note that whether the link is actually showed or not is also
   * affected by the style class pepper-flash-settings.
   * @param {string} linkType Can be 'default' or 'exceptions'.
   * @param {string} contentType Can be 'mic' or 'camera'.
   * @param {boolean} show Whether to show (or hide) the link.
   */
  ContentSettings.showMediaPepperFlashLink =
      function(linkType, contentType, show) {
    assert(['default', 'exceptions'].indexOf(linkType) >= 0);
    assert(['mic', 'camera'].indexOf(contentType) >= 0);
    $('media-pepper-flash-' + linkType + '-' + contentType).hidden = !show;
  };

  /**
   * Updates the microphone/camera devices menu with the given entries.
   * @param {string} type The device type.
   * @param {Array} devices List of available devices.
   * @param {string} defaultdevice The unique id of the current default device.
   */
  ContentSettings.updateDevicesMenu = function(type, devices, defaultdevice) {
    var deviceSelect = '';
    if (type == 'mic') {
      deviceSelect = $('media-select-mic');
    } else if (type == 'camera') {
      deviceSelect = $('media-select-camera');
    } else {
      console.error('Unknown device type for <device select> UI element: ' +
                    type);
      return;
    }

    deviceSelect.textContent = '';

    var deviceCount = devices.length;
    var defaultIndex = -1;
    for (var i = 0; i < deviceCount; i++) {
      var device = devices[i];
      var option = new Option(device.name, device.id);
      if (option.value == defaultdevice)
        defaultIndex = i;
      deviceSelect.appendChild(option);
    }
    if (defaultIndex >= 0)
      deviceSelect.selectedIndex = defaultIndex;
  };

  /**
   * Sets the visibility of the microphone/camera devices menu.
   * @param {string} type The content settings type name of this device.
   * @param {boolean} show Whether to show the menu.
   */
  ContentSettings.setDevicesMenuVisibility = function(type, show) {
    assert(type == 'media-stream-mic' || type == 'media-stream-camera');
    var deviceSelect = $(type == 'media-stream-mic' ? 'media-select-mic' :
                                                      'media-select-camera');
    deviceSelect.hidden = !show;
  };

  /**
   * Enables/disables the protected content exceptions button.
   * @param {boolean} enable Whether to enable the button.
   */
  ContentSettings.enableProtectedContentExceptions = function(enable) {
    var exceptionsButton = $('protected-content-exceptions');
    if (exceptionsButton)
      exceptionsButton.disabled = !enable;
  };

  /**
   * Set the default microphone device based on the popup selection.
   * @private
   */
  ContentSettings.setDefaultMicrophone_ = function() {
    var deviceSelect = $('media-select-mic');
    chrome.send('setDefaultCaptureDevice', ['mic', deviceSelect.value]);
  };

  /**
   * Set the default camera device based on the popup selection.
   * @private
   */
  ContentSettings.setDefaultCamera_ = function() {
    var deviceSelect = $('media-select-camera');
    chrome.send('setDefaultCaptureDevice', ['camera', deviceSelect.value]);
  };

  // Export
  return {
    ContentSettings: ContentSettings
  };

});
