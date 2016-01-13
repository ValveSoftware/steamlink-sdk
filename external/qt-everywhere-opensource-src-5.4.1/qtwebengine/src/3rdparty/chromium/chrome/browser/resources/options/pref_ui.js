// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var Preferences = options.Preferences;

  /**
   * Allows an element to be disabled for several reasons.
   * The element is disabled if at least one reason is true, and the reasons
   * can be set separately.
   * @private
   * @param {!HTMLElement} el The element to update.
   * @param {string} reason The reason for disabling the element.
   * @param {boolean} disabled Whether the element should be disabled or enabled
   * for the given |reason|.
   */
  function updateDisabledState_(el, reason, disabled) {
    if (!el.disabledReasons)
      el.disabledReasons = {};
    if (el.disabled && (Object.keys(el.disabledReasons).length == 0)) {
      // The element has been previously disabled without a reason, so we add
      // one to keep it disabled.
      el.disabledReasons.other = true;
    }
    if (!el.disabled) {
      // If the element is not disabled, there should be no reason, except for
      // 'other'.
      delete el.disabledReasons.other;
      if (Object.keys(el.disabledReasons).length > 0)
        console.error('Element is not disabled but should be');
    }
    if (disabled) {
      el.disabledReasons[reason] = true;
    } else {
      delete el.disabledReasons[reason];
    }
    el.disabled = Object.keys(el.disabledReasons).length > 0;
  }

  /////////////////////////////////////////////////////////////////////////////
  // PrefInputElement class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefInputElement = cr.ui.define('input');

  PrefInputElement.prototype = {
    // Set up the prototype chain
    __proto__: HTMLInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      var self = this;

      // Listen for user events.
      this.addEventListener('change', this.handleChange_.bind(this));

      // Listen for pref changes.
      Preferences.getInstance().addEventListener(this.pref, function(event) {
        if (event.value.uncommitted && !self.dialogPref)
          return;
        self.updateStateFromPref_(event);
        updateDisabledState_(self, 'notUserModifiable', event.value.disabled);
        self.controlledBy = event.value.controlledBy;
      });
    },

    /**
     * Handle changes to the input element's state made by the user. If a custom
     * change handler does not suppress it, a default handler is invoked that
     * updates the associated pref.
     * @param {Event} event Change event.
     * @private
     */
    handleChange_: function(event) {
      if (!this.customChangeHandler(event))
        this.updatePrefFromState_();
    },

    /**
     * Update the input element's state when the associated pref changes.
     * @param {Event} event Pref change event.
     * @private
     */
    updateStateFromPref_: function(event) {
      this.value = event.value.value;
    },

    /**
     * See |updateDisabledState_| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState_(this, reason, disabled);
    },

    /**
     * Custom change handler that is invoked first when the user makes changes
     * to the input element's state. If it returns false, a default handler is
     * invoked next that updates the associated pref. If it returns true, the
     * default handler is suppressed (i.e., this works like stopPropagation or
     * cancelBubble).
     * @param {Event} event Input element change event.
     */
    customChangeHandler: function(event) {
      return false;
    },
  };

  /**
   * The name of the associated preference.
   * @type {string}
   */
  cr.defineProperty(PrefInputElement, 'pref', cr.PropertyKind.ATTR);

  /**
   * The data type of the associated preference, only relevant for derived
   * classes that support different data types.
   * @type {string}
   */
  cr.defineProperty(PrefInputElement, 'dataType', cr.PropertyKind.ATTR);

  /**
   * Whether this input element is part of a dialog. If so, changes take effect
   * in the settings UI immediately but are only actually committed when the
   * user confirms the dialog. If the user cancels the dialog instead, the
   * changes are rolled back in the settings UI and never committed.
   * @type {boolean}
   */
  cr.defineProperty(PrefInputElement, 'dialogPref', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the associated preference is controlled by a source other than the
   * user's setting (can be 'policy', 'extension', 'recommended' or unset).
   * @type {string}
   */
  cr.defineProperty(PrefInputElement, 'controlledBy', cr.PropertyKind.ATTR);

  /**
   * The user metric string.
   * @type {string}
   */
  cr.defineProperty(PrefInputElement, 'metric', cr.PropertyKind.ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefCheckbox class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefCheckbox = cr.ui.define('input');

  PrefCheckbox.prototype = {
    // Set up the prototype chain
    __proto__: PrefInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      PrefInputElement.prototype.decorate.call(this);
      this.type = 'checkbox';

      // Consider a checked dialog checkbox as a 'suggestion' which is committed
      // once the user confirms the dialog.
      if (this.dialogPref && this.checked)
        this.updatePrefFromState_();
    },

    /**
     * Update the associated pref when when the user makes changes to the
     * checkbox state.
     * @private
     */
    updatePrefFromState_: function() {
      var value = this.inverted_pref ? !this.checked : this.checked;
      Preferences.setBooleanPref(this.pref, value,
                                 !this.dialogPref, this.metric);
    },

    /**
     * Update the checkbox state when the associated pref changes.
     * @param {Event} event Pref change event.
     * @private
     */
    updateStateFromPref_: function(event) {
      var value = Boolean(event.value.value);
      this.checked = this.inverted_pref ? !value : value;
    },
  };

  /**
   * Whether the mapping between checkbox state and associated pref is inverted.
   * @type {boolean}
   */
  cr.defineProperty(PrefCheckbox, 'inverted_pref', cr.PropertyKind.BOOL_ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefNumber class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefNumber = cr.ui.define('input');

  PrefNumber.prototype = {
    // Set up the prototype chain
    __proto__: PrefInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      PrefInputElement.prototype.decorate.call(this);
      this.type = 'number';
    },

    /**
     * Update the associated pref when when the user inputs a number.
     * @private
     */
    updatePrefFromState_: function() {
      if (this.validity.valid) {
        Preferences.setIntegerPref(this.pref, this.value,
                                   !this.dialogPref, this.metric);
      }
    },
  };

  /////////////////////////////////////////////////////////////////////////////
  // PrefRadio class:

  //Define a constructor that uses an input element as its underlying element.
  var PrefRadio = cr.ui.define('input');

  PrefRadio.prototype = {
    // Set up the prototype chain
    __proto__: PrefInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      PrefInputElement.prototype.decorate.call(this);
      this.type = 'radio';
    },

    /**
     * Update the associated pref when when the user selects the radio button.
     * @private
     */
    updatePrefFromState_: function() {
      if (this.value == 'true' || this.value == 'false') {
        Preferences.setBooleanPref(this.pref,
                                   this.value == String(this.checked),
                                   !this.dialogPref, this.metric);
      } else {
        Preferences.setIntegerPref(this.pref, this.value,
                                   !this.dialogPref, this.metric);
      }
    },

    /**
     * Update the radio button state when the associated pref changes.
     * @param {Event} event Pref change event.
     * @private
     */
    updateStateFromPref_: function(event) {
      this.checked = this.value == String(event.value.value);
    },
  };

  /////////////////////////////////////////////////////////////////////////////
  // PrefRange class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefRange = cr.ui.define('input');

  PrefRange.prototype = {
    // Set up the prototype chain
    __proto__: PrefInputElement.prototype,

    /**
     * The map from slider position to corresponding pref value.
     */
    valueMap: undefined,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      PrefInputElement.prototype.decorate.call(this);
      this.type = 'range';

      // Listen for user events.
      // TODO(jhawkins): Add onmousewheel handling once the associated WK bug is
      // fixed.
      // https://bugs.webkit.org/show_bug.cgi?id=52256
      this.addEventListener('keyup', this.handleRelease_.bind(this));
      this.addEventListener('mouseup', this.handleRelease_.bind(this));
    },

    /**
     * Update the associated pref when when the user releases the slider.
     * @private
     */
    updatePrefFromState_: function() {
      Preferences.setIntegerPref(this.pref, this.mapPositionToPref(this.value),
                                 !this.dialogPref, this.metric);
    },

    /**
     * Ignore changes to the slider position made by the user while the slider
     * has not been released.
     * @private
     */
    handleChange_: function() {
    },

    /**
     * Handle changes to the slider position made by the user when the slider is
     * released. If a custom change handler does not suppress it, a default
     * handler is invoked that updates the associated pref.
     * @param {Event} event Change event.
     * @private
     */
    handleRelease_: function(event) {
      if (!this.customChangeHandler(event))
        this.updatePrefFromState_();
    },

    /**
     * Update the slider position when the associated pref changes.
     * @param {Event} event Pref change event.
     * @private
     */
    updateStateFromPref_: function(event) {
      var value = event.value.value;
      this.value = this.valueMap ? this.valueMap.indexOf(value) : value;
    },

    /**
     * Map slider position to the range of values provided by the client,
     * represented by |valueMap|.
     * @param {number} position The slider position to map.
     */
    mapPositionToPref: function(position) {
      return this.valueMap ? this.valueMap[position] : position;
    },
  };

  /////////////////////////////////////////////////////////////////////////////
  // PrefSelect class:

  // Define a constructor that uses a select element as its underlying element.
  var PrefSelect = cr.ui.define('select');

  PrefSelect.prototype = {
    // Set up the prototype chain
    __proto__: PrefInputElement.prototype,

    /**
     * Update the associated pref when when the user selects an item.
     * @private
     */
    updatePrefFromState_: function() {
      var value = this.options[this.selectedIndex].value;
      switch (this.dataType) {
        case 'number':
          Preferences.setIntegerPref(this.pref, value,
                                     !this.dialogPref, this.metric);
          break;
        case 'double':
          Preferences.setDoublePref(this.pref, value,
                                    !this.dialogPref, this.metric);
          break;
        case 'boolean':
          Preferences.setBooleanPref(this.pref, value == 'true',
                                     !this.dialogPref, this.metric);
          break;
        case 'string':
          Preferences.setStringPref(this.pref, value,
                                    !this.dialogPref, this.metric);
          break;
        default:
          console.error('Unknown data type for <select> UI element: ' +
                        this.dataType);
      }
    },

    /**
     * Update the selected item when the associated pref changes.
     * @param {Event} event Pref change event.
     * @private
     */
    updateStateFromPref_: function(event) {
      // Make sure the value is a string, because the value is stored as a
      // string in the HTMLOptionElement.
      value = String(event.value.value);

      var found = false;
      for (var i = 0; i < this.options.length; i++) {
        if (this.options[i].value == value) {
          this.selectedIndex = i;
          found = true;
        }
      }

      // Item not found, select first item.
      if (!found)
        this.selectedIndex = 0;

      // The "onchange" event automatically fires when the user makes a manual
      // change. It should never be fired for a programmatic change. However,
      // these two lines were here already and it is hard to tell who may be
      // relying on them.
      if (this.onchange)
        this.onchange(event);
    },
  };

  /////////////////////////////////////////////////////////////////////////////
  // PrefTextField class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefTextField = cr.ui.define('input');

  PrefTextField.prototype = {
    // Set up the prototype chain
    __proto__: PrefInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      PrefInputElement.prototype.decorate.call(this);
      var self = this;

      // Listen for user events.
      window.addEventListener('unload', function() {
        if (document.activeElement == self)
          self.blur();
      });
    },

    /**
     * Update the associated pref when when the user inputs text.
     * @private
     */
    updatePrefFromState_: function(event) {
      switch (this.dataType) {
        case 'number':
          Preferences.setIntegerPref(this.pref, this.value,
                                     !this.dialogPref, this.metric);
          break;
        case 'double':
          Preferences.setDoublePref(this.pref, this.value,
                                    !this.dialogPref, this.metric);
          break;
        case 'url':
          Preferences.setURLPref(this.pref, this.value,
                                 !this.dialogPref, this.metric);
          break;
        default:
          Preferences.setStringPref(this.pref, this.value,
                                    !this.dialogPref, this.metric);
          break;
      }
    },
  };

  /////////////////////////////////////////////////////////////////////////////
  // PrefPortNumber class:

  // Define a constructor that uses an input element as its underlying element.
  var PrefPortNumber = cr.ui.define('input');

  PrefPortNumber.prototype = {
    // Set up the prototype chain
    __proto__: PrefTextField.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      var self = this;
      self.type = 'text';
      self.dataType = 'number';
      PrefTextField.prototype.decorate.call(this);
      self.oninput = function() {
        // Note that using <input type="number"> is insufficient to restrict
        // the input as it allows negative numbers and does not limit the
        // number of charactes typed even if a range is set.  Furthermore,
        // it sometimes produces strange repaint artifacts.
        var filtered = self.value.replace(/[^0-9]/g, '');
        if (filtered != self.value)
          self.value = filtered;
      };
    }
  };

  /////////////////////////////////////////////////////////////////////////////
  // PrefButton class:

  // Define a constructor that uses a button element as its underlying element.
  var PrefButton = cr.ui.define('button');

  PrefButton.prototype = {
    // Set up the prototype chain
    __proto__: HTMLButtonElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      var self = this;

      // Listen for pref changes.
      // This element behaves like a normal button and does not affect the
      // underlying preference; it just becomes disabled when the preference is
      // managed, and its value is false. This is useful for buttons that should
      // be disabled when the underlying Boolean preference is set to false by a
      // policy or extension.
      Preferences.getInstance().addEventListener(this.pref, function(event) {
        updateDisabledState_(self, 'notUserModifiable',
                             event.value.disabled && !event.value.value);
        self.controlledBy = event.value.controlledBy;
      });
    },

    /**
     * See |updateDisabledState_| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState_(this, reason, disabled);
    },
  };

  /**
   * The name of the associated preference.
   * @type {string}
   */
  cr.defineProperty(PrefButton, 'pref', cr.PropertyKind.ATTR);

  /**
   * Whether the associated preference is controlled by a source other than the
   * user's setting (can be 'policy', 'extension', 'recommended' or unset).
   * @type {string}
   */
  cr.defineProperty(PrefButton, 'controlledBy', cr.PropertyKind.ATTR);

  // Export
  return {
    PrefCheckbox: PrefCheckbox,
    PrefNumber: PrefNumber,
    PrefRadio: PrefRadio,
    PrefRange: PrefRange,
    PrefSelect: PrefSelect,
    PrefTextField: PrefTextField,
    PrefPortNumber: PrefPortNumber,
    PrefButton: PrefButton
  };

});
