Polymer({
      is: 'app-header-layout',

      behaviors: [
        Polymer.IronResizableBehavior
      ],

      properties: {
        /**
         * If true, the current element will have its own scrolling region.
         * Otherwise, it will use the document scroll to control the header.
         */
        hasScrollingRegion: {
          type: Boolean,
          value: false,
          reflectToAttribute: true
        }
      },

      listeners: {
        'iron-resize': '_resizeHandler',
        'app-header-reset-layout': 'resetLayout'
      },

      observers: [
        'resetLayout(isAttached, hasScrollingRegion)'
      ],

      /**
       * A reference to the app-header element.
       *
       * @property header
       */
      get header() {
        return Polymer.dom(this.$.header).getDistributedNodes()[0];
      },

      /**
       * Resets the layout. This method is automatically called when the element is attached
       * to the DOM.
       *
       * @method resetLayout
       */
      resetLayout: function() {
        this._updateScroller();
        this.debounce('_resetLayout', this._updateContentPosition);
      },

      _updateContentPosition: function() {
        var header = this.header;
        if (!this.isAttached || !header) {
          return;
        }

        // Get header height here so that style reads are batched together before style writes
        // (i.e. getBoundingClientRect() below).
        var headerHeight = header.offsetHeight;

        // Update the header position.
        if (!this.hasScrollingRegion) {
          var rect = this.getBoundingClientRect();
          var rightOffset = document.documentElement.clientWidth - rect.right;
          header.style.left = rect.left + 'px';
          header.style.right = rightOffset + 'px';
        } else {
          header.style.left = '';
          header.style.right = '';
        }

        // Update the content container position.
        var containerStyle = this.$.contentContainer.style;
        if (header.fixed && !header.willCondense() && this.hasScrollingRegion) {
          // If the header size does not change and we're using a scrolling region, exclude
          // the header area from the scrolling region so that the header doesn't overlap
          // the scrollbar.
          containerStyle.marginTop = headerHeight + 'px';
          containerStyle.paddingTop = '';
        } else {
          containerStyle.paddingTop = headerHeight + 'px';
          containerStyle.marginTop = '';
        }
      },

      _updateScroller: function() {
        if (!this.isAttached) {
          return;
        }
        var header = this.header;
        if (header) {
          header.scrollTarget = this.hasScrollingRegion ?
              this.$.contentContainer : this.ownerDocument.documentElement;
        }
      },

      _resizeHandler: function() {
        this.resetLayout();
      }
    });