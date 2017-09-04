(function() {

    Polymer({

      is: 'paper-submenu',

      properties: {
        /**
         * Fired when the submenu is opened.
         *
         * @event paper-submenu-open
         */

        /**
         * Fired when the submenu is closed.
         *
         * @event paper-submenu-close
         */

        /**
         * Set opened to true to show the collapse element and to false to hide it.
         *
         * @attribute opened
         */
        opened: {
          type: Boolean,
          value: false,
          notify: true,
          observer: '_openedChanged'
        }
      },

      behaviors: [
        Polymer.IronControlState
      ],

      listeners: {
        'focus': '_onFocus'
      },

      get __parent() {
        return Polymer.dom(this).parentNode;
      },

      get __trigger() {
        return Polymer.dom(this.$.trigger).getDistributedNodes()[0];
      },

      get __content() {
        return Polymer.dom(this.$.content).getDistributedNodes()[0];
      },

      attached: function() {
        this.listen(this.__parent, 'iron-activate', '_onParentIronActivate');
      },

      dettached: function() {
        this.unlisten(this.__parent, 'iron-activate', '_onParentIronActivate');
      },

      /**
       * Expand the submenu content.
       */
      open: function() {
        if (!this.disabled && !this._active) {
          this.$.collapse.show();
          this._active = true;
          this.__trigger && this.__trigger.classList.add('iron-selected');
          this.__content && this.__content.focus();
        }
      },

      /**
       * Collapse the submenu content.
       */
      close: function() {
        if (this._active) {
          this.$.collapse.hide();
          this._active = false;
          this.__trigger && this.__trigger.classList.remove('iron-selected');
        }
      },

      /**
       * Toggle the submenu.
       */
      toggle: function() {
        if (this._active) {
          this.close();
        } else {
          this.open();
        }
      },

      /**
       * A handler that is called when the trigger is tapped.
       */
      _onTap: function(e) {
        if (!this.disabled) {
          this.toggle();
        }
      },

      /**
       * Toggles the submenu content when the trigger is tapped.
       */
      _openedChanged: function(opened, oldOpened) {
        if (opened) {
          this.fire('paper-submenu-open');
        } else if (oldOpened != null) {
          this.fire('paper-submenu-close');
        }
      },

      /**
       * A handler that is called when `iron-activate` is fired.
       *
       * @param {CustomEvent} event An `iron-activate` event.
       */
      _onParentIronActivate: function(event) {
        var parent = this.__parent;
        if (Polymer.dom(event).localTarget === parent) {
          // The activated item can either be this submenu, in which case it
          // should be expanded, or any of the other sibling submenus, in which
          // case this submenu should be collapsed.
          if (event.detail.item !== this && !parent.multi) {
            this.close();
          }
        }
      },

      /**
       * If the dropdown is open when disabled becomes true, close the
       * dropdown.
       *
       * @param {boolean} disabled True if disabled, otherwise false.
       */
      _disabledChanged: function(disabled) {
        Polymer.IronControlState._disabledChanged.apply(this, arguments);
        if (disabled && this._active) {
          this.close();
        }
      },

      /**
       * Handler that is called when the menu receives focus.
       *
       * @param {FocusEvent} event A focus event.
       */
      _onFocus: function(event) {
        this.__trigger && this.__trigger.focus();
      }

    });

  })();