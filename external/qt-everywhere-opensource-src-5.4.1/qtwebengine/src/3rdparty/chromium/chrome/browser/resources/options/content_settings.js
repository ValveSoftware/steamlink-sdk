// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  //////////////////////////////////////////////////////////////////////////////
  // ContentSettings class:

  /**
   * Encapsulated handling of content settings page.
   * @constructor
   */
  function ContentSettings() {
    this.activeNavTab = null;
    OptionsPage.call(this, 'content',
                     loadTimeData.getString('contentSettingsPageTabTitle'),
                     'content-settings-page');
  }

  cr.addSingletonGetter(ContentSettings);

  ContentSettings.prototype = {
    __proto__: OptionsPage.prototype,

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var exceptionsButtons =
          this.pageDiv.querySelectorAll('.exceptions-list-button');
      for (var i = 0; i < exceptionsButtons.length; i++) {
        exceptionsButtons[i].onclick = function(event) {
          var page = ContentSettingsExceptionsArea.getInstance();

          // Add on the proper hash for the content type, and store that in the
          // history so back/forward and tab restore works.
          var hash = event.currentTarget.getAttribute('contentType');
          var url = page.name + '#' + hash;
          uber.pushState({pageName: page.name}, url);

          // Navigate after the local history has been replaced in order to have
          // the correct hash loaded.
          OptionsPage.showPageByName('contentExceptions', false);
        };
      }

      var manageHandlersButton = $('manage-handlers-button');
      if (manageHandlersButton) {
        manageHandlersButton.onclick = function(event) {
          OptionsPage.navigateToPage('handlers');
        };
      }

      if (cr.isChromeOS)
        UIAccountTweaks.applyGuestModeVisibility(document);

      // Cookies filter page ---------------------------------------------------
      $('show-cookies-button').onclick = function(event) {
        chrome.send('coreOptionsUserMetricsAction', ['Options_ShowCookies']);
        OptionsPage.navigateToPage('cookies');
      };

      $('content-settings-overlay-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);

      $('media-pepper-flash-default').hidden = true;
      $('media-pepper-flash-exceptions').hidden = true;

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
   * Sets the values for all the content settings radios.
   * @param {Object} dict A mapping from radio groups to the checked value for
   *     that group.
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
    }
  };

  /**
   * Updates the labels and indicators for the Media settings. Those require
   * special handling because they are backed by multiple prefs and can change
   * their scope based on the managed state of the backing prefs.
   * @param {Object} mediaSettings A dictionary containing the following fields:
   *     {String} askText The label for the ask radio button.
   *     {String} blockText The label for the block radio button.
   *     {Boolean} cameraDisabled Whether to disable the camera dropdown.
   *     {Boolean} micDisabled Whether to disable the microphone dropdown.
   *     {Boolean} showBubble Wether to show the managed icon and bubble for the
   *         media label.
   *     {String} bubbleText The text to use inside the bubble if it is shown.
   */
  ContentSettings.updateMediaUI = function(mediaSettings) {
    $('media-stream-ask-label').innerHTML =
        loadTimeData.getString(mediaSettings.askText);
    $('media-stream-block-label').innerHTML =
        loadTimeData.getString(mediaSettings.blockText);

    if (mediaSettings.micDisabled)
      $('media-select-mic').disabled = true;
    if (mediaSettings.cameraDisabled)
      $('media-select-camera').disabled = true;

    OptionsPage.hideBubble();
    // Create a synthetic pref change event decorated as
    // CoreOptionsHandler::CreateValueForPref() does.
    // TODO(arv): It was not clear what event type this should use?
    var event = new Event('undefined');
    event.value = {};

    if (mediaSettings.showBubble) {
      event.value = { controlledBy: 'policy' };
      $('media-indicator').setAttribute(
          'textpolicy', loadTimeData.getString(mediaSettings.bubbleText));
      $('media-indicator').location = cr.ui.ArrowLocation.TOP_START;
    }

    $('media-indicator').handlePrefChange(event);
  };

  /**
   * Initializes an exceptions list.
   * @param {string} type The content type that we are setting exceptions for.
   * @param {Array} exceptions An array of pairs, where the first element of
   *     each pair is the filter string, and the second is the setting
   *     (allow/block).
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
    exceptionsList.parentNode.hidden = false;
    exceptionsList.setExceptions(otrExceptions);
  };

  /**
   * @param {string} type The type of exceptions (e.g. "location") to get.
   * @param {string} mode The mode of the desired exceptions list (e.g. otr).
   * @return {?ExceptionsList} The corresponding exceptions list or null.
   */
  ContentSettings.getExceptionsList = function(type, mode) {
    return document.querySelector(
        'div[contentType=' + type + '] list[mode=' + mode + ']');
  };

  /**
   * The browser's response to a request to check the validity of a given URL
   * pattern.
   * @param {string} type The content type.
   * @param {string} mode The browser mode.
   * @param {string} pattern The pattern.
   * @param {bool} valid Whether said pattern is valid in the context of
   *     a content exception setting.
   */
  ContentSettings.patternValidityCheckComplete =
      function(type, mode, pattern, valid) {
    this.getExceptionsList(type, mode).patternValidityCheckComplete(pattern,
                                                                    valid);
  };

  /**
   * Shows/hides the link to the Pepper Flash camera and microphone default
   * settings.
   * Please note that whether the link is actually showed or not is also
   * affected by the style class pepper-flash-settings.
   */
  ContentSettings.showMediaPepperFlashDefaultLink = function(show) {
    $('media-pepper-flash-default').hidden = !show;
  };

  /**
   * Shows/hides the link to the Pepper Flash camera and microphone
   * site-specific settings.
   * Please note that whether the link is actually showed or not is also
   * affected by the style class pepper-flash-settings.
   */
  ContentSettings.showMediaPepperFlashExceptionsLink = function(show) {
    $('media-pepper-flash-exceptions').hidden = !show;
  };

  /**
   * Shows/hides the whole Web MIDI settings.
   * @param {bool} show Wether to show the whole Web MIDI settings.
   */
  ContentSettings.showExperimentalWebMIDISettings = function(show) {
    $('experimental-web-midi-settings').hidden = !show;
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
   * Enables/disables the protected content exceptions button.
   * @param {bool} enable Whether to enable the button.
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
