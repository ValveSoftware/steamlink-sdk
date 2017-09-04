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
  function updateDisabledState(el, reason, disabled) {
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

    if (disabled)
      el.disabledReasons[reason] = true;
    else
      delete el.disabledReasons[reason];

    el.disabled = Object.keys(el.disabledReasons).length > 0;
  }

  /////////////////////////////////////////////////////////////////////////////
  // PrefInputElement class:

  /**
   * Define a constructor that uses an input element as its underlying element.
   * @constructor
   * @extends {HTMLInputElement}
   */
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
      this.addEventListener('change', this.handleChange.bind(this));

      // Listen for pref changes.
      Preferences.getInstance().addEventListener(this.pref, function(event) {
        if (event.value.uncommitted && !self.dialogPref)
          return;
        self.updateStateFromPref(event);
        updateDisabledState(self, 'notUserModifiable', event.value.disabled);
        self.controlledBy = event.value.controlledBy;
      });
    },

    /**
     * Handle changes to the input element's state made by the user. If a custom
     * change handler does not suppress it, a default handler is invoked that
     * updates the associated pref.
     * @param {Event} event Change event.
     * @protected
     */
    handleChange: function(event) {
      if (!this.customChangeHandler(event))
        this.updatePrefFromState();
    },

    /**
     * Handles changes to the pref. If a custom change handler does not suppress
     * it, a default handler is invoked that updates the input element's state.
     * @param {Event} event Pref change event.
     * @protected
     */
    updateStateFromPref: function(event) {
      if (!this.customPrefChangeHandler(event))
        this.value = event.value.value;
    },

    /**
     * An abstract method for all subclasses to override to update their
     * preference from existing state.
     * @protected
     */
    updatePrefFromState: assertNotReached,

    /**
     * See |updateDisabledState| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState(this, reason, disabled);
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

    /**
     * Custom change handler that is invoked first when the preference
     * associated with the input element changes. If it returns false, a default
     * handler is invoked next that updates the input element. If it returns
     * true, the default handler is suppressed.
     * @param {Event} event Input element change event.
     */
    customPrefChangeHandler: function(event) {
      return false;
    },
  };

  /**
   * The name of the associated preference.
   */
  cr.defineProperty(PrefInputElement, 'pref', cr.PropertyKind.ATTR);

  /**
   * The data type of the associated preference, only relevant for derived
   * classes that support different data types.
   */
  cr.defineProperty(PrefInputElement, 'dataType', cr.PropertyKind.ATTR);

  /**
   * Whether this input element is part of a dialog. If so, changes take effect
   * in the settings UI immediately but are only actually committed when the
   * user confirms the dialog. If the user cancels the dialog instead, the
   * changes are rolled back in the settings UI and never committed.
   */
  cr.defineProperty(PrefInputElement, 'dialogPref', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the associated preference is controlled by a source other than the
   * user's setting (can be 'policy', 'extension', 'recommended' or unset).
   */
  cr.defineProperty(PrefInputElement, 'controlledBy', cr.PropertyKind.ATTR);

  /**
   * The user metric string.
   */
  cr.defineProperty(PrefInputElement, 'metric', cr.PropertyKind.ATTR);

  /////////////////////////////////////////////////////////////////////////////
  // PrefCheckbox class:

  /**
   * Define a constructor that uses an input element as its underlying element.
   * @constructor
   * @extends {options.PrefInputElement}
   */
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
        this.updatePrefFromState();
    },

    /**
     * Update the associated pref when when the user makes changes to the
     * checkbox state.
     * @override
     */
    updatePrefFromState: function() {
      var value = this.inverted_pref ? !this.checked : this.checked;
      Preferences.setBooleanPref(this.pref, value,
                                 !this.dialogPref, this.metric);
    },

    /** @override */
    updateStateFromPref: function(event) {
      if (!this.customPrefChangeHandler(event))
        this.defaultPrefChangeHandler(event);
    },

    /**
     * @param {Event} event A pref change event.
     */
    defaultPrefChangeHandler: function(event) {
      var value = Boolean(event.value.value);
      this.checked = this.inverted_pref ? !value : value;
    },
  };

  /**
   * Whether the mapping between checkbox state and associated pref is inverted.
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
     * Update the associated pref when the user inputs a number.
     * @override
     */
    updatePrefFromState: function() {
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
     * @override
     */
    updatePrefFromState: function() {
      if (this.value == 'true' || this.value == 'false') {
        Preferences.setBooleanPref(this.pref,
                                   this.value == String(this.checked),
                                   !this.dialogPref, this.metric);
      } else {
        Preferences.setIntegerPref(this.pref, this.value,
                                   !this.dialogPref, this.metric);
      }
    },

    /** @override */
    updateStateFromPref: function(event) {
      if (!this.customPrefChangeHandler(event))
        this.checked = this.value == String(event.value.value);
    },
  };

  /////////////////////////////////////////////////////////////////////////////
  // PrefRange class:

  /**
   * Define a constructor that uses an input element as its underlying element.
   * @constructor
   * @extends {options.PrefInputElement}
   */
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
      this.addEventListener('touchcancel', this.handleRelease_.bind(this));
      this.addEventListener('touchend', this.handleRelease_.bind(this));
    },

    /**
     * Update the associated pref when when the user releases the slider.
     * @override
     */
    updatePrefFromState: function() {
      Preferences.setIntegerPref(
          this.pref,
          this.mapPositionToPref(parseInt(this.value, 10)),
          !this.dialogPref,
          this.metric);
    },

    /** @override */
    handleChange: function() {
      // Ignore changes to the slider position made by the user while the slider
      // has not been released.
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
        this.updatePrefFromState();
    },

    /**
     * Handles changes to the pref associated with the slider. If a custom
     * change handler does not suppress it, a default handler is invoked that
     * updates the slider position.
     * @override.
     */
    updateStateFromPref: function(event) {
      if (this.customPrefChangeHandler(event))
        return;
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
    __proto__: HTMLSelectElement.prototype,

    /** @override */
    decorate: PrefInputElement.prototype.decorate,

    /** @override */
    handleChange: PrefInputElement.prototype.handleChange,

    /**
     * Update the associated pref when when the user selects an item.
     * @override
     */
    updatePrefFromState: function() {
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

    /** @override */
    updateStateFromPref: function(event) {
      if (this.customPrefChangeHandler(event))
        return;

      // Make sure the value is a string, because the value is stored as a
      // string in the HTMLOptionElement.
      var value = String(event.value.value);

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

    /** @override */
    setDisabled: PrefInputElement.prototype.setDisabled,

    /** @override */
    customChangeHandler: PrefInputElement.prototype.customChangeHandler,

    /** @override */
    customPrefChangeHandler: PrefInputElement.prototype.customPrefChangeHandler,
  };

  /**
   * The name of the associated preference.
   */
  cr.defineProperty(PrefSelect, 'pref', cr.PropertyKind.ATTR);

  /**
   * The data type of the associated preference, only relevant for derived
   * classes that support different data types.
   */
  cr.defineProperty(PrefSelect, 'dataType', cr.PropertyKind.ATTR);

  /**
   * Whether this input element is part of a dialog. If so, changes take effect
   * in the settings UI immediately but are only actually committed when the
   * user confirms the dialog. If the user cancels the dialog instead, the
   * changes are rolled back in the settings UI and never committed.
   */
  cr.defineProperty(PrefSelect, 'dialogPref', cr.PropertyKind.BOOL_ATTR);

  /**
   * Whether the associated preference is controlled by a source other than the
   * user's setting (can be 'policy', 'extension', 'recommended' or unset).
   */
  cr.defineProperty(PrefSelect, 'controlledBy', cr.PropertyKind.ATTR);

  /**
   * The user metric string.
   */
  cr.defineProperty(PrefSelect, 'metric', cr.PropertyKind.ATTR);

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
     * @override
     */
    updatePrefFromState: function(event) {
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
        updateDisabledState(self, 'notUserModifiable',
                            event.value.disabled && !event.value.value);
        self.controlledBy = event.value.controlledBy;
      });
    },

    /**
     * See |updateDisabledState| above.
     */
    setDisabled: function(reason, disabled) {
      updateDisabledState(this, reason, disabled);
    },
  };

  /**
   * The name of the associated preference.
   */
  cr.defineProperty(PrefButton, 'pref', cr.PropertyKind.ATTR);

  /**
   * Whether the associated preference is controlled by a source other than the
   * user's setting (can be 'policy', 'extension', 'recommended' or unset).
   */
  cr.defineProperty(PrefButton, 'controlledBy', cr.PropertyKind.ATTR);

  // Export
  return {
    PrefCheckbox: PrefCheckbox,
    PrefInputElement: PrefInputElement,
    PrefNumber: PrefNumber,
    PrefRadio: PrefRadio,
    PrefRange: PrefRange,
    PrefSelect: PrefSelect,
    PrefTextField: PrefTextField,
    PrefPortNumber: PrefPortNumber,
    PrefButton: PrefButton
  };
});
