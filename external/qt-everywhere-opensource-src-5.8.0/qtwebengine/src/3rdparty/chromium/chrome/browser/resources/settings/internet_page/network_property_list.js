// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of network properties
 * in a list in the format:
 *    Key1.........Value1
 *    KeyTwo.......ValueTwo
 * This also supports editing fields inline for fields listed in editFieldTypes:
 *    KeyThree....._________
 * TODO(stevenjb): Translate the keys and (where appropriate) values.
 */
Polymer({
  is: 'network-property-list',

  behaviors: [CrPolicyNetworkBehavior],

  properties: {
    /**
     * The dictionary containing the properties to display.
     * @type {!Object|undefined}
     */
    propertyDict: {
      type: Object
    },

    /**
     * Fields to display.
     * @type {!Array<string>}
     */
    fields: {
      type: Array,
      value: function() { return []; }
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
      value: function() { return {}; }
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
      curValue =
          CrOnc.getActiveValue(/** @type {!CrOnc.ManagedProperty} */(curValue));
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
    // TODO(stevenjb): Localize.
    return key;
  },

  /**
   * @param {!Object} propertyDict
   * @param {string} key The property key.
   * @return {boolean} Whether or not the property exists in |propertyDict|.
   * @private
   */
  hasPropertyValue_: function(propertyDict, key) {
    var value = this.get(key, propertyDict);
    return value !== undefined && value !== '';
  },

  /**
   * @param {!Object} propertyDict
   * @param {!Object} editFieldTypes The editFieldTypes object.
   * @param {string} key The property key.
   * @return {boolean} Whether or not to show the property. Editable properties
   *     are always shown.
   * @private
   */
  showProperty_: function(propertyDict, editFieldTypes, key) {
    if (editFieldTypes.hasOwnProperty(key))
      return true;
    return this.hasPropertyValue_(propertyDict, key);
  },

  /**
   * @param {!Object} propertyDict
   * @param {!Object} editFieldTypes The editFieldTypes object.
   * @param {string} key The property key.
   * @return {boolean} True if |key| exists in |propertiesDict| and is not
   *     editable.
   * @private
   */
  showNoEdit_: function(propertyDict, editFieldTypes, key) {
    if (!this.hasPropertyValue_(propertyDict, key))
      return false;
    var property = /** @type {!CrOnc.ManagedProperty|undefined} */(
      this.get(key, propertyDict));
    if (this.isNetworkPolicyEnforced(property))
      return true;
    return !editFieldTypes[key];
  },

  /**
   * @param {!Object} propertyDict
   * @param {!Object} editFieldTypes The editFieldTypes object.
   * @param {string} key The property key.
   * @param {string} type The field type.
   * @return {boolean} True if |key| exists in |propertyDict| and is of editable
   *     type |type|.
   * @private
   */
  showEdit_: function(propertyDict, editFieldTypes, key, type) {
    if (!this.hasPropertyValue_(propertyDict, key))
      return false;
    var property = /** @type {!CrOnc.ManagedProperty|undefined} */(
        this.get(key, propertyDict));
    if (this.isNetworkPolicyEnforced(property))
      return false;
    return editFieldTypes[key] == type;
  },

  /**
   * @param {!Object} propertyDict
   * @param {string} key The property key.
   * @return {string} The text to display for the property value.
   * @private
   */
  getPropertyValue_: function(propertyDict, key) {
    var value = this.get(key, propertyDict);
    if (value === undefined)
      return '';
    if (typeof value == 'object') {
      // Extract the property from an ONC managed dictionary
      value =
          CrOnc.getActiveValue(/** @type {!CrOnc.ManagedProperty} */(value));
    }
    // TODO(stevenjb): Localize.
    if (typeof value == 'number' || typeof value == 'boolean')
      return value.toString();
    return /** @type {string} */(value);
  },
});
