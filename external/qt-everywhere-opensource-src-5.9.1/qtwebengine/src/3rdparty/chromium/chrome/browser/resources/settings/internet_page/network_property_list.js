// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of network properties
 * in a list. This also supports editing fields inline for fields listed in
 * editFieldTypes.
 */
Polymer({
  is: 'network-property-list',

  behaviors: [I18nBehavior, CrPolicyNetworkBehavior],

  properties: {
    /**
     * The dictionary containing the properties to display.
     * @type {!Object|undefined}
     */
    propertyDict: {type: Object},

    /**
     * Fields to display.
     * @type {!Array<string>}
     */
    fields: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Edit type of editable fields. May contain a property for any field in
     * |fields|. Other properties will be ignored. Property values can be:
     *   'String' - A text input will be displayed.
     *   TODO(stevenjb): Support types with custom validation, e.g. IPAddress.
     *   TODO(stevenjb): Support 'Number'.
     * When a field changes, the 'property-change' event will be fired with
     * the field name and the new value provided in the event detail.
     */
    editFieldTypes: {
      type: Object,
      value: function() {
        return {};
      },
    },

    /** Prefix used to look up property key translations. */
    prefix: {
      type: String,
      value: '',
    },
  },

  /**
   * Event triggered when an input field changes. Fires a 'property-change'
   * event with the field (property) name set to the target id, and the value
   * set to the target input value.
   * @param {Event} event The input change event.
   * @private
   */
  onValueChange_: function(event) {
    if (!this.propertyDict)
      return;
    var field = event.target.id;
    var curValue = this.get(field, this.propertyDict);
    if (typeof curValue == 'object') {
      // Extract the property from an ONC managed dictionary.
      curValue = CrOnc.getActiveValue(
          /** @type {!CrOnc.ManagedProperty} */ (curValue));
    }
    var newValue = event.target.value;
    if (newValue == curValue)
      return;
    this.fire('property-change', {field: field, value: newValue});
  },

  /**
   * @param {string} key The property key.
   * @return {string} The text to display for the property label.
   * @private
   */
  getPropertyLabel_: function(key) {
    var oncKey = 'Onc' + this.prefix + key;
    oncKey = oncKey.replace(/\./g, '-');
    if (this.i18nExists(oncKey))
      return this.i18n(oncKey);
    // We do not provide translations for every possible network property key.
    // For keys specific to a type, strip the type prefix.
    var result = this.prefix + key;
    for (let entry in chrome.networkingPrivate.NetworkType) {
      let type = chrome.networkingPrivate.NetworkType[entry];
      if (result.startsWith(type + '.')) {
        result = result.substr(type.length + 1);
        break;
      }
    }
    return result;
  },

  /**
   * @param {string} key The property key.
   * @return {boolean} Whether or not the property exists in |propertyDict|.
   * @private
   */
  hasPropertyValue_: function(key) {
    var value = this.get(key, this.propertyDict);
    return value !== undefined && value !== '';
  },

  /**
   * Generates a filter function dependent on propertyDict and editFieldTypes.
   * @private
   */
  computeFilter_: function() {
    return function(key) {
      return this.showProperty_(key);
    }.bind(this);
  },

  /**
   * @param {string} key The property key.
   * @return {boolean} Whether or not to show the property. Editable properties
   *     are always shown.
   * @private
   */
  showProperty_: function(key) {
    if (this.editFieldTypes.hasOwnProperty(key))
      return true;
    return this.hasPropertyValue_(key);
  },

  /**
   * @param {string} key The property key.
   * @param {string} type The field type.
   * @return {boolean}
   * @private
   */
  isEditable_: function(key, type) {
    var property = /** @type {!CrOnc.ManagedProperty|undefined} */ (
        this.get(key, this.propertyDict));
    if (this.isNetworkPolicyEnforced(property))
      return false;
    var editType = this.editFieldTypes[key];
    return editType !== undefined && (type == '' || editType == type);
  },

  /**
   * @param {string} key The property key.
   * @return {*} The managed property dictionary associated with |key|.
   * @private
   */
  getProperty_: function(key) {
    return this.get(key, this.propertyDict);
  },

  /**
   * @param {string} key The property key.
   * @return {string} The text to display for the property value.
   * @private
   */
  getPropertyValue_: function(key) {
    var value = this.get(key, this.propertyDict);
    if (value === undefined)
      return '';
    if (typeof value == 'object') {
      // Extract the property from an ONC managed dictionary
      value =
          CrOnc.getActiveValue(/** @type {!CrOnc.ManagedProperty} */ (value));
    }
    if (typeof value == 'number' || typeof value == 'boolean')
      return value.toString();
    assert(typeof value == 'string');
    let valueStr = /** @type {string} */ (value);
    var oncKey = 'Onc' + this.prefix + key;
    oncKey = oncKey.replace(/\./g, '-');
    oncKey += '_' + valueStr;
    if (this.i18nExists(oncKey))
      return this.i18n(oncKey);
    return valueStr;
  },
});
