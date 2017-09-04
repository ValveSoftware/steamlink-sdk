// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-date-time-page' is the settings page containing date and time
 * settings.
 */

cr.exportPath('settings');

/**
 * Describes the status of the auto-detect policy.
 * @enum {number}
 */
settings.TimeZoneAutoDetectPolicy = {
  NONE: 0,
  FORCED_ON: 1,
  FORCED_OFF: 2,
};

Polymer({
  is: 'settings-date-time-page',

  behaviors: [PrefsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The time zone auto-detect policy.
     * @private {settings.TimeZoneAutoDetectPolicy}
     */
    timeZoneAutoDetectPolicy_: {
      type: Boolean,
      value: function() {
        if (!loadTimeData.valueExists('timeZoneAutoDetectValueFromPolicy'))
          return settings.TimeZoneAutoDetectPolicy.NONE;
        return loadTimeData.getBoolean('timeZoneAutoDetectValueFromPolicy') ?
            settings.TimeZoneAutoDetectPolicy.FORCED_ON :
            settings.TimeZoneAutoDetectPolicy.FORCED_OFF;
      },
    },

    /**
     * Whether a policy controls the time zone auto-detect setting.
     * @private
     */
    hasTimeZoneAutoDetectPolicy_: {
      type: Boolean,
      computed:
          'computeHasTimeZoneAutoDetectPolicy_(timeZoneAutoDetectPolicy_)',
    },

    /**
     * The effective time zone auto-detect setting.
     * @private
     */
    timeZoneAutoDetect_: {
      type: Boolean,
      computed: 'computeTimeZoneAutoDetect_(' +
          'timeZoneAutoDetectPolicy_,' +
          'prefs.settings.resolve_timezone_by_geolocation.value)',
    },

    /**
     * A fake preference to provide cr-policy-pref-indicator with policy info.
     * @private {!chrome.settingsPrivate.PrefObject|undefined}
     */
    fakeTimeZonePolicyPref_: Object,

    /**
     * Initialized with the current time zone so the menu displays the
     * correct value. The full option list is fetched lazily if necessary by
     * maybeGetTimeZoneList_.
     * @private {!DropdownMenuOptionList}
     */
    timeZoneList_: {
      type: Array,
      value: function() {
        return [{
          name: loadTimeData.getString('timeZoneName'),
          value: loadTimeData.getString('timeZoneID'),
        }];
      },
    },

    /**
     * Whether date and time are settable. Normally the date and time are forced
     * by network time, so default to false to initially hide the button.
     * @private
     */
    canSetDateTime_: {
      type: Boolean,
      value: false,
    },
  },

  observers: [
    'maybeGetTimeZoneList_(' +
        'prefs.cros.system.timezone.value, timeZoneAutoDetect_)',
  ],

  attached: function() {
    this.addWebUIListener(
        'time-zone-auto-detect-policy',
        this.onTimeZoneAutoDetectPolicyChanged_.bind(this));
    this.addWebUIListener(
        'can-set-date-time-changed', this.onCanSetDateTimeChanged_.bind(this));

    chrome.send('dateTimePageReady');
    this.maybeGetTimeZoneList_();
  },

  /**
   * @param {boolean} managed Whether the auto-detect setting is controlled.
   * @param {boolean} valueFromPolicy The value of the auto-detect setting
   *     forced by policy.
   * @private
   */
  onTimeZoneAutoDetectPolicyChanged_: function(managed, valueFromPolicy) {
    if (!managed) {
      this.timeZoneAutoDetectPolicy_ = settings.TimeZoneAutoDetectPolicy.NONE;
      this.fakeTimeZonePolicyPref_ = undefined;
      return;
    }

    this.timeZoneAutoDetectPolicy_ = valueFromPolicy ?
        settings.TimeZoneAutoDetectPolicy.FORCED_ON :
        settings.TimeZoneAutoDetectPolicy.FORCED_OFF;

    if (!this.fakeTimeZonePolicyPref_) {
      this.fakeTimeZonePolicyPref_ = {
        key: 'fakeTimeZonePref',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 0,
        controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
        enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      };
    }
  },

  /**
   * @param {boolean} canSetDateTime Whether date and time are settable.
   * @private
   */
  onCanSetDateTimeChanged_: function(canSetDateTime) {
    this.canSetDateTime_ = canSetDateTime;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onTimeZoneAutoDetectCheckboxChange_: function(e) {
    this.setPrefValue(
        'settings.resolve_timezone_by_geolocation', e.target.checked);
  },

  /** @private */
  onSetDateTimeTap_: function() {
    chrome.send('showSetDateTimeUI');
  },

  /**
   * @param {settings.TimeZoneAutoDetectPolicy} timeZoneAutoDetectPolicy
   * @return {boolean}
   * @private
   */
  computeHasTimeZoneAutoDetectPolicy_: function(timeZoneAutoDetectPolicy) {
    return timeZoneAutoDetectPolicy != settings.TimeZoneAutoDetectPolicy.NONE;
  },

  /**
   * @param {settings.TimeZoneAutoDetectPolicy} timeZoneAutoDetectPolicy
   * @param {boolean} prefValue Value of the geolocation pref.
   * @return {boolean} Whether time zone auto-detect is enabled.
   * @private
   */
  computeTimeZoneAutoDetect_: function(timeZoneAutoDetectPolicy, prefValue) {
    switch (timeZoneAutoDetectPolicy) {
      case settings.TimeZoneAutoDetectPolicy.NONE:
        return prefValue;
      case settings.TimeZoneAutoDetectPolicy.FORCED_ON:
        return true;
      case settings.TimeZoneAutoDetectPolicy.FORCED_OFF:
        return false;
      default:
        assertNotReached();
    }
  },

  /**
   * Fetches the list of time zones if necessary.
   * @private
   */
  maybeGetTimeZoneList_: function() {
    // Only fetch the list once.
    if (this.timeZoneList_.length > 1 || !CrSettingsPrefs.isInitialized)
      return;

    // If auto-detect is enabled, we only need the current time zone.
    if (this.timeZoneAutoDetect_ &&
        this.getPref('cros.system.timezone').value ==
            this.timeZoneList_[0].value) {
      return;
    }

    cr.sendWithPromise('getTimeZones').then(this.setTimeZoneList_.bind(this));
  },

  /**
   * Converts the C++ response into an array of menu options.
   * @param {!Array<!Array<string>>} timeZones C++ time zones response.
   * @private
   */
  setTimeZoneList_: function(timeZones) {
    this.timeZoneList_ = timeZones.map(function(timeZonePair) {
      return {
        name: timeZonePair[1],
        value: timeZonePair[0],
      };
    });
  },
});
