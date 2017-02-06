Polymer({
    is: 'paper-textarea',

    behaviors: [
      Polymer.PaperInputBehavior,
      Polymer.IronFormElementBehavior
    ],

    properties: {
      _ariaLabelledBy: {
        observer: '_ariaLabelledByChanged',
        type: String
      },

      _ariaDescribedBy: {
        observer: '_ariaDescribedByChanged',
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
        value: 1
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
       value: 0
      }
    },

    _ariaLabelledByChanged: function(ariaLabelledBy) {
      this.$.input.textarea.setAttribute('aria-labelledby', ariaLabelledBy);
    },

    _ariaDescribedByChanged: function(ariaDescribedBy) {
      this.$.input.textarea.setAttribute('aria-describedby', ariaDescribedBy);
    },

    get _focusableElement() {
      return this.$.input.textarea;
    },
  });