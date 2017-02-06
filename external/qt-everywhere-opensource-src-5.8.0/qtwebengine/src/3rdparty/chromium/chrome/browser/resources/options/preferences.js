// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  /////////////////////////////////////////////////////////////////////////////
  // Preferences class:

  /**
   * Preferences class manages access to Chrome profile preferences.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function Preferences() {
    // Map of registered preferences.
    this.registeredPreferences_ = {};
  }

  cr.addSingletonGetter(Preferences);

  /**
   * Sets a Boolean preference and signals its new value.
   * @param {string} name Preference name.
   * @param {boolean} value New preference value.
   * @param {boolean} commit Whether to commit the change to Chrome.
   * @param {string=} opt_metric User metrics identifier.
   */
  Preferences.setBooleanPref = function(name, value, commit, opt_metric) {
    if (!commit) {
      Preferences.getInstance().setPrefNoCommit_(name, 'bool', Boolean(value));
      return;
    }

    var argumentList = [name, Boolean(value)];
    if (opt_metric != undefined) argumentList.push(opt_metric);
    chrome.send('setBooleanPref', argumentList);
  };

  /**
   * Sets an integer preference and signals its new value.
   * @param {string} name Preference name.
   * @param {number} value New preference value.
   * @param {boolean} commit Whether to commit the change to Chrome.
   * @param {string} metric User metrics identifier.
   */
  Preferences.setIntegerPref = function(name, value, commit, metric) {
    if (!commit) {
      Preferences.getInstance().setPrefNoCommit_(name, 'int', Number(value));
      return;
    }

    var argumentList = [name, Number(value)];
    if (metric != undefined) argumentList.push(metric);
    chrome.send('setIntegerPref', argumentList);
  };

  /**
   * Sets a double-valued preference and signals its new value.
   * @param {string} name Preference name.
   * @param {number} value New preference value.
   * @param {boolean} commit Whether to commit the change to Chrome.
   * @param {string} metric User metrics identifier.
   */
  Preferences.setDoublePref = function(name, value, commit, metric) {
    if (!commit) {
      Preferences.getInstance().setPrefNoCommit_(name, 'double', Number(value));
      return;
    }

    var argumentList = [name, Number(value)];
    if (metric != undefined) argumentList.push(metric);
    chrome.send('setDoublePref', argumentList);
  };

  /**
   * Sets a string preference and signals its new value.
   * @param {string} name Preference name.
   * @param {string} value New preference value.
   * @param {boolean} commit Whether to commit the change to Chrome.
   * @param {string} metric User metrics identifier.
   */
  Preferences.setStringPref = function(name, value, commit, metric) {
    if (!commit) {
      Preferences.getInstance().setPrefNoCommit_(name, 'string', String(value));
      return;
    }

    var argumentList = [name, String(value)];
    if (metric != undefined) argumentList.push(metric);
    chrome.send('setStringPref', argumentList);
  };

  /**
   * Sets a string preference that represents a URL and signals its new value.
   * The value will be fixed to be a valid URL when it gets committed to Chrome.
   * @param {string} name Preference name.
   * @param {string} value New preference value.
   * @param {boolean} commit Whether to commit the change to Chrome.
   * @param {string} metric User metrics identifier.
   */
  Preferences.setURLPref = function(name, value, commit, metric) {
    if (!commit) {
      Preferences.getInstance().setPrefNoCommit_(name, 'url', String(value));
      return;
    }

    var argumentList = [name, String(value)];
    if (metric != undefined) argumentList.push(metric);
    chrome.send('setURLPref', argumentList);
  };

  /**
   * Sets a JSON list preference and signals its new value.
   * @param {string} name Preference name.
   * @param {Array} value New preference value.
   * @param {boolean} commit Whether to commit the change to Chrome.
   * @param {string} metric User metrics identifier.
   */
  Preferences.setListPref = function(name, value, commit, metric) {
    if (!commit) {
      Preferences.getInstance().setPrefNoCommit_(name, 'list', value);
      return;
    }

    var argumentList = [name, JSON.stringify(value)];
    if (metric != undefined) argumentList.push(metric);
    chrome.send('setListPref', argumentList);
  };

  /**
   * Clears the user setting for a preference and signals its new effective
   * value.
   * @param {string} name Preference name.
   * @param {boolean} commit Whether to commit the change to Chrome.
   * @param {string=} opt_metric User metrics identifier.
   */
  Preferences.clearPref = function(name, commit, opt_metric) {
    if (!commit) {
      Preferences.getInstance().clearPrefNoCommit_(name);
      return;
    }

    var argumentList = [name];
    if (opt_metric != undefined) argumentList.push(opt_metric);
    chrome.send('clearPref', argumentList);
  };

  Preferences.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Adds an event listener to the target.
     * @param {string} type The name of the event.
     * @param {EventListenerType} handler The handler for the event. This is
     *     called when the event is dispatched.
     */
    addEventListener: function(type, handler) {
      cr.EventTarget.prototype.addEventListener.call(this, type, handler);
      if (!(type in this.registeredPreferences_))
        this.registeredPreferences_[type] = {};
    },

    /**
     * Initializes preference reading and change notifications.
     */
    initialize: function() {
      var params1 = ['Preferences.prefsFetchedCallback'];
      var params2 = ['Preferences.prefsChangedCallback'];
      for (var prefName in this.registeredPreferences_) {
        params1.push(prefName);
        params2.push(prefName);
      }
      chrome.send('fetchPrefs', params1);
      chrome.send('observePrefs', params2);
    },

    /**
     * Helper function for flattening of dictionary passed via fetchPrefs
     * callback.
     * @param {string} prefix Preference name prefix.
     * @param {Object} dict Map with preference values.
     * @private
     */
    flattenMapAndDispatchEvent_: function(prefix, dict) {
      for (var prefName in dict) {
        var value = dict[prefName];
        if (typeof value == 'object' &&
            !this.registeredPreferences_[prefix + prefName]) {
          this.flattenMapAndDispatchEvent_(prefix + prefName + '.', value);
        } else if (value) {
          var event = new Event(prefix + prefName);
          this.registeredPreferences_[prefix + prefName].orig = value;
          event.value = value;
          this.dispatchEvent(event);
        }
      }
    },

    /**
     * Sets a preference and signals its new value. The change is propagated
     * throughout the UI code but is not committed to Chrome yet. The new value
     * and its data type are stored so that commitPref() can later be used to
     * invoke the appropriate set*Pref() method and actually commit the change.
     * @param {string} name Preference name.
     * @param {string} type Preference data type.
     * @param {*} value New preference value.
     * @private
     */
    setPrefNoCommit_: function(name, type, value) {
      var pref = this.registeredPreferences_[name];
      pref.action = 'set';
      pref.type = type;
      pref.value = value;

      var event = new Event(name);
      // Decorate pref value as CoreOptionsHandler::CreateValueForPref() does.
      event.value = {value: value, uncommitted: true};
      if (pref.orig) {
        event.value.recommendedValue = pref.orig.recommendedValue;
        event.value.disabled = pref.orig.disabled;
      }
      this.dispatchEvent(event);
    },

    /**
     * Clears a preference and signals its new value. The change is propagated
     * throughout the UI code but is not committed to Chrome yet.
     * @param {string} name Preference name.
     * @private
     */
    clearPrefNoCommit_: function(name) {
      var pref = this.registeredPreferences_[name];
      pref.action = 'clear';
      delete pref.type;
      delete pref.value;

      var event = new Event(name);
      // Decorate pref value as CoreOptionsHandler::CreateValueForPref() does.
      event.value = {controlledBy: 'recommended', uncommitted: true};
      if (pref.orig) {
        event.value.value = pref.orig.recommendedValue;
        event.value.recommendedValue = pref.orig.recommendedValue;
        event.value.disabled = pref.orig.disabled;
      }
      this.dispatchEvent(event);
    },

    /**
     * Commits a preference change to Chrome and signals the new preference
     * value. Does nothing if there is no uncommitted change.
     * @param {string} name Preference name.
     * @param {string} metric User metrics identifier.
     */
    commitPref: function(name, metric) {
      var pref = this.registeredPreferences_[name];
      switch (pref.action) {
        case 'set':
          switch (pref.type) {
            case 'bool':
              Preferences.setBooleanPref(name, pref.value, true, metric);
              break;
            case 'int':
              Preferences.setIntegerPref(name, pref.value, true, metric);
              break;
            case 'double':
              Preferences.setDoublePref(name, pref.value, true, metric);
              break;
            case 'string':
              Preferences.setStringPref(name, pref.value, true, metric);
              break;
            case 'url':
              Preferences.setURLPref(name, pref.value, true, metric);
              break;
            case 'list':
              Preferences.setListPref(name, pref.value, true, metric);
              break;
          }
          break;
        case 'clear':
          Preferences.clearPref(name, true, metric);
          break;
      }
      delete pref.action;
      delete pref.type;
      delete pref.value;
    },

    /**
     * Rolls back a preference change and signals the original preference value.
     * Does nothing if there is no uncommitted change.
     * @param {string} name Preference name.
     */
    rollbackPref: function(name) {
      var pref = this.registeredPreferences_[name];
      if (!pref.action)
        return;

      delete pref.action;
      delete pref.type;
      delete pref.value;

      var event = new Event(name);
      event.value = pref.orig || {};
      event.value.uncommitted = true;
      this.dispatchEvent(event);
    }
  };

  /**
   * Callback for fetchPrefs method.
   * @param {Object} dict Map of fetched property values.
   */
  Preferences.prefsFetchedCallback = function(dict) {
    Preferences.getInstance().flattenMapAndDispatchEvent_('', dict);
  };

  /**
   * Callback for observePrefs method.
   * @param {Array} notification An array defining changed preference values.
   *     notification[0] contains name of the change preference while its new
   *     value is stored in notification[1].
   */
  Preferences.prefsChangedCallback = function(notification) {
    var event = new Event(notification[0]);
    event.value = notification[1];
    var prefs = Preferences.getInstance();
    prefs.registeredPreferences_[notification[0]] = {orig: notification[1]};
    if (event.value)
      prefs.dispatchEvent(event);
  };

  // Export
  return {
    Preferences: Preferences
  };

});
