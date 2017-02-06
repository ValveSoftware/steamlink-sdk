(function() {
      'use strict';

      Polymer({
        is: 'paper-dropdown-menu',

        behaviors: [
          Polymer.IronButtonState,
          Polymer.IronControlState,
          Polymer.IronFormElementBehavior,
          Polymer.IronValidatableBehavior
        ],

        properties: {
          /**
           * The derived "label" of the currently selected item. This value
           * is the `label` property on the selected item if set, or else the
           * trimmed text content of the selected item.
           */
          selectedItemLabel: {
            type: String,
            notify: true,
            readOnly: true
          },

          /**
           * The last selected item. An item is selected if the dropdown menu has
           * a child with class `dropdown-content`, and that child triggers an
           * `iron-select` event with the selected `item` in the `detail`.
           *
           * @type {?Object}
           */
          selectedItem: {
            type: Object,
            notify: true,
            readOnly: true
          },

          /**
           * The value for this element that will be used when submitting in
           * a form. It is read only, and will always have the same value
           * as `selectedItemLabel`.
           */
          value: {
            type: String,
            notify: true,
            readOnly: true
          },

          /**
           * The label for the dropdown.
           */
          label: {
            type: String
          },

          /**
           * The placeholder for the dropdown.
           */
          placeholder: {
            type: String
          },

          /**
           * The error message to display when invalid.
           */
          errorMessage: {
              type: String
          },

          /**
           * True if the dropdown is open. Otherwise, false.
           */
          opened: {
            type: Boolean,
            notify: true,
            value: false,
            observer: '_openedChanged'
          },

          /**
           * Set to true to disable the floating label. Bind this to the
           * `<paper-input-container>`'s `noLabelFloat` property.
           */
          noLabelFloat: {
              type: Boolean,
              value: false,
              reflectToAttribute: true
          },

          /**
           * Set to true to always float the label. Bind this to the
           * `<paper-input-container>`'s `alwaysFloatLabel` property.
           */
          alwaysFloatLabel: {
            type: Boolean,
            value: false
          },

          /**
           * Set to true to disable animations when opening and closing the
           * dropdown.
           */
          noAnimations: {
            type: Boolean,
            value: false
          },

          /**
           * The orientation against which to align the menu dropdown
           * horizontally relative to the dropdown trigger.
           */
          horizontalAlign: {
            type: String,
            value: 'right'
          },

          /**
           * The orientation against which to align the menu dropdown
           * vertically relative to the dropdown trigger.
           */
          verticalAlign: {
            type: String,
            value: 'top'
          }
        },

        listeners: {
          'tap': '_onTap'
        },

        keyBindings: {
          'up down': 'open',
          'esc': 'close'
        },

        hostAttributes: {
          role: 'combobox',
          'aria-autocomplete': 'none',
          'aria-haspopup': 'true'
        },

        observers: [
          '_selectedItemChanged(selectedItem)'
        ],

        attached: function() {
          // NOTE(cdata): Due to timing, a preselected value in a `IronSelectable`
          // child will cause an `iron-select` event to fire while the element is
          // still in a `DocumentFragment`. This has the effect of causing
          // handlers not to fire. So, we double check this value on attached:
          var contentElement = this.contentElement;
          if (contentElement && contentElement.selectedItem) {
            this._setSelectedItem(contentElement.selectedItem);
          }
        },

        /**
         * The content element that is contained by the dropdown menu, if any.
         */
        get contentElement() {
          return Polymer.dom(this.$.content).getDistributedNodes()[0];
        },

        /**
         * Show the dropdown content.
         */
        open: function() {
          this.$.menuButton.open();
        },

        /**
         * Hide the dropdown content.
         */
        close: function() {
          this.$.menuButton.close();
        },

        /**
         * A handler that is called when `iron-select` is fired.
         *
         * @param {CustomEvent} event An `iron-select` event.
         */
        _onIronSelect: function(event) {
          this._setSelectedItem(event.detail.item);
        },

        /**
         * A handler that is called when `iron-deselect` is fired.
         *
         * @param {CustomEvent} event An `iron-deselect` event.
         */
        _onIronDeselect: function(event) {
          this._setSelectedItem(null);
        },

        /**
         * A handler that is called when the dropdown is tapped.
         *
         * @param {CustomEvent} event A tap event.
         */
        _onTap: function(event) {
          if (Polymer.Gestures.findOriginalTarget(event) === this) {
            this.open();
          }
        },

        /**
         * Compute the label for the dropdown given a selected item.
         *
         * @param {Element} selectedItem A selected Element item, with an
         * optional `label` property.
         */
        _selectedItemChanged: function(selectedItem) {
          var value = '';
          if (!selectedItem) {
            value = '';
          } else {
            value = selectedItem.label || selectedItem.textContent.trim();
          }

          this._setValue(value);
          this._setSelectedItemLabel(value);
        },

        /**
         * Compute the vertical offset of the menu based on the value of
         * `noLabelFloat`.
         *
         * @param {boolean} noLabelFloat True if the label should not float
         * above the input, otherwise false.
         */
        _computeMenuVerticalOffset: function(noLabelFloat) {
          // NOTE(cdata): These numbers are somewhat magical because they are
          // derived from the metrics of elements internal to `paper-input`'s
          // template. The metrics will change depending on whether or not the
          // input has a floating label.
          return noLabelFloat ? -4 : 8;
        },

        /**
         * Returns false if the element is required and does not have a selection,
         * and true otherwise.
         * @param {*=} _value Ignored.
         * @return {boolean} true if `required` is false, or if `required` is true
         * and the element has a valid selection.
         */
        _getValidity: function(_value) {
          return this.disabled || !this.required || (this.required && !!this.value);
        },

        _openedChanged: function() {
          var openState = this.opened ? 'true' : 'false';
          var e = this.contentElement;
          if (e) {
            e.setAttribute('aria-expanded', openState);
          }
        }
      });
    })();