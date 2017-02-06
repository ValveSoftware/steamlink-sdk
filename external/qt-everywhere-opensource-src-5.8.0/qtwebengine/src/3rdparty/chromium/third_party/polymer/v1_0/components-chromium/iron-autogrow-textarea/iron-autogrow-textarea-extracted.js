Polymer({

    is: 'iron-autogrow-textarea',

    behaviors: [
      Polymer.IronFormElementBehavior,
      Polymer.IronValidatableBehavior,
      Polymer.IronControlState
    ],

    properties: {

      /**
       * Use this property instead of `value` for two-way data binding.
       * This property will be deprecated in the future. Use `value` instead.
       * @type {string|number}
       */
      bindValue: {
        observer: '_bindValueChanged',
        type: String
      },

      /**
       * The initial number of rows.
       *
       * @attribute rows
       * @type number
       * @default 1
       */
      rows: {
        type: Number,
        value: 1,
        observer: '_updateCached'
      },

      /**
       * The maximum number of rows this element can grow to until it
       * scrolls. 0 means no maximum.
       *
       * @attribute maxRows
       * @type number
       * @default 0
       */
      maxRows: {
       type: Number,
       value: 0,
       observer: '_updateCached'
      },

      /**
       * Bound to the textarea's `autocomplete` attribute.
       */
      autocomplete: {
        type: String,
        value: 'off'
      },

      /**
       * Bound to the textarea's `autofocus` attribute.
       */
      autofocus: {
        type: Boolean,
        value: false
      },

      /**
       * Bound to the textarea's `inputmode` attribute.
       */
      inputmode: {
        type: String
      },

      /**
       * Bound to the textarea's `placeholder` attribute.
       */
      placeholder: {
        type: String
      },

      /**
       * Bound to the textarea's `readonly` attribute.
       */
      readonly: {
        type: String
      },

      /**
       * Set to true to mark the textarea as required.
       */
      required: {
        type: Boolean
      },

      /**
       * The maximum length of the input value.
       */
      maxlength: {
        type: Number
      }

    },

    listeners: {
      'input': '_onInput'
    },

    observers: [
      '_onValueChanged(value)'
    ],

    /**
     * Returns the underlying textarea.
     * @type HTMLTextAreaElement
     */
    get textarea() {
      return this.$.textarea;
    },

    /**
     * Returns textarea's selection start.
     * @type Number
     */
    get selectionStart() {
      return this.$.textarea.selectionStart;
    },

    /**
     * Returns textarea's selection end.
     * @type Number
     */
    get selectionEnd() {
      return this.$.textarea.selectionEnd;
    },

    /**
     * Sets the textarea's selection start.
     */
    set selectionStart(value) {
      this.$.textarea.selectionStart = value;
    },

    /**
     * Sets the textarea's selection end.
     */
    set selectionEnd(value) {
      this.$.textarea.selectionEnd = value;
    },

    /**
     * Returns true if `value` is valid. The validator provided in `validator`
     * will be used first, if it exists; otherwise, the `textarea`'s validity
     * is used.
     * @return {boolean} True if the value is valid.
     */
    validate: function() {
      // Empty, non-required input is valid.
      if (!this.required && this.value == '') {
        this.invalid = false;
        return true;
      }

      var valid;
      if (this.hasValidator()) {
        valid = Polymer.IronValidatableBehavior.validate.call(this, this.value);
      } else {
        valid = this.$.textarea.validity.valid;
        this.invalid = !valid;
      }
      this.fire('iron-input-validate');
      return valid;
    },

    _bindValueChanged: function() {
      var textarea = this.textarea;
      if (!textarea) {
        return;
      }

      // If the bindValue changed manually, then we need to also update
      // the underlying textarea's value. Otherwise this change was probably
      // generated from the _onInput handler, and the two values are already
      // the same.
      if (textarea.value !== this.bindValue) {
        textarea.value = !(this.bindValue || this.bindValue === 0) ? '' : this.bindValue;
      }

      this.value = this.bindValue;
      this.$.mirror.innerHTML = this._valueForMirror();
      // manually notify because we don't want to notify until after setting value
      this.fire('bind-value-changed', {value: this.bindValue});
    },

    _onInput: function(event) {
      this.bindValue = event.path ? event.path[0].value : event.target.value;
    },

    _constrain: function(tokens) {
      var _tokens;
      tokens = tokens || [''];
      // Enforce the min and max heights for a multiline input to avoid measurement
      if (this.maxRows > 0 && tokens.length > this.maxRows) {
        _tokens = tokens.slice(0, this.maxRows);
      } else {
        _tokens = tokens.slice(0);
      }
      while (this.rows > 0 && _tokens.length < this.rows) {
        _tokens.push('');
      }
      // Use &#160; instead &nbsp; of to allow this element to be used in XHTML.
      return _tokens.join('<br/>') + '&#160;';
    },

    _valueForMirror: function() {
      var input = this.textarea;
      if (!input) {
        return;
      }
      this.tokens = (input && input.value) ? input.value.replace(/&/gm, '&amp;').replace(/"/gm, '&quot;').replace(/'/gm, '&#39;').replace(/</gm, '&lt;').replace(/>/gm, '&gt;').split('\n') : [''];
      return this._constrain(this.tokens);
    },

    _updateCached: function() {
      this.$.mirror.innerHTML = this._constrain(this.tokens);
    },

    _onValueChanged: function() {
      this.bindValue = this.value;
    }
  });