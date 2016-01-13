// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('network.config', function() {
  var NetworkConfig = cr.ui.define('div');

  NetworkConfig.prototype = {
    __proto__: HTMLDivElement.prototype,
    decorate: function() {
      var params = parseQueryParams(window.location);
      this.networkId_ = params.network;
      this.activeArea_ = null;
      this.userArea_ = null;
      this.managedArea_ = null;
      this.stateArea_ = null;
      this.updateDom_();
      this.fetchProperties_();
    },

    fetchProperties_: function() {
      chrome.networkingPrivate.getProperties(
          this.networkId_,
          this.updateActiveSettings_.bind(this));
      chrome.networkingPrivate.getManagedProperties(
          this.networkId_,
          this.updateManagedSettings_.bind(this));
      chrome.networkingPrivate.getState(
          this.networkId_,
          this.updateState_.bind(this));
    },

    stringifyJSON_: function(properties) {
      return JSON.stringify(properties, undefined, 2);
    },

    updateActiveSettings_: function(properties) {
      this.activeArea_.value = this.stringifyJSON_(properties);
    },

    updateManagedSettings_: function(properties) {
      var error = chrome.runtime.lastError;
      if (error) {
        this.managedArea_.value = error.message;
        this.userArea_.value = 'undefined';
      } else {
        this.managedArea_.value = this.stringifyJSON_(properties);
        this.userArea_.value = this.stringifyJSON_(
            this.extractUserSettings_(properties));
      }
    },

    updateState_: function(properties) {
      this.stateArea_.value = this.stringifyJSON_(properties);
    },

    extractUserSettings_: function(properties) {
      if ('UserSetting' in properties)
        return properties['UserSetting'];

      if ('SharedSetting' in properties)
        return properties['SharedSetting'];

      var result = {};
      for (var fieldName in properties) {
        var entry = properties[fieldName];
        if (typeof entry === 'object') {
          var nestedResult = this.extractUserSettings_(entry);
          if (nestedResult)
            result[fieldName] = nestedResult;
        }
      }
      if (Object.keys(result).length)
        return result;
      else
        return undefined;
    },

    updateDom_: function() {
      var div = document.createElement('div');

      this.activeArea_ = function() {
        var label = document.createElement('h4');
        label.textContent = 'Active Settings (getProperties)';
        div.appendChild(label);
        var area = document.createElement('textarea');
        div.appendChild(area);
        return area;
      }();

      this.userArea_ = function() {
        var label = document.createElement('h4');
        label.textContent = 'User Settings';
        div.appendChild(label);
        var area = document.createElement('textarea');
        div.appendChild(area);
        return area;
      }();

      this.managedArea_ = function() {
        var label = document.createElement('h4');
        label.textContent = 'Managed Settings (getManagedProperties)';
        div.appendChild(label);
        var area = document.createElement('textarea');
        div.appendChild(area);
        return area;
      }();

      this.stateArea_ = function() {
        var label = document.createElement('h4');
        label.textContent = 'State (getState)';
        div.appendChild(label);
        var area = document.createElement('textarea');
        div.appendChild(area);
        return area;
      }();

      this.appendChild(div);
    },

    get userSettings() {
      return JSON.parse(this.userArea_.value);
    },

    get networkId() {
      return this.networkId_;
    }
  };

  return {
    NetworkConfig: NetworkConfig
  };
});
