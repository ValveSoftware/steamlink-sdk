Polymer({
      is: 'paper-icon-button-light',
      extends: 'button',

      behaviors: [
        Polymer.PaperRippleBehavior
      ],

      listeners: {
        'down': '_rippleDown',
        'up': '_rippleUp',
        'focus': '_rippleDown',
        'blur': '_rippleUp',
      },

      _rippleDown: function() {
        this.getRipple().downAction();
      },

      _rippleUp: function() {
        this.getRipple().upAction();
      },

      /**
       * @param {...*} var_args
       */
      ensureRipple: function(var_args) {
        var lastRipple = this._ripple;
        Polymer.PaperRippleBehavior.ensureRipple.apply(this, arguments);
        if (this._ripple && this._ripple !== lastRipple) {
          this._ripple.center = true;
          this._ripple.classList.add('circle');
        }
      }
    });